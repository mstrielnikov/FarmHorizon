import marimo

__generated_with = "0.25.2"
app = marimo.App(width="full", app_title="FarmHorizon — Field Scene Widget")


@app.cell
def _():
    import sys
    from pathlib import Path

    import marimo as mo

    _dashboard_dir = str(Path(__file__).resolve().parent)
    if _dashboard_dir not in sys.path:
        sys.path.insert(0, _dashboard_dir)

    from widget import (
        CROP_TYPES,
        DEFAULT_SCENE_STATE,
        SOIL_TYPES,
        FarmHorizonWidget,
    )

    VIEWPORT_PRESETS: dict[str, tuple[int, int]] = {
        "Compact (560×340)": (560, 340),
        "Standard (760×460)": (760, 460),
        "Wide (960×540)": (960, 540),
    }
    return (
        CROP_TYPES,
        DEFAULT_SCENE_STATE,
        FarmHorizonWidget,
        SOIL_TYPES,
        VIEWPORT_PRESETS,
        mo,
    )


@app.cell
def _(
    CROP_TYPES,
    DEFAULT_SCENE_STATE,
    FarmHorizonWidget,
    SOIL_TYPES,
    VIEWPORT_PRESETS,
    mo,
):
    # Instantiate the Wasm widget once so slider updates stream over comm
    # without remounting the WebGL context.
    scene = mo.ui.anywidget(FarmHorizonWidget(width=760, height=460))

    field_size = mo.ui.slider(
        start=12,
        stop=64,
        step=2,
        value=int(DEFAULT_SCENE_STATE["fieldSize"]),
        label="Field size (cells)",
        show_value=True,
        full_width=True,
    )
    soil_type = mo.ui.dropdown(
        options=list(SOIL_TYPES),
        value=str(DEFAULT_SCENE_STATE["soilType"]),
        label="Soil type",
        full_width=True,
    )
    soil_moisture = mo.ui.slider(
        start=10,
        stop=60,
        step=1,
        value=float(DEFAULT_SCENE_STATE["soilMoisture"]),
        label="Soil moisture (%)",
        show_value=True,
        full_width=True,
    )
    crop_type = mo.ui.dropdown(
        options=list(CROP_TYPES),
        value=str(DEFAULT_SCENE_STATE["cropType"]),
        label="Crop type",
        full_width=True,
    )
    ndvi = mo.ui.slider(
        start=0.10,
        stop=0.85,
        step=0.01,
        value=float(DEFAULT_SCENE_STATE["ndvi"]),
        label="NDVI",
        show_value=True,
        full_width=True,
    )
    yield_ext = mo.ui.slider(
        start=10,
        stop=70,
        step=1,
        value=float(DEFAULT_SCENE_STATE["yieldExt"]),
        label="Canopy scale",
        show_value=True,
        full_width=True,
    )
    harvested_pct = mo.ui.slider(
        start=0,
        stop=100,
        step=1,
        value=float(DEFAULT_SCENE_STATE["harvestedPct"]),
        label="Harvested (%)",
        show_value=True,
        full_width=True,
    )
    time_of_day = mo.ui.slider(
        start=6.0,
        stop=18.0,
        step=0.25,
        value=float(DEFAULT_SCENE_STATE["timeOfDay"]),
        label="Time of day (h)",
        show_value=True,
        full_width=True,
    )

    viewport_preset = mo.ui.dropdown(
        options=list(VIEWPORT_PRESETS.keys()),
        value="Standard (760×460)",
        label="Widget size",
        full_width=True,
    )
    show_hud = mo.ui.checkbox(value=False, label="In-canvas HUD")
    show_controls = mo.ui.checkbox(value=False, label="In-canvas raygui")
    return (
        crop_type,
        field_size,
        harvested_pct,
        ndvi,
        scene,
        show_controls,
        show_hud,
        soil_moisture,
        soil_type,
        time_of_day,
        viewport_preset,
        yield_ext,
    )


@app.cell
def _(
    crop_type,
    field_size,
    harvested_pct,
    mo,
    ndvi,
    scene,
    show_controls,
    show_hud,
    soil_moisture,
    soil_type,
    time_of_day,
    viewport_preset,
    yield_ext,
):
    controls_sidebar = mo.vstack(
        [
            mo.md("### Field & Soil"),
            field_size,
            soil_type,
            soil_moisture,
            mo.md("### Crop & Harvest"),
            crop_type,
            ndvi,
            yield_ext,
            harvested_pct,
            mo.md("### Lighting & Viewport"),
            time_of_day,
            viewport_preset,
            mo.hstack([show_hud, show_controls], justify="start", gap=1.0),
        ],
        gap=0.55,
    )

    mo.vstack(
        [
            mo.md(
                "## FarmHorizon — Embedded WebAssembly Scene\n"
                "Reactive **marimo** controls driving the C++ / Raylib WebAssembly "
                "field renderer via `anywidget`."
            ),
            mo.hstack(
                [controls_sidebar, scene],
                widths=[1, 2.4],
                align="start",
                gap=1.5,
            ),
        ],
        gap=1.0,
    )
    return


@app.cell
def _(
    VIEWPORT_PRESETS,
    crop_type,
    field_size,
    harvested_pct,
    mo,
    ndvi,
    scene,
    show_controls,
    show_hud,
    soil_moisture,
    soil_type,
    time_of_day,
    viewport_preset,
    yield_ext,
):
    active_state = scene.set_scene(
        field_size=field_size.value,
        soil_type=soil_type.value,
        soil_moisture=soil_moisture.value,
        crop_type=crop_type.value,
        ndvi=ndvi.value,
        yield_ext=yield_ext.value,
        harvested_pct=harvested_pct.value,
        time_of_day=time_of_day.value,
    )
    vp_w, vp_h = VIEWPORT_PRESETS[viewport_preset.value]
    scene.resize(vp_w, vp_h)
    scene.set_overlays(
        show_hud=show_hud.value,
        show_controls=show_controls.value,
    )

    mo.hstack(
        [
            mo.stat(
                label="Crop / Soil",
                value=f"{active_state['cropType'].title()} · {active_state['soilType'].title()}",
            ),
            mo.stat(
                label="Grid",
                value=f"{active_state['fieldSize']}×{active_state['fieldSize']}",
            ),
            mo.stat(
                label="Moisture / NDVI",
                value=f"{active_state['soilMoisture']:.0f}% · {active_state['ndvi']:.2f}",
            ),
            mo.stat(
                label="Harvested",
                value=f"{active_state['harvestedPct']:.0f}%",
            ),
            mo.stat(
                label="Viewport",
                value=f"{vp_w}×{vp_h} px",
            ),
        ],
        justify="space-around",
    )
    return


if __name__ == "__main__":
    app.run()
