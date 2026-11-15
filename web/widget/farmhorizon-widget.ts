// FarmHorizon widget: embeds the WebAssembly field renderer in any container
// element and drives it from page data.
//
//   const widget = await FarmHorizonWidget.mount(el, { initialState: { cropType: "rice" } });
//   widget.setState({ ndvi: 0.72, harvestedPct: 40 });
//
// Size comes from the container (CSS, data-width / data-height, or options).
// The scene always fits the widget; there is no camera to drive.

export const SOIL_TYPES = ["sandy", "loam", "clay", "chernozem"] as const;
export const CROP_TYPES = ["wheat", "rice", "corn", "soybean"] as const;

export type SoilType = (typeof SOIL_TYPES)[number];
export type CropType = (typeof CROP_TYPES)[number];

/** Everything the scene shows; mirrors core::FieldState + core::ViewSettings. */
export interface FieldSceneState {
  fieldSize: number;    // cells per side
  soilType: SoilType;
  soilMoisture: number; // %
  cropType: CropType;
  ndvi: number;
  yieldExt: number;     // canopy height scale (renderer units)
  harvestedPct: number; // %
  timeOfDay: number;    // hours, sets the sun angle
}

type NumericKey = { [K in keyof FieldSceneState]: FieldSceneState[K] extends number ? K : never }[keyof FieldSceneState];

/** Valid ranges, matching the clamps in App::set_external_state / set_time_of_day. */
export const STATE_LIMITS: Readonly<Record<NumericKey, readonly [number, number]>> = {
  fieldSize: [12, 64],
  soilMoisture: [10, 60],
  ndvi: [0.1, 0.85],
  yieldExt: [10, 70],
  harvestedPct: [0, 100],
  timeOfDay: [6, 18],
};

export const DEFAULT_STATE: Readonly<FieldSceneState> = {
  fieldSize: 22,
  soilType: "loam",
  soilMoisture: 35,
  cropType: "wheat",
  ndvi: 0.65,
  yieldExt: 48,
  harvestedPct: 30,
  timeOfDay: 14.5,
};

export interface OverlayOptions {
  /** In-canvas raygui panel (debugging; edits there are not reported back). */
  showControls?: boolean;
  /** In-canvas status bar. */
  showHud?: boolean;
  /** Canvas clear colour as "#rgb" or "#rrggbb". */
  background?: string;
}

export interface WidgetOptions extends OverlayOptions {
  /** CSS pixels. Defaults: container data-width/height, else its current size, else 640×400. */
  width?: number;
  height?: number;
  initialState?: Partial<FieldSceneState>;
  /** URL of the Emscripten module. Defaults to farmhorizon.js next to this script. */
  moduleUrl?: string;
  /** Already-imported engine factory (e.g. a bundled single-file build); skips `moduleUrl`. */
  engine?: EngineFactory;
}

const FALLBACK_SIZE = { width: 640, height: 400 } as const;
const STYLE_ID = "farmhorizon-widget-style";

// Minimal view of the Emscripten module produced by engine/targets/wasm/Makefile.
interface EngineModule {
  cwrap(name: string, returnType: null, argTypes: string[]): (...args: number[]) => void;
  pauseMainLoop?: () => void;
  specialHTMLTargets?: Record<string, unknown>;
}
export type EngineFactory = (config: Record<string, unknown>) => Promise<EngineModule>;

interface EngineCalls {
  setScene: (...args: number[]) => void;
  setTimeOfDay: (hours: number) => void;
  setSize: (width: number, height: number, pixelRatio: number) => void;
  setOverlays: (showControls: number, showHud: number) => void;
  setBackground: (r: number, g: number, b: number) => void;
}

let canvasCounter = 0;

export class FarmHorizonWidget {
  readonly container: HTMLElement;
  readonly canvas: HTMLCanvasElement;

  #engine: EngineModule;
  #calls: EngineCalls;
  #state: FieldSceneState;
  #overlays: Required<Omit<OverlayOptions, "background">> = { showControls: false, showHud: false };
  #pending = false;
  #frame = 0;
  #size = { width: 0, height: 0, pixelRatio: 0 };
  #observer: ResizeObserver;
  #destroyed = false;

  /** Creates the canvas inside `container`, loads the engine and applies the initial state. */
  static async mount(container: HTMLElement, options: WidgetOptions = {}): Promise<FarmHorizonWidget> {
    injectStyle(container.getRootNode());
    const size = initialSize(container, options);

    const canvas = document.createElement("canvas");
    canvas.id = `farmhorizon-canvas-${++canvasCounter}`; // raylib targets the canvas by id
    canvas.className = "farmhorizon-canvas";
    canvas.addEventListener("contextmenu", (e) => e.preventDefault());
    container.classList.add("farmhorizon-widget");
    container.replaceChildren(canvas);

    const factory = options.engine ?? await importEngine(options.moduleUrl);
    const pixelRatio = window.devicePixelRatio || 1;
    const engine = await factory({
      canvas,
      arguments: [String(size.width), String(size.height), String(pixelRatio)],
      // Emscripten resolves "#id" with document.querySelector, which cannot see
      // into shadow roots (marimo, Jupyter widgets); register the canvas directly.
      preRun: [(m: EngineModule) => {
        if (m.specialHTMLTargets) m.specialHTMLTargets[`#${canvas.id}`] = canvas;
      }],
      print: (text: string) => console.debug("[farmhorizon]", text),
      printErr: (text: string) => console.warn("[farmhorizon]", text),
    });

    return new FarmHorizonWidget(container, canvas, engine, options, { ...size, pixelRatio });
  }

  private constructor(
    container: HTMLElement,
    canvas: HTMLCanvasElement,
    engine: EngineModule,
    options: WidgetOptions,
    size: { width: number; height: number; pixelRatio: number },
  ) {
    this.container = container;
    this.canvas = canvas;
    this.#engine = engine;
    this.#calls = {
      setScene: engine.cwrap("SetExternalSceneState", null, Array(7).fill("number")),
      setTimeOfDay: engine.cwrap("SetTimeOfDay", null, ["number"]),
      setSize: engine.cwrap("SetWidgetSize", null, ["number", "number", "number"]),
      setOverlays: engine.cwrap("SetWidgetOptions", null, ["number", "number"]),
      setBackground: engine.cwrap("SetBackgroundColor", null, ["number", "number", "number"]),
    };
    this.#size = size; // main() already created the canvas at this size

    this.#state = normalize({ ...DEFAULT_STATE, ...options.initialState });
    this.setOptions(options);
    this.#flush();

    this.#observer = new ResizeObserver(() => this.#syncSize());
    this.#observer.observe(container);
  }

  /** Current state (after clamping). */
  get state(): Readonly<FieldSceneState> {
    return this.#state;
  }

  /** Merges a partial update; values are clamped and sent once per animation frame. */
  setState(patch: Partial<FieldSceneState>): void {
    if (this.#destroyed) return;
    this.#state = normalize({ ...this.#state, ...patch });
    if (!this.#pending) {
      this.#pending = true;
      this.#frame = requestAnimationFrame(() => this.#flush());
    }
  }

  setOptions(options: OverlayOptions): void {
    if (this.#destroyed) return;
    if (options.showControls !== undefined) this.#overlays.showControls = options.showControls;
    if (options.showHud !== undefined) this.#overlays.showHud = options.showHud;
    this.#calls.setOverlays(Number(this.#overlays.showControls), Number(this.#overlays.showHud));

    if (options.background !== undefined) {
      const rgb = parseHexColor(options.background);
      if (rgb) this.#calls.setBackground(...rgb);
      else console.warn(`[farmhorizon] ignoring background "${options.background}" (expected #rgb or #rrggbb)`);
    }
  }

  /** Sets an explicit CSS size, or re-measures the container when called without arguments. */
  resize(width?: number, height?: number): void {
    if (this.#destroyed) return;
    if (width !== undefined) this.container.style.width = `${width}px`;
    if (height !== undefined) this.container.style.height = `${height}px`;
    this.#syncSize();
  }

  /** Stops observing and pauses rendering; the engine instance cannot be restarted. */
  destroy(): void {
    if (this.#destroyed) return;
    this.#destroyed = true;
    cancelAnimationFrame(this.#frame);
    this.#observer.disconnect();
    this.#engine.pauseMainLoop?.();
    delete this.#engine.specialHTMLTargets?.[`#${this.canvas.id}`];
    this.canvas.remove();
  }

  #flush(): void {
    this.#pending = false;
    if (this.#destroyed) return;
    const s = this.#state;
    this.#calls.setScene(
      s.fieldSize,
      SOIL_TYPES.indexOf(s.soilType),
      s.soilMoisture,
      CROP_TYPES.indexOf(s.cropType),
      s.ndvi,
      s.yieldExt,
      s.harvestedPct,
    );
    this.#calls.setTimeOfDay(s.timeOfDay);
  }

  #syncSize(): void {
    if (this.#destroyed) return;
    injectStyle(this.container.getRootNode());
    const cw = Math.round(this.container.clientWidth);
    const ch = Math.round(this.container.clientHeight);
    if (cw <= 0 || ch <= 0) return;
    const pixelRatio = window.devicePixelRatio || 1;
    const s = this.#size;
    if (cw === s.width && ch === s.height && pixelRatio === s.pixelRatio) return;
    this.#size = { width: cw, height: ch, pixelRatio };
    this.#calls.setSize(cw, ch, pixelRatio);
  }
}

function normalize(state: FieldSceneState): FieldSceneState {
  const out = { ...state };
  for (const key of Object.keys(STATE_LIMITS) as NumericKey[]) {
    const [lo, hi] = STATE_LIMITS[key];
    const value = Number(out[key]);
    out[key] = Number.isFinite(value) ? Math.min(hi, Math.max(lo, value)) : DEFAULT_STATE[key];
  }
  out.fieldSize = Math.round(out.fieldSize);
  if (!SOIL_TYPES.includes(out.soilType)) {
    console.warn(`[farmhorizon] unknown soilType "${out.soilType}"`);
    out.soilType = DEFAULT_STATE.soilType;
  }
  if (!CROP_TYPES.includes(out.cropType)) {
    console.warn(`[farmhorizon] unknown cropType "${out.cropType}"`);
    out.cropType = DEFAULT_STATE.cropType;
  }
  return out;
}

function initialSize(container: HTMLElement, options: WidgetOptions): { width: number; height: number } {
  const fromData = (v: string | undefined) => (v !== undefined && Number(v) > 0 ? Number(v) : undefined);
  const width = options.width ?? fromData(container.dataset.width);
  const height = options.height ?? fromData(container.dataset.height);
  if (width !== undefined) container.style.width = `${width}px`;
  if (height !== undefined) container.style.height = `${height}px`;

  // Fall back to a sensible box if the page gave the container no size at all.
  if (width === undefined && container.clientWidth === 0) {
    container.style.width = `${FALLBACK_SIZE.width}px`;
  }
  if (height === undefined && container.clientHeight === 0) {
    container.style.height = `${FALLBACK_SIZE.height}px`;
  }
  return {
    width: Math.max(1, Math.round(container.clientWidth || width || FALLBACK_SIZE.width)),
    height: Math.max(1, Math.round(container.clientHeight || height || FALLBACK_SIZE.height)),
  };
}

async function importEngine(moduleUrl?: string): Promise<EngineFactory> {
  const url = moduleUrl ?? new URL("./farmhorizon.js", import.meta.url).href;
  return (await import(/* @vite-ignore */ url)).default as EngineFactory;
}

// Emscripten clears inline canvas width/height on every resize, so the CSS
// size has to come from a stylesheet rule. Hosts that render widgets in a
// shadow root (marimo) need the rule inside that root, not in document.head.
function injectStyle(root: Node): void {
  const target = root instanceof ShadowRoot ? root : document.head;
  if (target.querySelector(`#${STYLE_ID}`)) return;
  const style = document.createElement("style");
  style.id = STYLE_ID;
  style.textContent = `
    .farmhorizon-widget { position: relative; overflow: hidden; }
    .farmhorizon-canvas { display: block; width: 100%; height: 100%; outline: none; }
  `;
  target.append(style);
}

function parseHexColor(color: string): [number, number, number] | null {
  const m = /^#?([0-9a-f]{3}|[0-9a-f]{6})$/i.exec(color.trim());
  if (!m) return null;
  const hex = m[1].length === 3 ? [...m[1]].map((c) => c + c).join("") : m[1];
  return [0, 2, 4].map((i) => parseInt(hex.slice(i, i + 2), 16)) as [number, number, number];
}
