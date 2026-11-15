"""Interactive world/regional territory lookup and Plotly map builder.

Designed to accept external data-decision-pipeline outputs (list of dicts or any
pandas/polars DataFrame) directly via `coerce_territory_records(...)`, while
providing a lightweight CSV summary loader so the dashboard map works standalone
before the pipeline branch is merged.
"""

from __future__ import annotations

import csv
import math
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import narwhals as nw
import plotly.graph_objects as go

from farmhorizon.widget import CROP_TYPES, SOIL_TYPES

_DEFAULT_RAW_CSV = (
    Path(__file__).resolve().parents[3]
    / "data"
    / "inputs"
    / "yield_prediction_dataset_raw.csv"
)

# Reference thermal (°C) & moisture (%) optima used only when incoming records
# do not yet carry precomputed pipeline `match_score` / crop suitability values.
_CROP_CLIMATE_OPTIMA: dict[str, tuple[float, float]] = {
    "wheat": (18.5, 34.0),
    "rice": (27.5, 46.0),
    "corn": (24.0, 39.0),
    "soybean": (25.0, 29.0),
}


@dataclass(frozen=True)
class TerritoryMatchResult:
    """Matched vs. unmatched dataset territories and their coordinate bounds."""

    matched: list[dict[str, Any]]
    unmatched: list[dict[str, Any]]
    lat_range: tuple[float, float] | None
    lon_range: tuple[float, float] | None


def load_raw_parcel_records(csv_path: Path | str | None = None) -> list[dict[str, Any]]:
    """Aggregate per-parcel coordinates and baseline means from the raw CSV.

    Serves as a zero-pipeline fallback until the data decision pipeline supplies
    cleaned, forecasted, and LP-scored parcel records.
    """
    path = Path(csv_path) if csv_path is not None else _DEFAULT_RAW_CSV
    if not path.is_file():
        return []

    grouped: dict[str, dict[str, Any]] = {}
    with path.open(newline="", encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            fid = row["field_id"]
            moist = max(0.0, min(100.0, float(row["soil_moisture"])))
            rain = max(0.0, float(row["rainfall"]))
            ndvi = float(row["NDVI"])
            temp = float(row["temperature"])
            yld = float(row["yield"])

            acc = grouped.get(fid)
            if acc is None:
                grouped[fid] = {
                    "field_id": fid,
                    "latitude": float(row["latitude"]),
                    "longitude": float(row["longitude"]),
                    "n": 1,
                    "moist_sum": moist,
                    "rlog_sum": math.log1p(rain),
                    "ndvi_sum": ndvi,
                    "temp_sum": temp,
                    "yield_sum": yld,
                }
            else:
                acc["n"] += 1
                acc["moist_sum"] += moist
                acc["rlog_sum"] += math.log1p(rain)
                acc["ndvi_sum"] += ndvi
                acc["temp_sum"] += temp
                acc["yield_sum"] += yld

    # Rank retention ratio across fields to assign a balanced 4-tier SoilType proxy.
    retentions = sorted(
        (a["moist_sum"] / a["n"]) / max(0.25, a["rlog_sum"] / a["n"])
        for a in grouped.values()
    )
    q1 = retentions[len(retentions) // 4]
    q2 = retentions[len(retentions) // 2]
    q3 = retentions[(3 * len(retentions)) // 4]

    records: list[dict[str, Any]] = []
    for acc in grouped.values():
        n = acc["n"]
        moist_mean = acc["moist_sum"] / n
        rlog_mean = acc["rlog_sum"] / n
        ndvi_mean = acc["ndvi_sum"] / n
        temp_mean = acc["temp_sum"] / n
        yield_mean = acc["yield_sum"] / n
        retention = moist_mean / max(0.25, rlog_mean)

        if retention < q1:
            soil = "sandy"
        elif retention < q2:
            soil = "loam"
        elif retention >= q3 and ndvi_mean >= 0.45:
            soil = "chernozem"
        elif retention >= q2:
            soil = "clay"
        else:
            soil = "loam"

        records.append(
            {
                "field_id": acc["field_id"],
                "latitude": acc["latitude"],
                "longitude": acc["longitude"],
                "soil_type": soil,
                "soil_moisture": round(max(10.0, min(60.0, moist_mean)), 2),
                "ndvi": round(max(0.10, min(0.85, ndvi_mean)), 3),
                "yield_ext": round(max(10.0, min(70.0, yield_mean)), 1),
                "temperature": round(temp_mean, 1),
            }
        )
    return records


def coerce_territory_records(data: Any = None) -> list[dict[str, Any]]:
    """Normalize pipeline DataFrames or dict sequences into canonical parcel dicts.

    Accepted input shapes:
    - `None`: loads baseline summaries from `data/inputs/yield_prediction_dataset_raw.csv`.
    - `Sequence[Mapping[str, Any]]`: list/tuple of parcel dicts from the pipeline.
    - Any DataFrame supported by `narwhals` (`pandas`, `polars`, `pyarrow`, etc.).
    """
    if data is None:
        return load_raw_parcel_records()

    raw_rows: Sequence[Mapping[str, Any]]
    if isinstance(data, Sequence) and (not data or isinstance(data[0], Mapping)):
        raw_rows = data  # type: ignore[assignment]
    else:
        df = nw.from_native(data, eager_only=True)
        cols = df.columns
        raw_rows = [
            dict(zip(cols, row)) for row in df.iter_rows(named=False)
        ]

    normalized: list[dict[str, Any]] = []
    for idx, row in enumerate(raw_rows):
        lat = float(row.get("latitude", row.get("lat", 0.0)))
        lon = float(row.get("longitude", row.get("lon", 0.0)))
        soil_raw = str(row.get("soil_type", row.get("soilType", "loam"))).lower()
        soil = soil_raw if soil_raw in SOIL_TYPES else "loam"

        crop_raw = row.get("recommended_crop", row.get("crop_type", row.get("cropType")))
        crop = str(crop_raw).lower() if crop_raw is not None else None
        if crop not in CROP_TYPES:
            crop = None

        rec: dict[str, Any] = {
            "field_id": str(row.get("field_id", f"Field_{idx + 1}")),
            "latitude": lat,
            "longitude": lon,
            "soil_type": soil,
            "soil_moisture": float(row.get("soil_moisture", row.get("soilMoisture", 35.0))),
            "ndvi": float(row.get("ndvi", row.get("NDVI", 0.50))),
            "yield_ext": float(
                row.get("yield_ext", row.get("yieldExt", row.get("yield", 40.0)))
            ),
        }
        if crop is not None:
            rec["crop_type"] = crop
        if "temperature" in row and row["temperature"] is not None:
            rec["temperature"] = float(row["temperature"])
        if "match_score" in row and row["match_score"] is not None:
            rec["match_score"] = float(row["match_score"])
        if "expected_profit" in row and row["expected_profit"] is not None:
            rec["expected_profit"] = float(row["expected_profit"])
        normalized.append(rec)

    return normalized


def match_territories(
    records: Sequence[Mapping[str, Any]],
    *,
    crop_type: str,
    soil_type: str,
    soil_moisture: float,
    ndvi: float,
    yield_ext: float,
    min_score: float = 0.68,
) -> TerritoryMatchResult:
    """Filter and rank dataset parcels matching the prompted simulation state."""
    target_soil_idx = SOIL_TYPES.index(soil_type) if soil_type in SOIL_TYPES else 1
    opt_temp, opt_moist = _CROP_CLIMATE_OPTIMA.get(crop_type, (23.0, 36.0))

    matched: list[dict[str, Any]] = []
    unmatched: list[dict[str, Any]] = []

    for raw in records:
        rec = dict(raw)
        if "match_score" in rec and rec["match_score"] is not None:
            score = max(0.0, min(1.0, float(rec["match_score"])))
        else:
            moist_sim = max(0.0, 1.0 - abs(rec["soil_moisture"] - soil_moisture) / 35.0)
            ndvi_sim = max(0.0, 1.0 - abs(rec["ndvi"] - ndvi) / 0.55)
            yield_sim = max(0.0, 1.0 - abs(rec["yield_ext"] - yield_ext) / 45.0)

            rec_soil = rec.get("soil_type", "loam")
            rec_soil_idx = SOIL_TYPES.index(rec_soil) if rec_soil in SOIL_TYPES else 1
            soil_sim = 1.0 - (abs(rec_soil_idx - target_soil_idx) / 3.0) * 0.75

            if rec.get("crop_type") in CROP_TYPES:
                crop_sim = 1.0 if rec["crop_type"] == crop_type else 0.45
            else:
                temp = float(rec.get("temperature", opt_temp))
                temp_sim = max(0.0, 1.0 - abs(temp - opt_temp) / 14.0)
                crop_moist_sim = max(
                    0.0, 1.0 - abs(rec["soil_moisture"] - opt_moist) / 32.0
                )
                crop_sim = 0.65 * temp_sim + 0.35 * crop_moist_sim

            score = (
                0.28 * crop_sim
                + 0.24 * moist_sim
                + 0.20 * soil_sim
                + 0.15 * ndvi_sim
                + 0.13 * yield_sim
            )

        rec["match_score"] = round(score, 3)
        if score >= min_score:
            matched.append(rec)
        else:
            unmatched.append(rec)

    matched.sort(key=lambda r: r["match_score"], reverse=True)

    if matched:
        lats = [r["latitude"] for r in matched]
        lons = [r["longitude"] for r in matched]
        lat_range: tuple[float, float] | None = (min(lats), max(lats))
        lon_range: tuple[float, float] | None = (min(lons), max(lons))
    else:
        lat_range = None
        lon_range = None

    return TerritoryMatchResult(
        matched=matched,
        unmatched=unmatched,
        lat_range=lat_range,
        lon_range=lon_range,
    )


def _hover_label(rec: Mapping[str, Any]) -> str:
    temp_part = (
        f" · Temp {rec['temperature']:.1f}°C" if "temperature" in rec else ""
    )
    profit_part = (
        f"<br>Expected margin: ${rec['expected_profit']:.0f}/ha"
        if "expected_profit" in rec
        else ""
    )
    return (
        f"<b>{rec['field_id']}</b> ({rec['latitude']:.2f}°, {rec['longitude']:.2f}°)<br>"
        f"Soil: {str(rec['soil_type']).title()} · Moisture {rec['soil_moisture']:.0f}%"
        f"{temp_part}<br>"
        f"NDVI: {rec['ndvi']:.2f} · Canopy/Yield: {rec['yield_ext']:.0f} · "
        f"Match: {rec['match_score'] * 100:.0f}%"
        f"{profit_part}"
    )


def build_territory_figure(
    result: TerritoryMatchResult,
    *,
    scope: str = "world",
) -> go.Figure:
    """Create an interactive Plotly world/regional map highlighting matched territories."""
    fig = go.Figure()

    # 1. Unmatched dataset parcels for geographic context.
    if result.unmatched:
        fig.add_trace(
            go.Scattergeo(
                lon=[r["longitude"] for r in result.unmatched],
                lat=[r["latitude"] for r in result.unmatched],
                customdata=[r["field_id"] for r in result.unmatched],
                hovertext=[_hover_label(r) for r in result.unmatched],
                hoverinfo="text",
                mode="markers",
                marker=dict(size=6, color="#94a3b8", opacity=0.45),
                name=f"Other parcels ({len(result.unmatched)})",
            )
        )

    # 2. Matched territories coloured by similarity / pipeline score.
    if result.matched:
        fig.add_trace(
            go.Scattergeo(
                lon=[r["longitude"] for r in result.matched],
                lat=[r["latitude"] for r in result.matched],
                customdata=[r["field_id"] for r in result.matched],
                hovertext=[_hover_label(r) for r in result.matched],
                hoverinfo="text",
                mode="markers",
                marker=dict(
                    size=11,
                    color=[r["match_score"] * 100.0 for r in result.matched],
                    colorscale="YlOrRd",
                    cmin=55,
                    cmax=100,
                    line=dict(width=1.1, color="#1f2937"),
                    colorbar=dict(title="Match %", thickness=12, len=0.75),
                ),
                name=f"Matched territories ({len(result.matched)})",
            )
        )

    all_records = result.matched + result.unmatched
    geo_kwargs: dict[str, Any] = dict(
        projection_type="natural earth",
        showcountries=True,
        countrycolor="#cbd5e1",
        showsubunits=True,
        subunitcolor="#e2e8f0",
        showcoastlines=True,
        coastlinecolor="#cbd5e1",
        showland=True,
        landcolor="#f8fafc",
        showocean=True,
        oceancolor="#eff6ff",
        showlakes=True,
        lakecolor="#dbeafe",
    )

    if scope == "region" and all_records:
        focus = result.matched if result.matched else all_records
        min_lat = min(r["latitude"] for r in focus)
        max_lat = max(r["latitude"] for r in focus)
        min_lon = min(r["longitude"] for r in focus)
        max_lon = max(r["longitude"] for r in focus)
        pad_lat = max(4.0, (max_lat - min_lat) * 0.25)
        pad_lon = max(5.0, (max_lon - min_lon) * 0.25)
        geo_kwargs["lataxis_range"] = [
            max(-85.0, min_lat - pad_lat),
            min(85.0, max_lat + pad_lat),
        ]
        geo_kwargs["lonaxis_range"] = [
            max(-180.0, min_lon - pad_lon),
            min(180.0, max_lon + pad_lon),
        ]

    fig.update_geos(**geo_kwargs)
    fig.update_layout(
        margin=dict(l=0, r=0, t=0, b=0),
        height=400,
        legend=dict(
            orientation="h",
            yanchor="bottom",
            y=0.01,
            xanchor="left",
            x=0.01,
            bgcolor="rgba(255,255,255,0.82)",
        ),
        clickmode="event+select",
        dragmode="pan",
    )
    return fig


def resolve_selected_parcels(
    selected_points: Sequence[Mapping[str, Any]] | None,
    records: Sequence[Mapping[str, Any]],
) -> list[dict[str, Any]]:
    """Map `mo.ui.plotly` clicked/lassoed points back to full parcel dicts."""
    if not selected_points or not records:
        return []

    by_id = {str(r["field_id"]): dict(r) for r in records}
    resolved: list[dict[str, Any]] = []
    seen: set[str] = set()

    for pt in selected_points:
        cid = pt.get("customdata")
        if isinstance(cid, str) and cid in by_id:
            if cid not in seen:
                seen.add(cid)
                resolved.append(by_id[cid])
            continue

        lat = pt.get("lat")
        lon = pt.get("lon")
        if isinstance(lat, (int, float)) and isinstance(lon, (int, float)):
            best = min(
                records,
                key=lambda r: (r["latitude"] - float(lat)) ** 2
                + (r["longitude"] - float(lon)) ** 2,
            )
            dist_sq = (best["latitude"] - float(lat)) ** 2 + (
                best["longitude"] - float(lon)
            ) ** 2
            fid = str(best["field_id"])
            if dist_sq < 1e-3 and fid not in seen:
                seen.add(fid)
                resolved.append(dict(best))

    return resolved
