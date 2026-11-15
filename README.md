# FarmHorizon

**FarmHorizon** is a real-time isometric crop field and harvester scene renderer with native (Raylib) and WebAssembly (Emscripten/WebGL) targets.<br>
The renderer is stateless,acting only as a visualization loop with no simulation / decision making within. It expose external parameters from external yield prediction models

## The model

The source dataset [yield_prediction_dataset_raw.csv](./data/inputs/yield_prediction_dataset_raw.csv)
...

### Math

...

## Scene renderer

### 1. Field Grid (`engine/include/farmhorizon/core/field_grid.hpp`)

The `Grid` stores a flat $N \times N$ array of `Cell` structures representing the parcel's visual state:

- `role`: `BorderHeadland`, `StandingCrop`, `HarvestedStubble`, or `HarvesterActive`.
- `height`: Subtle plough furrow relief along crop rows (no mountain elevation).
- `crop_height` & `canopy_density`: Extruded 3D crop canopy dimensions scaled by externally supplied `yield_ext` and `ndvi_index`.
- `r, g, b` & `crop_r, crop_g, crop_b`: Soil bed and crop canopy RGB colors mapped from `SoilType`, `soil_moisture`, `CropType`, and `ndvi_index`.

### 2. Scene Parameters (`engine/include/farmhorizon/core/field_state.hpp`)

`GenConfig` (`SceneConfig`) holds the externally controlled parameters exposed in the floating Raygui panel and via the C/WebAssembly bridge `SetExternalSceneState(...)`:

- `Zoom` (`0.50` – `1.80`) & `Time of Day` (`6.0` – `18.0` hours)
- `Field Size` (`12x12` – `64x64`)
- `Soil Type` (`Sandy`, `Loam`, `Clay`, `Chernozem`) & `Soil Moist` (`10%` – `60%`)
- `Crop Type` (`Wheat`, `Rice`, `Corn`, `Soybean`) & `NDVI Index` (`0.10` – `0.85`)
- `Yield (Ext)` (`10` – `70` t/ha) & `Harvested` (`0%` – `100%` along a boustrophedon harvest pass with an active Combine Harvester marker)

## Buil & Run

**GUI (Native):**

```bash
make gui
make run # Runs ./build/bin/FarmHorizon-gui
```

**WebAssembly & Dashboard:**

```bash
make wasm
make serve       # Standalone TS demo at http://localhost:8080
make marimo      # Reactive marimo dashboard at http://localhost:8080
make marimo-edit # Interactive marimo notebook editor at http://localhost:8080
```

## Resources

Based on:

1. Kaggle [dataset](./data/inputs/yield_prediction_dataset_raw.csv) + [notebook](./python/notebooks/crop-yield-prediction-notebook.ipynb) from [source](https://www.kaggle.com/code/nishchalchandel/crop-yield-prediction-notebook?select=yield_prediction_dataset.csv)
2. [Scene grid with isometric projection and Raylib](https://github.com/mstrielnikov/Dunjourno)
