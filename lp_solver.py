"""
Crop allocation LP solver (Python / SciPy HiGHS).

Install:
    pip install numpy scipy

The function solve_crop_allocation() maximizes expected net margin for
Wheat, Rice, Corn and Soybean subject to land, water, crop-share and
drought-profit constraints.

All crop yields, prices, costs, water requirements, water availability
and profit floor must use consistent units (e.g. per hectare).
"""

from __future__ import annotations

from typing import Any, Mapping
import numpy as np
from scipy.optimize import linprog


CROP_NAMES = ("Wheat", "Rice", "Corn", "Soybean")


def solve_crop_allocation(
    crops: Mapping[str, Mapping[str, float]],
    water_available: float,
    u_max: float = 1.0,
    drought_profit_floor: float = 0.0,
    shadow_price_method: str = "dual",
    finite_difference_delta: float = 1e-4,
) -> dict[str, Any]:
    """
    Solve the four-crop land allocation LP.

    Parameters
    ----------
    crops:
        Mapping with exactly these crop keys:
        Wheat, Rice, Corn, Soybean.
        Each crop must contain:
          price          : crop price (USD / yield unit)
          expected_yield : expected yield per hectare
          drought_yield  : drought-stressed yield per hectare
          cost           : production cost per hectare (USD / ha)
          water          : water requirement per hectare
    water_available:
        Available irrigation water per hectare-equivalent allocation.
    u_max:
        Maximum land share for each crop, between 0.40 and 1.00.
    drought_profit_floor:
        Minimum total drought-case net margin (R_min).
    shadow_price_method:
        "dual" (HiGHS dual marginal) or "finite_difference".
    finite_difference_delta:
        Water perturbation for finite-difference shadow price.

    Returns
    -------
    dict with:
        shares, expectedProfit, droughtProfit, waterUsed,
        waterShadowPrice, feasible, status, message
    """

    if not 0.40 <= u_max <= 1.00:
        raise ValueError("u_max must be between 0.40 and 1.00.")
    if water_available < 0:
        raise ValueError("water_available cannot be negative.")
    if shadow_price_method not in {"dual", "finite_difference"}:
        raise ValueError(
            "shadow_price_method must be 'dual' or 'finite_difference'."
        )
    if finite_difference_delta <= 0:
        raise ValueError("finite_difference_delta must be positive.")

    missing = set(CROP_NAMES) - set(crops)
    if missing:
        raise ValueError(f"Missing crop inputs: {sorted(missing)}")

    prices = []
    expected_yields = []
    drought_yields = []
    costs = []
    water = []

    for crop in CROP_NAMES:
        item = crops[crop]
        required = (
            "price", "expected_yield", "drought_yield", "cost", "water"
        )
        absent = set(required) - set(item)
        if absent:
            raise ValueError(f"{crop} missing fields: {sorted(absent)}")

        values = [float(item[k]) for k in required]
        if not np.all(np.isfinite(values)):
            raise ValueError(f"{crop} contains non-finite values.")
        if values[4] < 0:
            raise ValueError(f"{crop} water requirement cannot be negative.")

        price, expected_yield, drought_yield, cost, water_need = values
        prices.append(price)
        expected_yields.append(expected_yield)
        drought_yields.append(drought_yield)
        costs.append(cost)
        water.append(water_need)

    prices = np.asarray(prices, dtype=float)
    expected_yields = np.asarray(expected_yields, dtype=float)
    drought_yields = np.asarray(drought_yields, dtype=float)
    costs = np.asarray(costs, dtype=float)
    water = np.asarray(water, dtype=float)

    # Per-crop net margins (USD / ha).
    expected_margins = prices * expected_yields - costs
    drought_margins = prices * drought_yields - costs

    # scipy.optimize.linprog minimizes, so negate expected profit.
    # A_ub @ x <= b_ub:
    #   land:          sum(x) <= 1
    #   water:         sum(w_c * x_c) <= W_available
    #   drought floor: -sum(drought_margin_c * x_c) <= -R_min
    A_ub = np.vstack([
        np.ones(4),
        water,
        -drought_margins,
    ])
    b_ub = np.array([
        1.0,
        float(water_available),
        -float(drought_profit_floor),
    ])

    bounds = [(0.0, float(u_max))] * 4

    def run_lp(water_budget: float):
        rhs = b_ub.copy()
        rhs[1] = water_budget
        return linprog(
            c=-expected_margins,
            A_ub=A_ub,
            b_ub=rhs,
            bounds=bounds,
            method="highs",
        )

    result = run_lp(float(water_available))

    if not result.success:
        return {
            "shares": {crop: 0.0 for crop in CROP_NAMES},
            "expectedProfit": None,
            "droughtProfit": None,
            "waterUsed": 0.0,
            "waterShadowPrice": None,
            "feasible": False,
            "status": int(result.status),
            "message": result.message,
        }

    x = np.asarray(result.x, dtype=float)
    expected_profit = float(expected_margins @ x)
    drought_profit = float(drought_margins @ x)
    water_used = float(water @ x)

    # Dual marginal is d(min objective)/d(W). Since min objective = -profit,
    # the profit shadow price is the negative of that marginal.
    if shadow_price_method == "dual":
        water_shadow_price = float(
            max(0.0, -result.ineqlin.marginals[1])
        )
    else:
        perturbed = run_lp(float(water_available) + finite_difference_delta)
        if perturbed.success:
            extra_profit = float(expected_margins @ perturbed.x)
            water_shadow_price = max(
                0.0,
                (extra_profit - expected_profit) / finite_difference_delta,
            )
        else:
            water_shadow_price = None

    # Numerical feasibility checks.
    feasible = bool(
        x.min() >= -1e-7
        and x.max() <= u_max + 1e-7
        and x.sum() <= 1.0 + 1e-7
        and water_used <= water_available + 1e-7
        and drought_profit >= drought_profit_floor - 1e-7
    )

    return {
        "shares": {
            crop: float(x[i]) for i, crop in enumerate(CROP_NAMES)
        },
        "expectedProfit": expected_profit,
        "droughtProfit": drought_profit,
        "waterUsed": water_used,
        "waterShadowPrice": water_shadow_price,
        "feasible": feasible,
        "status": int(result.status),
        "message": result.message,
    }


if __name__ == "__main__":
    # Illustrative example only; replace these assumptions with your
    # predicted yields, prices, costs and water requirements.
    example_crops = {
        "Wheat": {
            "price": 250, "expected_yield": 4.0,
            "drought_yield": 2.8, "cost": 400, "water": 2.0,
        },
        "Rice": {
            "price": 220, "expected_yield": 5.0,
            "drought_yield": 2.5, "cost": 500, "water": 4.0,
        },
        "Corn": {
            "price": 200, "expected_yield": 6.0,
            "drought_yield": 4.0, "cost": 450, "water": 3.0,
        },
        "Soybean": {
            "price": 500, "expected_yield": 2.5,
            "drought_yield": 1.8, "cost": 350, "water": 1.5,
        },
    }

    solution = solve_crop_allocation(
        crops=example_crops,
        water_available=2.5,
        u_max=0.60,
        drought_profit_floor=100,
        shadow_price_method="dual",
    )

    print("Crop allocation solution:")
    for crop, share in solution["shares"].items():
        print(f"  {crop:8s}: {share:.4f} ({share * 100:.2f}%)")

    print(f"Expected profit:    {solution['expectedProfit']}")
    print(f"Drought profit:     {solution['droughtProfit']}")
    print(f"Water used:         {solution['waterUsed']}")
    print(f"Water shadow price: {solution['waterShadowPrice']}")
    print(f"Feasible:           {solution['feasible']}")
    print(f"Solver status:      {solution['message']}")
