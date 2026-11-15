#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace farmhorizon::core {

enum class SoilType : uint8_t {
    Sandy     = 0,
    Loam      = 1,
    Clay      = 2,
    Chernozem = 3
};

enum class CropType : uint8_t {
    Wheat   = 0,
    Rice    = 1,
    Corn    = 2,
    Soybean = 3
};

constexpr SoilType soil_type_from_index(int idx) noexcept {
    return static_cast<SoilType>(std::clamp(idx, 0, 3));
}

constexpr CropType crop_type_from_index(int idx) noexcept {
    return static_cast<CropType>(std::clamp(idx, 0, 3));
}

constexpr const char* soil_name(SoilType soil) noexcept {
    switch (soil) {
        case SoilType::Sandy:     return "Sandy";
        case SoilType::Loam:      return "Loam";
        case SoilType::Clay:      return "Clay";
        case SoilType::Chernozem: return "Chernoz";
    }
    return "Chernoz";
}

constexpr const char* crop_name(CropType crop) noexcept {
    switch (crop) {
        case CropType::Wheat:   return "Wheat";
        case CropType::Rice:    return "Rice";
        case CropType::Corn:    return "Corn";
        case CropType::Soybean: return "Soybean";
    }
    return "Soybean";
}

/**
 * @brief Externally supplied agricultural field parameters.
 *
 * Pure data model — contains no UI widget state or camera/lighting settings.
 */
struct FieldState {
    int      field_size    = 22;              ///< Parcel dimension N x N (12 .. 64)
    SoilType soil_type     = SoilType::Loam;  ///< Base soil classification
    float    soil_moisture = 35.0f;           ///< Soil moisture percentage (10 .. 60)
    CropType crop_type     = CropType::Wheat; ///< Planted crop species
    float    ndvi_index    = 0.65f;           ///< Satellite vegetation index (0.10 .. 0.85)
    float    yield_ext     = 48.0f;           ///< Externally calculated yield in t/ha (10 .. 70)
    float    harvested_pct = 30.0f;           ///< Percentage of field harvested (0 .. 100)

    bool approx_equals(const FieldState& other) const noexcept {
        return field_size == other.field_size &&
               soil_type  == other.soil_type &&
               crop_type  == other.crop_type &&
               std::abs(soil_moisture - other.soil_moisture) < 0.01f &&
               std::abs(ndvi_index - other.ndvi_index) < 0.005f &&
               std::abs(yield_ext - other.yield_ext) < 0.05f &&
               std::abs(harvested_pct - other.harvested_pct) < 0.05f;
    }
};

/**
 * @brief Scene presentation settings independent of the underlying field data.
 */
struct ViewSettings {
    float time_of_day = 14.5f; ///< Sun angle in hours (6.0 .. 18.0)
};

} // namespace farmhorizon::core
