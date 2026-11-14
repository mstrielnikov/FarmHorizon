#pragma once

#include "farmhorizon/core/field_state.hpp"

namespace farmhorizon::core {

inline constexpr int kPresetCount = 4;

inline void apply_crop_preset(FieldState& state, int preset_idx) noexcept {
    switch (((preset_idx % kPresetCount) + kPresetCount) % kPresetCount) {
        case 0: // Golden Wheat Harvest on Loam
            state.soil_type     = SoilType::Loam;
            state.soil_moisture = 35.0f;
            state.crop_type     = CropType::Wheat;
            state.ndvi_index    = 0.65f;
            state.yield_ext     = 48.0f;
            state.harvested_pct = 30.0f;
            break;
        case 1: // Flooded Rice Paddy on Clay (Field_1 dataset snapshot)
            state.soil_type     = SoilType::Clay;
            state.soil_moisture = 52.0f;
            state.crop_type     = CropType::Rice;
            state.ndvi_index    = 0.78f;
            state.yield_ext     = 62.0f;
            state.harvested_pct = 0.0f;
            break;
        case 2: // High-Yield Corn on Chernozem
            state.soil_type     = SoilType::Chernozem;
            state.soil_moisture = 40.0f;
            state.crop_type     = CropType::Corn;
            state.ndvi_index    = 0.80f;
            state.yield_ext     = 68.0f;
            state.harvested_pct = 45.0f;
            break;
        default: // Drought-Stressed Soybean on Sandy Soil (Field_2 low-moisture snapshot)
            state.soil_type     = SoilType::Sandy;
            state.soil_moisture = 18.0f;
            state.crop_type     = CropType::Soybean;
            state.ndvi_index    = 0.30f;
            state.yield_ext     = 22.0f;
            state.harvested_pct = 15.0f;
            break;
    }
}

} // namespace farmhorizon::core
