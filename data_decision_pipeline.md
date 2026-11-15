# FarmHorizon Data & Crop-Selection Decision Pipeline

This document concludes our data investigation on [yield_prediction_dataset_raw.csv](./data/inputs/yield_prediction_dataset_raw.csv) (alongside [yield_prediction_dataset-selected-columns.csv](./data/inputs/yield_prediction_dataset-selected-columns.csv) and [crop-yield-prediction-notebook.ipynb](./python/notebooks/crop-yield-prediction-notebook.ipynb)), organizing the end-to-end **Forecast + Ridge Regression + Linear Programming (LP)** workflow into **6 implementation tasks** aligned with the restructured workspace (`data/`, `engine/`, `web/`, `python/`).

---

## Pipeline Overview

1. **Task 1 — Clean, Repair Dates & Normalize Raw CSV**: Drop empty/collinear columns from [yield_prediction_dataset_raw.csv](./data/inputs/yield_prediction_dataset_raw.csv) (`1,625` rows, `90` parcels), repair `322` swapped dates, normalize features, and infer parcel [`SoilType`](./engine/include/farmhorizon/core/field_state.hpp#L9-L14).
2. **Task 2 — Fit Per-Parcel Seasonal Harmonic Forecasters**: Fit 2-harmonic Fourier Ridge curves per parcel and evaluate seasonal expectations plus weather-shock scenarios $(\Delta T, \Delta R)$.
3. **Task 3 — Fit Quadratic Ridge Productivity & Crop Suitability Model**: Fit global standardized quadratic Ridge weights ($R^2 = 0.823$), persist fitted artifacts to [data/outputs/](./data/outputs/.gitkeep), and compute crop-specific yields $\hat{y} _ {f,c}$ (`t/ha`).
4. **Task 4 — Solve Crop Allocation via Linear Programming (LP)**: Optimize land shares $x ^ {\ast} _ c$ and water shadow price maximizing expected net margin under land, water budget, rotation cap, and drought-floor constraints.
5. **Task 5 — Build Interactive Marimo Decision Dashboard**: Extend [python/app.py](./python/app.py) with parcel map, 2023 satellite + forecast timeline scrubber, crop margin comparison bars, and LP scenario controls alongside the embedded WebAssembly scene.
6. **Task 6 — Wire Pipeline State into the WebAssembly Widget**: Drive [`FarmHorizonWidget.set_scene`](./python/src/farmhorizon/widget.py#L123-L129) $\to$ [`SetExternalSceneState`](./engine/targets/wasm/main.cpp#L30-L36) at 60 FPS from reactive marimo controls and timeline scrubbing.

> [!NOTE]
> **Workspace environment**: The editable `farmhorizon` Python package ([python/pyproject.toml](./python/pyproject.toml)), `marimo` dashboard ([python/app.py](./python/app.py)), and `anywidget` WebAssembly bridge ([widget.py](./python/src/farmhorizon/widget.py) $\to$ [anywidget-entry.ts](./web/widget/anywidget-entry.ts) $\to$ [farmhorizon-widget.ts](./web/widget/farmhorizon-widget.ts)) are already bootstrapped via [Makefile](./Makefile). Adding `numpy`, `pandas`, `scikit-learn`, `scipy`, and `altair`/`plotly` to [python/pyproject.toml](./python/pyproject.toml) keeps all modeling, simulation, and reactive visualization inside `python/.venv`.

---

## Task 1 — Clean, Repair Dates & Normalize Raw CSV

Verified across all `1,625` rows and `90` parcels (`Field_1`–`Field_90`) of [yield_prediction_dataset_raw.csv](./data/inputs/yield_prediction_dataset_raw.csv):

| Action | Columns Affected | Rule & Empirical Verification |
| :--- | :--- | :--- |
| **Drop empty & collinear** | `Unnamed: 13`, `Unnamed: 14`, `NDWI`, `SAVI` | `Unnamed: 13–14` are 100% empty; $\text{NDWI} \equiv -\text{GNDVI}$ ($r = -1.00$); $\text{SAVI} \equiv 1.5\thinspace\text{NDVI}$ ($r = +1.00$). |
| **Exclude raw `crop_type` from regression** | `crop_type` | Stamped sequentially 3 fields per crop (`Field_1..3 = Rice`, `Field_4..6 = Wheat`, ..., `Field_88..90 = Oil Palm`) with zero causal effect on `yield` (5-fold `GroupKFold` $R^2$ drops from `0.445` to `0.426` when included). |
| **Repair swapped dates** | `date_of_image` | Rows per `field_id` follow a chronological ~15-day pass cadence, but `322` rows with `day <= 12` had day/month swapped (`MM-DD-YYYY` vs `DD-MM-YYYY`). Selecting the earliest valid candidate $> t _ {\text{prev}}$ yields **`0` non-monotonic dates** across all `1,625` rows. |
| **Clamp & log-transform** | `soil_moisture`, `rainfall` | Clamp `soil_moisture` to `[0, 100]` (fixes row `1112` at `102.13%`). Transform `r_log = ln(1 + rainfall)` to tame `+4.51` right-skew without discarding real monsoon storms. |
| **Z-score standardize** | `NDVI`, `soil_moisture`, `temperature`, `r_log` | Compute training $(\mu _ j, \sigma _ j)$ so large-variance columns ($\sigma ^ 2 _ {\text{moist}} \approx 192$) do not crush `NDVI` ($\sigma ^ 2 \approx 0.048$) under L2 regularization. |
| **Infer parcel `SoilType`** | `soil_moisture`, `rainfall`, `NDVI` | Classify each parcel's [`SoilType`](./engine/include/farmhorizon/core/field_state.hpp#L9-L14) (`0..3`: `Sandy`, `Loam`, `Clay`, `Chernozem`) from its water-retention ratio $\overline{\text{moist}} _ f / \overline{\ln(1+\text{rain})} _ f$ and baseline organic vigor $\overline{\text{NDVI}} _ f$. |

### How-To
- **Target files**: `cleaning.py` under [python/src/farmhorizon/pipeline/](./python/src/farmhorizon/pipeline/__init__.py) (reads [yield_prediction_dataset_raw.csv](./data/inputs/yield_prediction_dataset_raw.csv), writes cleaned dataset to [data/outputs/](./data/outputs/.gitkeep)), plus unit tests in [python/tests/](./python/tests/.gitkeep).
- **Implementation steps**:
  1. Load rows grouped by `field_id` preserving file row order, omitting `Unnamed: 13`, `Unnamed: 14`, `NDWI`, and `SAVI`.
  2. For each `date_of_image` string `A-B-YYYY`, generate valid date candidates `(YYYY, B, A)` (`DD-MM-YYYY`) and, when `A <= 12` and `A != B`, `(YYYY, A, B)` (`MM-DD-YYYY`); pick `min(d for d in candidates if d > prev_date)` and convert to day-of-year $d \in [1, 365]$.
  3. Apply `clamp(soil_moisture, 0, 100)` and `r_log = ln(1 + rainfall)`, then bucket each field's mean retention & `NDVI` into `SoilType` (`sandy`, `loam`, `clay`, `chernozem`).
- **Libraries & functions**:
  - *`pandas` / `numpy`*: `pd.read_csv(..., usecols=...)`, `pd.to_datetime(..., format="%d-%m-%Y", errors="coerce")`, `Series.dt.dayofyear`, `Series.clip(0.0, 100.0)`, `np.log1p()`, `DataFrame.groupby("field_id", sort=False)`.
  - *Zero-dep Python stdlib*: `csv.DictReader`, `datetime.date(y, m, d)`, `date.timetuple().tm_yday`, `math.log1p()`.

---

## Task 2 — Fit Per-Parcel Seasonal Harmonic Forecasters

For each parcel $f \in \lbrace 1..90 \rbrace$, fit a 2-harmonic Fourier Ridge regression ($\lambda = 0.5$) over day-of-year $d \in [1, 365]$ ($\omega = 2\pi d / 365$):

$$\hat{z} _ f(d) = \beta _ {f,0} + \beta _ {f,1}\sin\omega + \beta _ {f,2}\cos\omega + \beta _ {f,3}\sin 2\omega + \beta _ {f,4}\cos 2\omega$$

**Verified goodness-of-fit across all 90 date-repaired fields**:
- **`soil_moisture(d)`**: $R^2 = \mathbf{0.900}$ ($\text{RMSE} = 4.39$%)
- **`NDVI(d)`**: $R^2 = \mathbf{0.892}$ ($\text{RMSE} = 0.072$)
- **`temperature(d)`**: $R^2 = \mathbf{0.714}$ ($\text{RMSE} = 5.26 ^ \circ\text{C}$)
- **`ln(1 + rainfall)(d)`**: $R^2 = \mathbf{0.380}$ ($\text{RMSE} = 0.33$ log-mm; residual std $\sigma _ {f,\text{rain}}$ quantifies parcel rainfall volatility)

Given a growing-season window $[d _ {\text{start}}, d _ {\text{end}}]$ and user weather-shock sliders ($\Delta T \in [-5, +5] ^ \circ\text{C}$, rainfall shock $\Delta R \in [-50, +50]$%), the forecaster outputs seasonal parcel expectations $(\widehat{T} _ f, \widehat{M} _ f, \widehat{R} _ f, \widehat{\text{NDVI}} _ f)$ plus a **drought stress scenario** $(\widehat{T} _ f + 2 ^ \circ\text{C},\enspace 0.75\widehat{M} _ f,\enspace \exp(\hat{r} _ {\log,f} - \sigma _ {f,\text{rain}}) - 1)$.

### How-To
- **Target file**: `forecast.py` under [python/src/farmhorizon/pipeline/](./python/src/farmhorizon/pipeline/__init__.py).
- **Implementation steps**:
  1. Build the $N _ f \times 5$ design matrix $X _ f = [1, \sin\omega, \cos\omega, \sin 2\omega, \cos 2\omega]$ per parcel (leaving intercept $\beta _ {f,0}$ unpenalized).
  2. Solve for $\beta _ f$ on `temperature`, `soil_moisture`, `NDVI`, and `r_log`, and record $\sigma _ {f,\text{rain}} = \text{std}(r _ {\log} - X _ f \beta _ {f,\text{rain}})$.
  3. Sample $\hat{z} _ f(d)$ across $[d _ {\text{start}}, d _ {\text{end}}]$ to compute both daily timeline curves and seasonal window means with $(\Delta T, \Delta R)$ applied.
- **Libraries & functions**:
  - *`scikit-learn` / `numpy`*: `np.column_stack([np.ones_like(w), np.sin(w), np.cos(w), np.sin(2*w), np.cos(2*w)])` + `np.linalg.solve(X.T @ X + reg, X.T @ y)` (or `sklearn.linear_model.Ridge(alpha=0.5)`) + `np.std(residuals, ddof=1)`.
  - *Alternative (`statsmodels`)*: `statsmodels.tsa.deterministic.CalendarFourier(freq="YE", order=2)`.

---

## Task 3 — Fit Quadratic Ridge Productivity & Crop Suitability Model

### 3a. Empirical Parcel Productivity Index ($\pi _ f$)
A global quadratic Ridge model ($\lambda = 1.0$) trained on z-scored features $[z _ {\text{ndvi}}, z _ {\text{moist}}, z _ {\text{temp}}, z _ {\text{rain}}, z _ {\text{ndvi}} ^ 2, z _ {\text{rain}} ^ 2, z _ {\text{ndvi}} z _ {\text{rain}}]$ predicts raw parcel productivity $\hat{y} _ {\text{raw}, f}$ (**5-fold `GroupKFold(field_id)` $R^2 = 0.823$**, up from `0.722` unnormalized), mapped to a dimensionless parcel index:

$$\pi _ f = \text{clamp}\left(\frac{\hat{y} _ {\text{raw}, f} - 26.0}{60.0 - 26.0},\enspace 0.15,\enspace 1.00\right)$$

### 3b. Crop-Specific Yield Response ($\hat{y} _ {f,c}$)
Each [`CropType`](./engine/include/farmhorizon/core/field_state.hpp#L16-L21) $c \in \lbrace \text{Wheat}, \text{Rice}, \text{Corn}, \text{Soybean} \rbrace$ applies a quadratic thermal & moisture suitability envelope $S _ c(\widehat{T} _ f, \widehat{W} _ f) \in [0.1, 1.0]$ and reference ceiling $Y _ {c,\max}$ (`t/ha`):

$$\hat{y} _ {f,c} = Y _ {c,\max} \cdot \pi _ f \cdot S _ c(\widehat{T} _ f, \widehat{W} _ f)$$

| Crop (`CropType`) | $Y _ {c,\max}$ (`t/ha`) | Optimal Temp $T ^ {\ast} _ c$ (°C) | Water Need $w _ c$ (index) | Price $p _ c$ (`USD/t`) | Input Cost $C _ c$ (`USD/ha`) | Niche Where It Wins |
| :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **Wheat** (`0`) | `6.5` | `16°C` (`8–24°C`) | `0.45` | `$260` | `$520` | Cool/winter (`Rabi`) parcels & moderate water |
| **Rice** (`1`) | `7.5` | `27°C` (`20–34°C`) | `0.85` | `$310` | `$780` | Warm, high-moisture / monsoon-fed clay & paddy parcels |
| **Corn / Maize** (`2`) | `10.5` | `24°C` (`16–31°C`) | `0.65` | `$210` | `$740` | High-productivity loams/chernozems with reliable moisture |
| **Soybean** (`3`) | `3.8` | `25°C` (`17–32°C`) | `0.38` | `$440` | `$410` | Tight water budgets, sandy/drier soils, or low-risk floors |

### How-To
- **Target files**: `yield_model.py` under [python/src/farmhorizon/pipeline/](./python/src/farmhorizon/pipeline/__init__.py) $\to$ serialized model artifact `field_models.json` in [data/outputs/](./data/outputs/.gitkeep).
- **Implementation steps**:
  1. Assemble degree-2 features, verify 5-fold `GroupKFold(field_id)` $R^2$, fit final `(mean, scale, coef, intercept)` on all `1,625` rows, and serialize alongside the 90 parcel records into `data/outputs/field_models.json`.
  2. Evaluate $\pi _ f$ via z-scored dot-product and compute both expected $\hat{y} _ {f,c}$ and drought-stressed $\hat{y} ^ {\text{drought}} _ {f,c}$ for all 4 crops.
- **Libraries & functions**:
  - *`scikit-learn`*: `sklearn.pipeline.make_pipeline(StandardScaler(), RidgeCV(alphas=[0.1, 1.0, 10.0]))`, `sklearn.model_selection.GroupKFold(n_splits=5)`, `cross_val_score(..., groups=df["field_id"], scoring="r2")`, `json.dump()`.
  - *Fast evaluation*: Vectorized `numpy` dot-product (`< 0.1 ms` per slider update inside marimo).

---

## Task 4 — Solve Crop Allocation via Linear Programming (LP)

Given expected crop yields $\hat{y} _ {f,c}$ and drought-stressed yields $\hat{y} ^ {\text{drought}} _ {f,c}$ on parcel $f$, solve for land shares $x = (x _ {\text{Wheat}}, x _ {\text{Rice}}, x _ {\text{Corn}}, x _ {\text{Soybean}}) ^ \top$:

$$\max _ {x \in \mathbb{R}^4 _ {\ge 0}} \sum _ {c=0} ^ {3} \bigl(p _ c \hat{y} _ {f,c} - C _ c\bigr)\thinspace x _ c$$

subject to:
1. **Land constraint**: $\sum _ {c=0} ^ {3} x _ c \le 1$ (allows fallow share if extreme shock makes margins negative).
2. **Water availability constraint**: $\sum _ {c=0} ^ {3} w _ c x _ c \le W _ {\text{avail}}(\widehat{M} _ f, \widehat{R} _ f)$.
3. **Diversification / rotation cap**: $0 \le x _ c \le u _ {\max}$ (slider `0.40 .. 1.00`; at `1.00`, picks single best crop unless water or risk binds).
4. **Drought downside floor**: $\sum _ {c=0} ^ {3} \bigl(p _ c \hat{y} ^ {\text{drought}} _ {f,c} - C _ c\bigr)\thinspace x _ c \ge R _ {\min}$.

> [!TIP]
> Because $x \in \mathbb{R}^4$, solving the LP also yields the **shadow price of water** ($\partial \text{Profit} ^ {\ast} / \partial W _ {\text{avail}}$), showing directly how much `USD/ha` additional irrigation is worth on `Field_k`.

### How-To
- **Target file**: `lp_solver.py` under [python/src/farmhorizon/pipeline/](./python/src/farmhorizon/pipeline/__init__.py) (with unit tests in [python/tests/](./python/tests/.gitkeep)).
- **Implementation steps**:
  1. Compute expected net margins $m _ c = p _ c \hat{y} _ {f,c} - C _ c$ and drought margins $m ^ {\text{drought}} _ c = p _ c \hat{y} ^ {\text{drought}} _ {f,c} - C _ c$.
  2. Solve the bounded 4-variable LP via HiGHS and extract the dual variable on the water constraint (`-res.ineqlin.marginals[water_idx]`).
  3. Return a typed result (`shares`, `expected_profit`, `drought_profit`, `water_used`, `water_shadow_price`, `feasible`).
- **Libraries & functions**:
  - *`scipy` (HiGHS)*: `scipy.optimize.linprog(c=-margins, A_ub=A_ub, b_ub=b_ub, bounds=[(0, u_max)] * 4, method="highs")` — read `res.x` and `res.ineqlin.marginals` *(alternatives: `pulp.LpProblem`, `cvxpy.Problem`)*.

---

## Task 5 — Build Interactive Marimo Decision Dashboard

### How-To
- **Target files**: [python/app.py](./python/app.py), [python/pyproject.toml](./python/pyproject.toml), served via `make marimo` / `make marimo-edit` in [Makefile](./Makefile#L45-L53) *(standalone widget demo remains at [web/demo/index.html](./web/demo/index.html) + [web/demo/demo.ts](./web/demo/demo.ts) via [web/tools/serve.ts](./web/tools/serve.ts))*.
- **Implementation steps**:
  1. Load or fit the cached parcel models (`data/outputs/field_models.json`) once in a top-level marimo setup cell so slider drags only re-evaluate fast in-memory forecast + LP cells.
  2. Organize [python/app.py](./python/app.py) around four linked views alongside the persistent `mo.ui.anywidget(FarmHorizonWidget(...))` canvas:
     - **Parcel selector & coordinate map**: `Field_1`–`Field_90` plotted at `(longitude, latitude)` colored by expected net margin or soil moisture (selectable directly via `mo.ui.altair_chart` / `mo.ui.dropdown`).
     - **Seasonal timeline & date scrubber**: Observed 2023 satellite pass points + smooth 2-harmonic forecast curves (`NDVI`, `soil_moisture`, `temperature`, `rainfall`) with a day-of-year slider $d \in [1, 365]$.
     - **Crop comparison & LP mix panel**: Expected vs. drought net margin bars for all 4 crops alongside optimal LP allocation shares $x ^ {\ast} _ c$ and water shadow price.
     - **Scenario controls**: Growing-season window, $\Delta T$, rainfall shock $\Delta R$, water cap $W _ {\text{avail}}$, max single-crop share $u _ {\max}$, drought floor $R _ {\min}$, and counterfactual crop override.
- **Libraries & functions**:
  - *Reactive UI*: `marimo` (`mo.ui.slider`, `mo.ui.dropdown`, `mo.ui.radio`, `mo.ui.altair_chart`, `mo.ui.anywidget`, `mo.hstack`, `mo.vstack`, `mo.stat`).
  - *Charts & parcel map*: `altair` (`alt.Chart(df).mark_circle()` / `mark_line()` / `mark_bar()` wrapped in `mo.ui.altair_chart` for click/brush selection) or `plotly.express`.

---

## Task 6 — Wire Pipeline State into the WebAssembly Widget

Every reactive update in [python/app.py](./python/app.py) streams through [`FarmHorizonWidget.set_scene`](./python/src/farmhorizon/widget.py#L123-L129) $\to$ [anywidget-entry.ts](./web/widget/anywidget-entry.ts) $\to$ [`FarmHorizonWidget.setState`](./web/widget/farmhorizon-widget.ts#L166-L173) $\to$ [`SetExternalSceneState`](./engine/targets/wasm/main.cpp#L30-L36):
- **`field_size`**: Parcel grid resolution (default `24`).
- **`soil_type`**: Parcel $f$'s inferred [`SoilType`](./engine/include/farmhorizon/core/field_state.hpp#L9-L14) (`"sandy"`, `"loam"`, `"clay"`, `"chernozem"`).
- **`soil_moisture`**: Clamped `[10, 60]` from the scrubbed date $d$ or forecasted window mean $\widehat{M} _ f$.
- **`crop_type`**: Active [`CropType`](./engine/include/farmhorizon/core/field_state.hpp#L16-L21) (`"wheat"`, `"rice"`, `"corn"`, `"soybean"`) — defaults to LP winner $\arg\max _ c x ^ {\ast} _ c$, or whichever crop the user selects to inspect counterfactually.
- **`ndvi`**: Clamped `[0.10, 0.85]` from $\widehat{\text{NDVI}} _ f \cdot S _ c(\widehat{T} _ f, \widehat{W} _ f)$ so poorly suited crops look visibly sparse/stressed.
- **`yield_ext`**: Canopy scale (yield) `[10, 70]` mapped from relative attainment $10 + 60 \cdot (\hat{y} _ {f,c} / Y _ {c,\max})$.
- **`harvested_pct`**: Driven by the date playhead (`0%` pre-harvest $\to$ `0..100%` across the harvest window, animating the boustrophedon Combine Harvester).
- **`time_of_day`**: Sun angle `[6.0, 18.0]` via [`SetTimeOfDay`](./engine/targets/wasm/main.cpp#L38-L40).

### How-To
- **Target files**: [python/app.py](./python/app.py), [widget.py](./python/src/farmhorizon/widget.py), [anywidget-entry.ts](./web/widget/anywidget-entry.ts), [farmhorizon-widget.ts](./web/widget/farmhorizon-widget.ts), [main.cpp](./engine/targets/wasm/main.cpp).
- **Implementation steps**:
  1. Instantiate `scene = mo.ui.anywidget(FarmHorizonWidget(...))` once in an upstream marimo cell so the WebGL context stays mounted across all reactive updates.
  2. Map the active parcel's forecasted state, LP crop selection, and timeline playhead into a single atomic `scene.set_scene(...)` call inside the downstream reactive bridge cell.
- **Libraries & functions**:
  - *Python $\to$ JS comm*: `anywidget.AnyWidget` + `traitlets.Dict(...).tag(sync=True)` in [`FarmHorizonWidget`](./python/src/farmhorizon/widget.py#L84-L157), bundled with `-s SINGLE_FILE=1` via [engine/targets/wasm/Makefile](./engine/targets/wasm/Makefile) and [web/Makefile](./web/Makefile).
  - *Wasm $\to$ C++ engine*: `engine.cwrap("SetExternalSceneState", ...)` in [farmhorizon-widget.ts](./web/widget/farmhorizon-widget.ts#L144) $\to$ invokes [`App::set_external_state`](./engine/src/app/app.cpp#L25-L36) and re-bakes [`FieldRenderer`](./engine/include/farmhorizon/render/field_renderer.hpp#L21-L49) on the next animation frame.
