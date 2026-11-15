# FarmHorizon Data & Crop-Selection Decision Pipeline

This document concludes our data investigation on [yield_prediction_dataset_raw.csv](file:///home/max/projects/time2farm/yield_prediction_dataset_raw.csv) and [crop-yield-prediction-notebook.ipynb](file:///home/max/projects/time2farm/crop-yield-prediction-notebook.ipynb), organizing the end-to-end **Forecast + Ridge Regression + Linear Programming (LP)** workflow into **6 implementation tasks** with detailed how-tos and library/function references.

---

## Pipeline Overview

1. **Task 1 — Clean, Repair Dates & Normalize Raw CSV**: Drop empty/collinear columns from [yield_prediction_dataset_raw.csv](file:///home/max/projects/time2farm/yield_prediction_dataset_raw.csv) (`1,625` rows, `90` parcels), repair `322` swapped dates, normalize features, and infer parcel [`SoilType`](file:///home/max/projects/time2farm/include/farmhorizon/core/field_state.hpp#L9-L14).
2. **Task 2 — Fit Per-Parcel Seasonal Harmonic Forecasters**: Fit 2-harmonic Fourier Ridge curves per parcel and evaluate seasonal expectations + weather-shock scenarios $(\Delta T, \Delta R\%)$.
3. **Task 3 — Fit Quadratic Ridge Productivity & Crop Suitability Model**: Fit global standardized quadratic Ridge weights ($R^2 = 0.823$), export static `build-wasm/field_models.json`, and compute crop-specific yields $\hat{y}_{f,c}$ (`t/ha`).
4. **Task 4 — Solve Crop Allocation via Linear Programming (LP)**: Optimize land shares $x^*_c$ and water shadow price maximizing expected net margin under land, water budget, rotation cap, and drought-floor constraints.
5. **Task 5 — Build Interactive Web Decision Dashboard**: Render the split-pane parcel map, 2023 satellite + forecast timeline scrubber, crop comparison bars, and LP controls alongside the WebGL canvas.
6. **Task 6 — Wire WebAssembly Scene Bridge into `FarmHorizon`**: Drive [`SetExternalSceneState`](file:///home/max/projects/time2farm/apps/web/main.cpp#L9-L15) at 60 FPS from every dashboard interaction.

> [!NOTE]
> **Local toolchain**: `bun 1.3.10` and standalone `./python/bin/python3` (`3.11`) are already installed in the workspace (`numpy`/`pandas`/`scikit-learn`/`scipy` are not preinstalled in `./python`). Offline tasks (`Tasks 1–3`) list **both** standard `scikit-learn`/`scipy` calls (via `.venv`) and **zero-install Python stdlib** equivalents, while runtime evaluation in the browser (`< 0.2 ms` per update) needs zero external ML dependencies.

---

## Task 1 — Clean, Repair Dates & Normalize Raw CSV

Verified across all `1,625` rows and `90` parcels (`Field_1`–`Field_90`) of [yield_prediction_dataset_raw.csv](file:///home/max/projects/time2farm/yield_prediction_dataset_raw.csv):

| Action | Columns Affected | Rule & Empirical Verification |
| :--- | :--- | :--- |
| **Drop empty & collinear** | `Unnamed: 13`, `Unnamed: 14`, `NDWI`, `SAVI` | `Unnamed: 13–14` are 100% empty; $\text{NDWI} \equiv -\text{GNDVI}$ ($r = -1.00$); $\text{SAVI} \equiv 1.5\,\text{NDVI}$ ($r = +1.00$). |
| **Exclude raw `crop_type` from regression** | `crop_type` | Stamped sequentially 3 fields per crop (`Field_1..3 = Rice`, `Field_4..6 = Wheat`, ..., `Field_88..90 = Oil Palm`) with zero causal effect on `yield` (5-fold `GroupKFold` $R^2$ drops from `0.445` to `0.426` when included). |
| **Repair swapped dates** | `date_of_image` | Rows per `field_id` follow a chronological ~15-day pass cadence, but `322` rows with `day <= 12` had day/month swapped (`MM-DD-YYYY` vs `DD-MM-YYYY`). Selecting the earliest valid candidate $> t_{\text{prev}}$ yields **`0` non-monotonic dates** across all `1,625` rows. |
| **Clamp & log-transform** | `soil_moisture`, `rainfall` | Clamp `soil_moisture` to `[0, 100]` (fixes row `1112` at `102.13%`). Transform `r_log = ln(1 + rainfall)` to tame `+4.51` right-skew without discarding real monsoon storms. |
| **Z-score standardize** | `NDVI`, `soil_moisture`, `temperature`, `r_log` | Compute training $(\mu_j, \sigma_j)$ so large-variance columns ($\sigma^2_{\text{moist}} \approx 192$) do not crush `NDVI` ($\sigma^2 \approx 0.048$) under L2 regularization. |
| **Infer parcel `SoilType`** | `soil_moisture`, `rainfall`, `NDVI` | Classify each parcel's [`SoilType`](file:///home/max/projects/time2farm/include/farmhorizon/core/field_state.hpp#L9-L14) (`0..3`: `Sandy`, `Loam`, `Clay`, `Chernozem`) from its water-retention ratio $\overline{\text{moist}}_f / \overline{\ln(1+\text{rain})}_f$ and baseline organic vigor $\overline{\text{NDVI}}_f$. |

### How-To
- **Target file**: `tools/build_data_pipeline.py` (reads [yield_prediction_dataset_raw.csv](file:///home/max/projects/time2farm/yield_prediction_dataset_raw.csv)).
- **Implementation steps**:
  1. Load rows grouped by `field_id` preserving file row order, omitting `Unnamed: 13`, `Unnamed: 14`, `NDWI`, and `SAVI`.
  2. For each `date_of_image` string `A-B-YYYY`, generate valid date candidates `(YYYY, B, A)` (`DD-MM-YYYY`) and, when `A <= 12` and `A != B`, `(YYYY, A, B)` (`MM-DD-YYYY`); pick `min(d for d in candidates if d > prev_date)` and convert to day-of-year $d \in [1, 365]$.
  3. Apply `clamp(soil_moisture, 0, 100)` and `r_log = ln(1 + rainfall)`, then bucket each field's mean retention & `NDVI` into `SoilType` `0..3`.
- **Libraries & functions**:
  - *`pandas` / `numpy`*: `pd.read_csv(..., usecols=...)`, `pd.to_datetime(..., format="%d-%m-%Y", errors="coerce")`, `Series.dt.dayofyear`, `Series.clip(0.0, 100.0)`, `np.log1p()`, `DataFrame.groupby("field_id", sort=False)`.
  - *Zero-dep Python stdlib*: `csv.DictReader`, `datetime.date(y, m, d)`, `date.timetuple().tm_yday`, `math.log1p()`.

---

## Task 2 — Fit Per-Parcel Seasonal Harmonic Forecasters

For each parcel $f \in \{1..90\}$, fit a 2-harmonic Fourier Ridge regression ($\lambda = 0.5$) over day-of-year $d \in [1, 365]$ ($\omega = 2\pi d / 365$):

$$\hat{z}_f(d) = \beta_{f,0} + \beta_{f,1}\sin\omega + \beta_{f,2}\cos\omega + \beta_{f,3}\sin 2\omega + \beta_{f,4}\cos 2\omega$$

**Verified goodness-of-fit across all 90 date-repaired fields**:
- **`soil_moisture(d)`**: $R^2 = \mathbf{0.900}$ ($\text{RMSE} = 4.39\%$)
- **`NDVI(d)`**: $R^2 = \mathbf{0.892}$ ($\text{RMSE} = 0.072$)
- **`temperature(d)`**: $R^2 = \mathbf{0.714}$ ($\text{RMSE} = 5.26^\circ\text{C}$)
- **`ln(1 + rainfall)(d)`**: $R^2 = \mathbf{0.380}$ ($\text{RMSE} = 0.33$ log-mm; residual std $\sigma_{f,\text{rain}}$ quantifies parcel rainfall volatility)

Given a growing-season window $[d_{\text{start}}, d_{\text{end}}]$ and user weather-shock sliders ($\Delta T \in [-5, +5]^\circ\text{C}$, $\Delta R\% \in [-50\%, +50\%]$), the forecaster outputs seasonal parcel expectations $(\widehat{T}_f, \widehat{M}_f, \widehat{R}_f, \widehat{\text{NDVI}}_f)$ plus a **drought stress scenario** $(\widehat{T}_f + 2^\circ\text{C},\, 0.75\widehat{M}_f,\, \exp(\hat{r}_{\log,f} - \sigma_{f,\text{rain}}) - 1)$.

### How-To
- **Target files**: `tools/build_data_pipeline.py` (offline coefficient fit) + `apps/web/dashboard/forecast.ts` (browser evaluator).
- **Implementation steps**:
  1. Build the $N_f \times 5$ design matrix $X_f = [1, \sin\omega, \cos\omega, \sin 2\omega, \cos 2\omega]$ per parcel (leaving intercept $\beta_{f,0}$ unpenalized).
  2. Solve for $\beta_f$ on `temperature`, `soil_moisture`, `NDVI`, and `r_log`, and record $\sigma_{f,\text{rain}} = \operatorname{std}(r_{\log} - X_f\beta_{f,\text{rain}})$.
  3. In `forecast.ts`, sample $\hat{z}_f(d)$ across $[d_{\text{start}}, d_{\text{end}}]$ to compute both daily timeline curves and seasonal window means with $(\Delta T, \Delta R\%)$ applied.
- **Libraries & functions**:
  - *`scikit-learn` / `statsmodels`*: `statsmodels.tsa.deterministic.CalendarFourier(freq="YE", order=2)` or `np.column_stack([np.sin(w), np.cos(w), ...])` + `sklearn.linear_model.Ridge(alpha=0.5).fit(X, y)` + `np.std(residuals, ddof=1)`.
  - *Zero-dep Python / TS*: $5 \times 5$ normal-equation solver $(X^\top X + \lambda I_0)^{-1} X^\top y$ in Python; `Math.sin()` / `Math.cos()` dot-product in TypeScript.

---

## Task 3 — Fit Quadratic Ridge Productivity & Crop Suitability Model

### 3a. Empirical Parcel Productivity Index ($\pi_f$)
A global quadratic Ridge model ($\lambda = 1.0$) trained on z-scored features $[z_{\text{ndvi}}, z_{\text{moist}}, z_{\text{temp}}, z_{\text{rain}}, z_{\text{ndvi}}^2, z_{\text{rain}}^2, z_{\text{ndvi}} z_{\text{rain}}]$ predicts raw parcel productivity $\hat{y}_{\text{raw}, f}$ (**5-fold `GroupKFold(field_id)` $R^2 = 0.823$**, up from `0.722` unnormalized), mapped to a dimensionless parcel index:

$$\pi_f = \operatorname{clamp}\!\left(\frac{\hat{y}_{\text{raw}, f} - 26.0}{60.0 - 26.0},\; 0.15,\; 1.00\right)$$

### 3b. Crop-Specific Yield Response ($\hat{y}_{f,c}$)
Each [`CropType`](file:///home/max/projects/time2farm/include/farmhorizon/core/field_state.hpp#L16-L21) $c \in \{\text{Wheat}, \text{Rice}, \text{Corn}, \text{Soybean}\}$ applies a quadratic thermal & moisture suitability envelope $S_c(\widehat{T}_f, \widehat{W}_f) \in [0.1, 1.0]$ and reference ceiling $Y_{c,\max}$ (`t/ha`):

$$\hat{y}_{f,c} = Y_{c,\max} \cdot \pi_f \cdot S_c(\widehat{T}_f, \widehat{W}_f)$$

| Crop (`CropType`) | $Y_{c,\max}$ (`t/ha`) | Optimal Temp $T^*_c$ (°C) | Water Need $w_c$ (index) | Price $p_c$ (`USD/t`) | Input Cost $C_c$ (`USD/ha`) | Niche Where It Wins |
| :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **Wheat** (`0`) | `6.5` | `16°C` (`8–24°C`) | `0.45` | `\$260` | `\$520` | Cool/winter (`Rabi`) parcels & moderate water |
| **Rice** (`1`) | `7.5` | `27°C` (`20–34°C`) | `0.85` | `\$310` | `\$780` | Warm, high-moisture / monsoon-fed clay & paddy parcels |
| **Corn / Maize** (`2`) | `10.5` | `24°C` (`16–31°C`) | `0.65` | `\$210` | `\$740` | High-productivity loams/chernozems with reliable moisture |
| **Soybean** (`3`) | `3.8` | `25°C` (`17–32°C`) | `0.38` | `\$440` | `\$410` | Tight water budgets, sandy/drier soils, or low-risk floors |

### How-To
- **Target files**: `tools/build_data_pipeline.py` $\to$ `build-wasm/field_models.json` (~120 KB) + `apps/web/dashboard/yield_model.ts`.
- **Implementation steps**:
  1. Assemble degree-2 features, verify 5-fold `GroupKFold(field_id)` $R^2$, fit final `(mean, scale, coef, intercept)` on all `1,625` rows, and serialize alongside the 90 parcel records into `build-wasm/field_models.json`.
  2. In `yield_model.ts`, evaluate $\pi_f$ via z-scored dot-product and compute both expected $\hat{y}_{f,c}$ and drought-stressed $\hat{y}^{\text{drought}}_{f,c}$ for all 4 crops.
- **Libraries & functions**:
  - *`scikit-learn`*: `sklearn.pipeline.make_pipeline(PolynomialFeatures(degree=2, include_bias=False), StandardScaler(), RidgeCV(alphas=[0.1, 1.0, 10.0]))`, `sklearn.model_selection.GroupKFold(n_splits=5)`, `cross_val_score(..., groups=df["field_id"], scoring="r2")`, `json.dump()`.
  - *Zero-dep Python / TS*: Explicit 7-feature standardization + Ridge normal equations in Python; pure array dot-product in TypeScript.

---

## Task 4 — Solve Crop Allocation via Linear Programming (LP)

Given expected crop yields $\hat{y}_{f,c}$ and drought-stressed yields $\hat{y}^{\text{drought}}_{f,c}$ on parcel $f$, solve for land shares $x = (x_{\text{Wheat}}, x_{\text{Rice}}, x_{\text{Corn}}, x_{\text{Soybean}})^\top$:

$$\max_{x \in \mathbb{R}^4_{\ge 0}} \sum_{c=0}^{3} \bigl(p_c \hat{y}_{f,c} - C_c\bigr)\, x_c$$

subject to:
1. **Land constraint**: $\sum_{c=0}^{3} x_c \le 1$ (allows fallow share if extreme shock makes margins negative).
2. **Water availability constraint**: $\sum_{c=0}^{3} w_c x_c \le W_{\text{avail}}(\widehat{M}_f, \widehat{R}_f)$.
3. **Diversification / rotation cap**: $0 \le x_c \le u_{\max}$ (slider `0.40 .. 1.00`; at `1.00`, picks single best crop unless water or risk binds).
4. **Drought downside floor**: $\sum_{c=0}^{3} \bigl(p_c \hat{y}^{\text{drought}}_{f,c} - C_c\bigr)\, x_c \ge R_{\min}$.

> [!TIP]
> Because $x \in \mathbb{R}^4$, solving the LP also yields the **shadow price of water** ($\partial \text{Profit}^* / \partial W_{\text{avail}}$), showing directly how much `USD/ha` additional irrigation is worth on `Field_k`.

### How-To
- **Target file**: `apps/web/dashboard/lp_solver.ts` (plus optional cross-check test in Python).
- **Implementation steps**:
  1. Compute expected net margins $m_c = p_c \hat{y}_{f,c} - C_c$ and drought margins $m^{\text{drought}}_c = p_c \hat{y}^{\text{drought}}_{f,c} - C_c$.
  2. Solve the bounded 4-variable LP and compute water shadow price via dual variable (or finite-difference perturbation $\Delta W = 10^{-4}$).
  3. Return `{ shares, expectedProfit, droughtProfit, waterUsed, waterShadowPrice, feasible }`.
- **Libraries & functions**:
  - *Python verification (`scipy` HiGHS)*: `scipy.optimize.linprog(c=-margins, A_ub=A_ub, b_ub=b_ub, bounds=[(0, u_max)] * 4, method="highs")` — read `res.x` and `res.ineqlin.marginals` *(alternatives: `pulp.LpProblem`, `cvxpy.Problem`)*.
  - *Browser TS*: Either `javascript-lp-solver` (`solver.Solve(model)`), `highs` (`highs-js` Wasm), or a ~60-line exact two-phase Simplex / active-set solver in pure TypeScript (`< 0.1 ms` per slider drag, zero bundle weight).

---

## Task 5 — Build Interactive Web Decision Dashboard

### How-To
- **Target files**: `apps/web/shell.html`, `apps/web/dashboard/app.ts`, [tools/serve.ts](file:///home/max/projects/time2farm/tools/serve.ts).
- **Implementation steps**:
  1. Create a responsive split-pane layout in `apps/web/shell.html`: WebGL `FarmHorizon` `<canvas id="canvas">` on the left, analytical decision workbench on the right.
  2. Load `field_models.json` on startup and render four linked views:
     - **Parcel selector & map**: `Field_1`–`Field_90` plotted at `(longitude, latitude)` colored by net margin or moisture.
     - **Seasonal timeline & date scrubber**: Observed 2023 satellite dots + smooth 2-harmonic forecast curves (`NDVI`, `soil_moisture`, `temperature`, `rainfall`) with an interactive day-of-year playhead.
     - **Crop comparison & LP mix panel**: Expected vs. drought net margin bars per crop (clickable to inspect any crop counterfactually in 3D) alongside optimal LP allocation shares $x^*_c$ and water shadow price.
     - **Scenario controls**: Growing-season selector, $\Delta T$, $\Delta R\%$, water cap $W_{\text{avail}}$, max single-crop share $u_{\max}$, and drought floor $R_{\min}$.
  3. Bundle TypeScript with Bun during `make wasm` (or on startup in [tools/serve.ts](file:///home/max/projects/time2farm/tools/serve.ts)).
- **Libraries & functions**:
  - *Bundler & server*: `Bun.build({ entrypoints: ["apps/web/dashboard/app.ts"], outdir: "build-wasm" })` and `Bun.serve()` in [tools/serve.ts](file:///home/max/projects/time2farm/tools/serve.ts) (serving `.wasm`, `.js`, `.json`).
  - *Charts*: **uPlot** or **Chart.js** (`new Chart(ctx, { type: "line" | "bar", ... })`), or lightweight zero-dep Canvas/SVG components.
  - *Map*: **Leaflet** (`L.map`, `L.circleMarker([lat, lon])`) or an offline SVG coordinate scatter map of the 90 parcels.

---

## Task 6 — Wire WebAssembly Scene Bridge into `FarmHorizon`

Every dashboard state update maps onto [`SetExternalSceneState`](file:///home/max/projects/time2farm/apps/web/main.cpp#L9-L15):
- **`field_size`**: Parcel grid resolution (default `24`).
- **`soil_type`**: Parcel $f$'s inferred [`SoilType`](file:///home/max/projects/time2farm/include/farmhorizon/core/field_state.hpp#L9-L14) (`0..3`).
- **`soil_moisture`**: Clamped `[10, 60]` from the scrubbed date $d$ or forecasted window mean $\widehat{M}_f$.
- **`crop_type`**: Active crop (`0..3`) — defaults to LP winner $\arg\max_c x^*_c$, or whichever crop bar the user clicks to inspect.
- **`ndvi_index`**: Clamped `[0.10, 0.85]` from $\widehat{\text{NDVI}}_f \cdot S_c(\widehat{T}_f, \widehat{W}_f)$ so poorly suited crops look visibly sparse/stressed.
- **`yield_ext`**: Canopy-height proxy `[10, 70]` mapped from relative attainment $10 + 60 \cdot (\hat{y}_{f,c} / Y_{c,\max})$.
- **`harvested_pct`**: Driven by the date playhead (`0%` pre-harvest $\to$ `0..100%` across the harvest window, animating the boustrophedon Combine Harvester).

### How-To
- **Target files**: [apps/web/Makefile](file:///home/max/projects/time2farm/apps/web/Makefile#L36-L50), [apps/web/main.cpp](file:///home/max/projects/time2farm/apps/web/main.cpp#L9-L15), `apps/web/dashboard/wasm_bridge.ts`.
- **Implementation steps**:
  1. Switch `--shell-file` in [apps/web/Makefile](file:///home/max/projects/time2farm/apps/web/Makefile#L39) from `$(RAYLIB_SRC)/shell.html` to `shell.html` (`apps/web/shell.html`) and run `tools/build_data_pipeline.py` + `Bun.build` as part of `make wasm`.
  2. Export `cwrap`/`ccall` in Emscripten `LDFLAGS` and bind `SetExternalSceneState` once `Module.onRuntimeInitialized` fires (queuing any initial state generated before Wasm finishes loading).
- **Libraries & functions**:
  - *Emscripten linker flags*: `-s EXPORTED_FUNCTIONS='["_main","_SetExternalSceneState"]'` and `-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'`.
  - *JS/TS bridge*: `const setScene = Module.cwrap("SetExternalSceneState", null, ["number","number","number","number","number","number","number"]);` $\to$ invokes [`App::set_external_state`](file:///home/max/projects/time2farm/src/app/app.cpp#L27-L38) and re-bakes [`FieldRenderer`](file:///home/max/projects/time2farm/include/farmhorizon/render/field_renderer.hpp#L19-L46) on the next frame.
