// Demo page: plain HTML controls drive the widget through its public API only.
import {
  CROP_TYPES,
  FarmHorizonWidget,
  SOIL_TYPES,
  type FieldSceneState,
} from "../widget/farmhorizon-widget";

const BACKGROUND = "#1e1e23";

const container = document.getElementById("field-widget") as HTMLElement;
const controls = document.getElementById("controls") as HTMLElement;
const stateView = document.getElementById("state-view") as HTMLPreElement;
const sizeBar = document.getElementById("size-bar") as HTMLElement;
const sizeLabel = document.getElementById("size-label") as HTMLElement;

const widget = await FarmHorizonWidget.mount(container, {
  background: BACKGROUND,
  initialState: { cropType: "wheat", harvestedPct: 30 },
});

// ── State controls ────────────────────────────────────────────────

fillSelect("soilType", SOIL_TYPES);
fillSelect("cropType", CROP_TYPES);

const numericKeys = ["fieldSize", "soilMoisture", "ndvi", "yieldExt", "harvestedPct", "timeOfDay"] as const;

for (const key of numericKeys) {
  const input = field<HTMLInputElement>(key);
  input.value = String(widget.state[key]);
  input.addEventListener("input", () => {
    widget.setState({ [key]: Number(input.value) } as Partial<FieldSceneState>);
    render();
  });
}

for (const key of ["soilType", "cropType"] as const) {
  const select = field<HTMLSelectElement>(key);
  select.value = widget.state[key];
  select.addEventListener("change", () => {
    widget.setState({ [key]: select.value } as Partial<FieldSceneState>);
    render();
  });
}

// ── Overlay options ───────────────────────────────────────────────

for (const key of ["showControls", "showHud"] as const) {
  const box = field<HTMLInputElement>(key);
  box.addEventListener("change", () => widget.setOptions({ [key]: box.checked }));
}
const background = field<HTMLInputElement>("background");
background.value = BACKGROUND;
background.addEventListener("input", () => widget.setOptions({ background: background.value }));

// ── Widget size ───────────────────────────────────────────────────

sizeBar.addEventListener("click", (event) => {
  const button = (event.target as HTMLElement).closest("button");
  if (!button) return;
  for (const b of sizeBar.querySelectorAll("button")) b.setAttribute("aria-pressed", String(b === button));

  const height = Number(button.dataset.h);
  if (button.dataset.w === "fluid") {
    container.style.width = "100%";
    widget.resize(undefined, height);
  } else {
    widget.resize(Number(button.dataset.w), height);
  }
});

new ResizeObserver(() => {
  sizeLabel.textContent = `${container.clientWidth}×${container.clientHeight} px`;
}).observe(container);

render();

// ── Helpers ───────────────────────────────────────────────────────

function render(): void {
  for (const key of numericKeys) {
    const out = controls.querySelector<HTMLOutputElement>(`output[data-for="${key}"]`);
    if (out) out.value = formatValue(key, widget.state[key]);
  }
  stateView.textContent = `widget.state = ${JSON.stringify(widget.state, null, 2)}`;
}

function formatValue(key: (typeof numericKeys)[number], value: number): string {
  if (key === "ndvi") return value.toFixed(2);
  if (key === "timeOfDay") return value.toFixed(1);
  return String(Math.round(value));
}

function field<T extends HTMLElement>(name: string): T {
  return controls.querySelector(`[name="${name}"]`) as T;
}

function fillSelect(name: string, values: readonly string[]): void {
  const select = field<HTMLSelectElement>(name);
  select.replaceChildren(
    ...values.map((v) => new Option(v.charAt(0).toUpperCase() + v.slice(1), v)),
  );
}
