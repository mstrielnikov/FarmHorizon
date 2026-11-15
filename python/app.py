import marimo

__generated_with = "0.25.2"
app = marimo.App(
    width="full",
    app_title="FarmHorizon: Harvest Simulation and Decision Making",
)


@app.cell
def _():
    import marimo as mo

    from farmhorizon import (
        CROP_TYPES,
        DEFAULT_SCENE_STATE,
        SOIL_TYPES,
        FarmHorizonWidget,
        build_territory_figure,
        coerce_territory_records,
        match_territories,
        resolve_selected_parcels,
    )

    # Ready to accept a pipeline DataFrame or list[dict] via coerce_territory_records(pipeline_df)
    # once the independent data-decision-pipeline branch is merged.
    parcel_records = coerce_territory_records()

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
        build_territory_figure,
        match_territories,
        mo,
        parcel_records,
        resolve_selected_parcels,
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
    # Instantiate the Wasm widget once so slider and map updates stream over comm
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
        label="Canopy scale (yield)",
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

    map_scope = mo.ui.radio(
        options={"World Map": "world", "Regional Focus": "region"},
        value="World Map",
        inline=True,
        label="Map view",
    )
    min_match_pct = mo.ui.slider(
        start=50,
        stop=90,
        step=2,
        value=68,
        label="Min match (%)",
        show_value=True,
    )
    return (
        crop_type,
        field_size,
        harvested_pct,
        map_scope,
        min_match_pct,
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
                "## FarmHorizon: Harvest Simulation and Decision Making\n"
                "Set crop and field parameters to simulate the 3D WebAssembly scene "
                "and locate matching coordinate territories on the interactive map."
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
    build_territory_figure,
    crop_type,
    map_scope,
    match_territories,
    min_match_pct,
    mo,
    ndvi,
    parcel_records,
    soil_moisture,
    soil_type,
    yield_ext,
):
    match_result = match_territories(
        parcel_records,
        crop_type=crop_type.value,
        soil_type=soil_type.value,
        soil_moisture=soil_moisture.value,
        ndvi=ndvi.value,
        yield_ext=yield_ext.value,
        min_score=min_match_pct.value / 100.0,
    )

    if match_result.lat_range and match_result.lon_range:
        lat_lo, lat_hi = match_result.lat_range
        lon_lo, lon_hi = match_result.lon_range
        coord_summary = (
            f"**{len(match_result.matched)} / {len(parcel_records)}** parcels matched · "
            f"Lat `{lat_lo:.2f}° .. {lat_hi:.2f}°` · "
            f"Lon `{lon_lo:.2f}° .. {lon_hi:.2f}°` "
            f"*(click any marker to preview in 3D)*"
        )
    else:
        coord_summary = (
            f"**0 / {len(parcel_records)}** parcels matched at ≥{min_match_pct.value}% "
            "*(lower Min match % to widen the territory search)*"
        )

    territory_map = mo.ui.plotly(
        build_territory_figure(match_result, scope=map_scope.value),
        config={"scrollZoom": True, "displaylogo": False},
    )

    mo.vstack(
        [
            mo.hstack(
                [
                    mo.vstack(
                        [
                            mo.md("### Matching Territories"),
                            mo.md(coord_summary),
                        ],
                        gap=0.15,
                    ),
                    mo.hstack([map_scope, min_match_pct], align="center", gap=1.5),
                ],
                justify="space-between",
                align="end",
            ),
            territory_map,
        ],
        gap=0.5,
    )
    return match_result, territory_map


@app.cell
def _(
    VIEWPORT_PRESETS,
    crop_type,
    field_size,
    harvested_pct,
    match_result,
    mo,
    ndvi,
    parcel_records,
    resolve_selected_parcels,
    scene,
    show_controls,
    show_hud,
    soil_moisture,
    soil_type,
    territory_map,
    time_of_day,
    viewport_preset,
    yield_ext,
):
    selected_parcels = resolve_selected_parcels(territory_map.value, parcel_records)

    if selected_parcels:
        picked = selected_parcels[0]
        active_state = scene.set_scene(
            field_size=field_size.value,
            soil_type=picked["soil_type"],
            soil_moisture=picked["soil_moisture"],
            crop_type=picked.get("crop_type", crop_type.value),
            ndvi=picked["ndvi"],
            yield_ext=picked["yield_ext"],
            harvested_pct=harvested_pct.value,
            time_of_day=time_of_day.value,
        )
        source_badge = (
            f"{picked['field_id']} ({picked['latitude']:.2f}°, {picked['longitude']:.2f}°)"
        )
    else:
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
        source_badge = f"Manual ({len(match_result.matched)} matched)"

    vp_w, vp_h = VIEWPORT_PRESETS[viewport_preset.value]
    scene.resize(vp_w, vp_h)
    scene.set_overlays(
        show_hud=show_hud.value,
        show_controls=show_controls.value,
    )

    mo.hstack(
        [
            mo.stat(label="Scene Source", value=source_badge),
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
                label="Canopy (Yield)",
                value=f"{active_state['yieldExt']:.0f}",
            ),
        ],
        justify="space-around",
    )
    return


if __name__ == "__main__":
    app.run()
