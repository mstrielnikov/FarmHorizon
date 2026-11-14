#include "farmhorizon/core/field_grid.hpp"

#include <algorithm>
#include <cmath>

namespace farmhorizon::core {

void populate_field_grid(FieldGrid& grid, const FieldState& state) noexcept {
    const int n = static_cast<int>(grid.width());
    if (n < 3 || static_cast<int>(grid.height()) != n) return;

    // 1. Soil base RGB palette by SoilType, darkened by soil_moisture
    static constexpr uint8_t kSoilBaseRgb[4][3] = {
        { 168, 138, 94 }, // Sandy
        { 118,  88, 58 }, // Loam
        { 135,  78, 52 }, // Clay
        {  62,  52, 46 }  // Chernozem
    };
    const int soil_idx = std::clamp(static_cast<int>(state.soil_type), 0, 3);
    const float moist_factor = 1.0f - ((std::clamp(state.soil_moisture, 10.0f, 60.0f) - 10.0f) / 75.0f) * 0.42f;
    const uint8_t base_sr = static_cast<uint8_t>(std::clamp(std::round(kSoilBaseRgb[soil_idx][0] * moist_factor), 0.0f, 255.0f));
    const uint8_t base_sg = static_cast<uint8_t>(std::clamp(std::round(kSoilBaseRgb[soil_idx][1] * moist_factor), 0.0f, 255.0f));
    const uint8_t base_sb = static_cast<uint8_t>(std::clamp(std::round(kSoilBaseRgb[soil_idx][2] * moist_factor), 0.0f, 255.0f));

    // 2. Crop canopy palette by CropType, interpolated by NDVI health
    struct CropPalette {
        uint8_t low[3];
        uint8_t high[3];
    };
    static constexpr CropPalette kCropPalettes[4] = {
        { { 140, 155, 75 }, { 228, 190, 68 } }, // Wheat (golden harvest)
        { {  95, 150, 80 }, {  65, 185, 75 } }, // Rice (lush paddy)
        { { 110, 150, 65 }, {  48, 145, 55 } }, // Corn (deep green)
        { { 120, 145, 70 }, {  75, 165, 62 } }  // Soybean
    };
    const int crop_idx = std::clamp(static_cast<int>(state.crop_type), 0, 3);
    const float ndvi_norm  = std::clamp((state.ndvi_index - 0.10f) / 0.75f, 0.0f, 1.0f);
    const float yield_norm = std::clamp((state.yield_ext - 10.0f) / 60.0f, 0.0f, 1.0f);

    const auto& pal = kCropPalettes[crop_idx];
    const uint8_t cr = static_cast<uint8_t>(std::round(pal.low[0] + (pal.high[0] - pal.low[0]) * ndvi_norm));
    const uint8_t cg = static_cast<uint8_t>(std::round(pal.low[1] + (pal.high[1] - pal.low[1]) * ndvi_norm));
    const uint8_t cb = static_cast<uint8_t>(std::round(pal.low[2] + (pal.high[2] - pal.low[2]) * ndvi_norm));

    // 3. Boustrophedon (zig-zag) harvest threshold
    const int interior_w = n - 2;
    const int total_plot_cells = interior_w * interior_w;
    const int harvested_limit = static_cast<int>(
        std::floor((std::clamp(state.harvested_pct, 0.0f, 100.0f) / 100.0f) * total_plot_cells));

    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            Cell& cell = grid(x, y);
            const bool is_border = (x == 0 || y == 0 || x == n - 1 || y == n - 1);

            // Subtle plough furrow wave along x (no mountain terrain)
            cell.height = (x % 2 == 0) ? 0.025f : -0.025f;

            cell.r = base_sr;
            cell.g = base_sg;
            cell.b = base_sb;

            if (is_border) {
                cell.role = CellRole::BorderHeadland;
                cell.crop_height = 0.0f;
                cell.canopy_density = 0.0f;
                cell.crop_r = base_sr;
                cell.crop_g = base_sg;
                cell.crop_b = base_sb;
                continue;
            }

            const int row_idx = y - 1;
            const int col_in_row = (row_idx % 2 == 0) ? (x - 1) : ((interior_w - 1) - (x - 1));
            const int seq_idx = row_idx * interior_w + col_in_row;

            if (state.harvested_pct > 0.0f && state.harvested_pct < 100.0f && seq_idx == harvested_limit) {
                cell.role = CellRole::HarvesterActive;
                cell.crop_height = 0.06f;
                cell.canopy_density = 0.45f;
                cell.crop_r = 175;
                cell.crop_g = 145;
                cell.crop_b = 70;
            } else if (seq_idx < harvested_limit) {
                cell.role = CellRole::HarvestedStubble;
                cell.crop_height = 0.05f;
                cell.canopy_density = 0.42f;
                cell.crop_r = 175;
                cell.crop_g = 145;
                cell.crop_b = 70;
            } else {
                cell.role = CellRole::StandingCrop;
                // Flooded rice paddy water tint in furrows when high moisture
                if (state.crop_type == CropType::Rice && state.soil_moisture > 38.0f) {
                    cell.r = static_cast<uint8_t>(std::clamp(base_sr * 0.7f + 35.0f, 0.0f, 255.0f));
                    cell.g = static_cast<uint8_t>(std::clamp(base_sg * 0.8f + 65.0f, 0.0f, 255.0f));
                    cell.b = static_cast<uint8_t>(std::clamp(base_sb * 0.8f + 80.0f, 0.0f, 255.0f));
                }
                // Deterministic subtle per-cell organic variation
                uint32_t hsh = static_cast<uint32_t>(x * 374761393U + y * 668265263U);
                hsh = (hsh ^ (hsh >> 13)) * 1274126177U;
                const float cell_var = 0.90f + static_cast<float>(hsh & 0xffff) / 65535.0f * 0.20f;

                cell.crop_height    = (0.10f + yield_norm * 0.48f) * cell_var;
                cell.canopy_density = std::clamp(0.35f + yield_norm * 0.42f + ndvi_norm * 0.22f, 0.28f, 0.94f);
                cell.crop_r = cr;
                cell.crop_g = cg;
                cell.crop_b = cb;
            }
        }
    }
}

} // namespace farmhorizon::core
