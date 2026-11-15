"""Python `anywidget` wrapper for the FarmHorizon WebAssembly field renderer."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import anywidget
import traitlets

SOIL_TYPES: tuple[str, ...] = ("sandy", "loam", "clay", "chernozem")
CROP_TYPES: tuple[str, ...] = ("wheat", "rice", "corn", "soybean")

STATE_LIMITS: dict[str, tuple[float, float]] = {
    "fieldSize": (12.0, 64.0),
    "soilMoisture": (10.0, 60.0),
    "ndvi": (0.10, 0.85),
    "yieldExt": (10.0, 70.0),
    "harvestedPct": (0.0, 100.0),
    "timeOfDay": (6.0, 18.0),
}

DEFAULT_SCENE_STATE: dict[str, Any] = {
    "fieldSize": 22,
    "soilType": "loam",
    "soilMoisture": 35.0,
    "cropType": "wheat",
    "ndvi": 0.65,
    "yieldExt": 48.0,
    "harvestedPct": 30.0,
    "timeOfDay": 14.5,
}

_SNAKE_TO_CAMEL: dict[str, str] = {
    "field_size": "fieldSize",
    "soil_type": "soilType",
    "soil_moisture": "soilMoisture",
    "crop_type": "cropType",
    "ndvi": "ndvi",
    "yield_ext": "yieldExt",
    "harvested_pct": "harvestedPct",
    "time_of_day": "timeOfDay",
}

_BUNDLE_PATH = (
    Path(__file__).resolve().parents[3] / "build" / "wasm" / "farmhorizon-anywidget.js"
)


def _clamp(value: float, bounds: tuple[float, float]) -> float:
    lo, hi = bounds
    return max(lo, min(hi, float(value)))


def normalize_scene_state(
    current: dict[str, Any], updates: dict[str, Any]
) -> dict[str, Any]:
    """Merge snake_case or camelCase updates into a validated scene state dict."""
    out = dict(DEFAULT_SCENE_STATE)
    out.update(current)

    for raw_key, raw_val in updates.items():
        if raw_val is None:
            continue
        key = _SNAKE_TO_CAMEL.get(raw_key, raw_key)
        if key not in out:
            raise KeyError(f"Unknown scene state key: {raw_key!r}")
        out[key] = raw_val

    for num_key, bounds in STATE_LIMITS.items():
        out[num_key] = _clamp(float(out[num_key]), bounds)
    out["fieldSize"] = int(round(out["fieldSize"]))

    if out["soilType"] not in SOIL_TYPES:
        raise ValueError(
            f"soil_type must be one of {SOIL_TYPES}, got {out['soilType']!r}"
        )
    if out["cropType"] not in CROP_TYPES:
        raise ValueError(
            f"crop_type must be one of {CROP_TYPES}, got {out['cropType']!r}"
        )
    return out


class FarmHorizonWidget(anywidget.AnyWidget):
    """Embeddable isometric crop-field WebAssembly widget for marimo & Jupyter."""

    _esm = _BUNDLE_PATH

    state = traitlets.Dict(default_value=dict(DEFAULT_SCENE_STATE)).tag(sync=True)
    width = traitlets.Int(760).tag(sync=True)
    height = traitlets.Int(460).tag(sync=True)
    background = traitlets.Unicode("#1e1e23").tag(sync=True)
    show_controls = traitlets.Bool(False).tag(sync=True)
    show_hud = traitlets.Bool(False).tag(sync=True)

    def __init__(
        self,
        *,
        width: int = 760,
        height: int = 460,
        background: str = "#1e1e23",
        show_controls: bool = False,
        show_hud: bool = False,
        **scene_kwargs: Any,
    ) -> None:
        if not _BUNDLE_PATH.is_file():
            raise FileNotFoundError(
                f"Missing Wasm bundle at {_BUNDLE_PATH}. "
                "Run `make wasm` from the project root first."
            )
        initial_state = normalize_scene_state(DEFAULT_SCENE_STATE, scene_kwargs)
        super().__init__(
            state=initial_state,
            width=max(1, int(width)),
            height=max(1, int(height)),
            background=background,
            show_controls=bool(show_controls),
            show_hud=bool(show_hud),
        )

    def set_scene(self, **scene_kwargs: Any) -> dict[str, Any]:
        """Atomically update one or more scene fields and sync to the browser."""
        next_state = normalize_scene_state(self.state, scene_kwargs)
        if next_state != self.state:
            self.state = next_state
        return next_state

    def resize(self, width: int, height: int) -> None:
        """Resize the widget viewport in CSS pixels."""
        w = max(1, int(width))
        h = max(1, int(height))
        if w == self.width and h == self.height:
            return
        with self.hold_sync():
            self.width = w
            self.height = h

    def set_overlays(
        self,
        *,
        show_controls: bool | None = None,
        show_hud: bool | None = None,
        background: str | None = None,
    ) -> None:
        """Toggle in-canvas debug overlays or update the clear colour."""
        with self.hold_sync():
            if show_controls is not None and bool(show_controls) != self.show_controls:
                self.show_controls = bool(show_controls)
            if show_hud is not None and bool(show_hud) != self.show_hud:
                self.show_hud = bool(show_hud)
            if background is not None and background != self.background:
                self.background = background
