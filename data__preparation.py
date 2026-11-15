#!/usr/bin/env python3
"""
Clean and normalize yield_prediction_dataset_raw.csv.

Usage:
    python tools/build_data_pipeline.py \
        --input yield_prediction_dataset_raw.csv \
        --output data/yield_prediction_clean.csv \
        --stats-output data/normalization_stats.json \
        --parcel-output data/parcel_soil_types.csv

The script:
- removes empty/duplicate-derived columns;
- repairs ambiguous DD-MM-YYYY / MM-DD-YYYY dates in original row order
  within each field, enforcing strictly increasing dates;
- clamps soil moisture and computes log1p rainfall;
- infers parcel soil classes from retention ratio and baseline NDVI;
- creates a deterministic field-level train/test split and fits z-score
  statistics on training fields only (avoids leakage);
- keeps crop_type as descriptive metadata, but excludes it from MODEL_FEATURES.
"""
from __future__ import annotations

import argparse
import json
from datetime import date
from pathlib import Path

import numpy as np
import pandas as pd

DROP_COLUMNS = ["Unnamed: 13", "Unnamed: 14", "NDWI", "SAVI"]
STANDARDIZE_COLUMNS = ["NDVI", "soil_moisture", "temperature", "r_log"]
MODEL_FEATURES = [
    "NDVI_z", "soil_moisture_z", "temperature_z", "r_log_z",
    "GNDVI", "latitude", "longitude", "day_of_year", "SoilType",
]
SOIL_NAMES = {0: "Sandy", 1: "Loam", 2: "Clay", 3: "Chernozem"}


def repair_field_dates(values: pd.Series, field_name: str) -> list[date]:
    """Choose earliest valid candidate later than previous date, preserving row order."""
    repaired: list[date] = []
    previous: date | None = None

    for row_number, raw in enumerate(values, start=1):
        parts = str(raw).strip().split("-")
        if len(parts) != 3:
            raise ValueError(f"{field_name}, row {row_number}: invalid date {raw!r}")
        try:
            a, b, year = (int(part) for part in parts)
        except ValueError as exc:
            raise ValueError(
                f"{field_name}, row {row_number}: invalid date {raw!r}"
            ) from exc

        candidates: set[date] = set()
        # DD-MM-YYYY candidate
        try:
            candidates.add(date(year, b, a))
        except ValueError:
            pass

        # MM-DD-YYYY candidate is allowed only by the stated ambiguity rule.
        if a <= 12 and a != b:
            try:
                candidates.add(date(year, a, b))
            except ValueError:
                pass

        valid = sorted(d for d in candidates if previous is None or d > previous)
        if not valid:
            raise ValueError(
                f"{field_name}, row {row_number}: no valid date after "
                f"{previous}; source value was {raw!r}"
            )
        chosen = valid[0]
        repaired.append(chosen)
        previous = chosen

    return repaired


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", default="yield_prediction_dataset_raw.csv")
    parser.add_argument("--output", default="yield_prediction_clean.csv")
    parser.add_argument("--stats-output", default="normalization_stats.json")
    parser.add_argument("--parcel-output", default="parcel_soil_types.csv")
    parser.add_argument("--test-fraction", type=float, default=0.20)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    if not 0.0 < args.test_fraction < 1.0:
        parser.error("--test-fraction must be between 0 and 1")
