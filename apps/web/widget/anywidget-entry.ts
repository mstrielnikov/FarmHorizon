// AnyWidget ESM entry: mounts FarmHorizonWidget inside a marimo / Jupyter
// widget container and keeps it synced with Python traitlets.

import createFarmHorizon from "../../../build-wasm/farmhorizon-single.js";
import { FarmHorizonWidget, type FieldSceneState } from "./farmhorizon-widget";

interface AnyWidgetModel {
  get(key: "state"): Partial<FieldSceneState> | undefined;
  get(key: "width"): number | undefined;
  get(key: "height"): number | undefined;
  get(key: "background"): string | undefined;
  get(key: "show_controls"): boolean | undefined;
  get(key: "show_hud"): boolean | undefined;
  on(event: string, callback: () => void): void;
  off?(event: string, callback: () => void): void;
}

export default {
  render({ model, el }: { model: AnyWidgetModel; el: HTMLElement }): () => void {
    const host = document.createElement("div");
    host.style.borderRadius = "8px";
    host.style.overflow = "hidden";
    el.replaceChildren(host);

    let disposed = false;
    let instance: FarmHorizonWidget | null = null;

    const syncState = () => {
      const next = model.get("state");
      if (next) instance?.setState(next);
    };
    const syncSize = () => {
      instance?.resize(model.get("width"), model.get("height"));
    };
    const syncOverlays = () => {
      instance?.setOptions({
        background: model.get("background"),
        showControls: model.get("show_controls"),
        showHud: model.get("show_hud"),
      });
    };

    model.on("change:state", syncState);
    model.on("change:width", syncSize);
    model.on("change:height", syncSize);
    model.on("change:background", syncOverlays);
    model.on("change:show_controls", syncOverlays);
    model.on("change:show_hud", syncOverlays);

    void FarmHorizonWidget.mount(host, {
      engine: createFarmHorizon,
      width: model.get("width"),
      height: model.get("height"),
      background: model.get("background"),
      showControls: model.get("show_controls"),
      showHud: model.get("show_hud"),
      initialState: model.get("state"),
    }).then((widget) => {
      if (disposed) {
        widget.destroy();
        return;
      }
      instance = widget;
      // Apply any trait updates that arrived while Wasm was still initializing.
      syncOverlays();
      syncSize();
      syncState();
    });

    return () => {
      disposed = true;
      model.off?.("change:state", syncState);
      model.off?.("change:width", syncSize);
      model.off?.("change:height", syncSize);
      model.off?.("change:background", syncOverlays);
      model.off?.("change:show_controls", syncOverlays);
      model.off?.("change:show_hud", syncOverlays);
      instance?.destroy();
      instance = null;
    };
  },
};
