#pragma once

#include <algorithm>
#include <cmath>

namespace generation {

enum class SoilType : int {
    Sandy     = 0,
    Loam      = 1,
    Clay      = 2,
    Chernozem = 3
};

enum class CropType : int {
    Wheat   = 0,
    Rice    = 1,
    Corn    = 2,
    Soybean = 3
};

struct GenConfig {
    float zoom          = 1.05f; // Camera zoom (0.50 .. 1.80)
    float time_of_day   = 14.5f; // Sun angle in hours (6.0 .. 18.0)
    float field_size    = 22.0f; // Field parcel dimension N x N (12 .. 64)
    float soil_type     = 1.0f;  // 0: Sandy, 1: Loam, 2: Clay, 3: Chernozem
    float soil_moisture = 35.0f; // Soil moisture percentage (10 .. 60)
    float crop_type     = 0.0f;  // 0: Wheat, 1: Rice, 2: Corn, 3: Soybean
    float ndvi_index    = 0.65f; // Vegetation index from satellite/dataset (0.10 .. 0.85)
    float yield_ext     = 48.0f; // Externally calculated yield in t/ha (10 .. 70)
    float harvested_pct = 30.0f; // Percentage of field harvested (0 .. 100)

    bool state_equals(const GenConfig& other) const noexcept {
        return (int)std::round(field_size) == (int)std::round(other.field_size) &&
               (int)std::round(soil_type) == (int)std::round(other.soil_type) &&
               std::abs(soil_moisture - other.soil_moisture) < 0.01f &&
               (int)std::round(crop_type) == (int)std::round(other.crop_type) &&
               std::abs(ndvi_index - other.ndvi_index) < 0.005f &&
               std::abs(yield_ext - other.yield_ext) < 0.05f &&
               std::abs(harvested_pct - other.harvested_pct) < 0.05f;
    }
};

using SceneConfig = GenConfig;

inline const char* get_soil_name(int idx) noexcept {
    switch (std::clamp(idx, 0, 3)) {
        case 0: return "Sandy";
        case 1: return "Loam";
        case 2: return "Clay";
        default: return "Chernoz";
    }
}

inline const char* get_crop_name(int idx) noexcept {
    switch (std::clamp(idx, 0, 3)) {
        case 0: return "Wheat";
        case 1: return "Rice";
        case 2: return "Corn";
        default: return "Soybean";
    }
}

inline void apply_crop_preset(GenConfig& cfg, int preset_idx) noexcept {
    switch (preset_idx % 4) {
        case 0: // Golden Wheat Harvest on Loam
            cfg.soil_type     = 1.0f;
            cfg.soil_moisture = 35.0f;
            cfg.crop_type     = 0.0f;
            cfg.ndvi_index    = 0.65f;
            cfg.yield_ext     = 48.0f;
            cfg.harvested_pct = 30.0f;
            break;
        case 1: // Flooded Rice Paddy on Clay (Field_1 dataset snapshot)
            cfg.soil_type     = 2.0f;
            cfg.soil_moisture = 52.0f;
            cfg.crop_type     = 1.0f;
            cfg.ndvi_index    = 0.78f;
            cfg.yield_ext     = 62.0f;
            cfg.harvested_pct = 0.0f;
            break;
        case 2: // High-Yield Corn on Chernozem
            cfg.soil_type     = 3.0f;
            cfg.soil_moisture = 40.0f;
            cfg.crop_type     = 2.0f;
            cfg.ndvi_index    = 0.80f;
            cfg.yield_ext     = 68.0f;
            cfg.harvested_pct = 45.0f;
            break;
        default: // Drought-Stressed Soybean on Sandy Soil (Field_2 low-moisture snapshot)
            cfg.soil_type     = 0.0f;
            cfg.soil_moisture = 18.0f;
            cfg.crop_type     = 3.0f;
            cfg.ndvi_index    = 0.30f;
            cfg.yield_ext     = 22.0f;
            cfg.harvested_pct = 15.0f;
            break;
    }
}

} // namespace generation
