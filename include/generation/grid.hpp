#pragma once
#include <vector>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <cmath>
#include <algorithm>
#include "generation_config.hpp"

namespace generation {

enum class CellRole : uint8_t {
    BorderHeadland = 0,
    StandingCrop,
    HarvestedStubble,
    HarvesterActive
};

struct Cell {
    CellRole role = CellRole::StandingCrop;
    float height = 0.0f;         // Subtle plough furrow elevation only (no mountains)
    float crop_height = 0.0f;    // Extruded crop canopy height driven by external yield
    float canopy_density = 0.0f; // Row canopy fill ratio (0.25 .. 0.96)
    uint8_t r = 118, g = 88, b = 58;          // Base soil color
    uint8_t crop_r = 200, crop_g = 175, crop_b = 70; // Crop canopy / stubble color
};

/**
 * @brief Read-only view of a Field Grid for rendering systems without transferring ownership.
 */
struct GridView {
private:
    size_t width_;
    size_t height_;
    std::span<const Cell> cells_;

public:
    GridView() : width_(0), height_(0), cells_() {}
    GridView(size_t w, size_t h, std::span<const Cell> c) : width_(w), height_(h), cells_(c) {}

    size_t width() const noexcept { return width_; }
    size_t height() const noexcept { return height_; }
    
    const Cell& operator()(size_t x, size_t y) const noexcept {
        return cells_[y * width_ + x];
    }
};

/**
 * @brief 2D Field Grid structure storing externally parametrized crop & soil cell states.
 */
struct Grid {
private:
    size_t width_;
    size_t height_;
    std::vector<Cell> cells_;

    explicit Grid(size_t width, size_t height)
        : width_(width), height_(height) {
        cells_.resize(width_ * height_);
    }

public:
    static std::optional<Grid> create(size_t width, size_t height) noexcept {
        if (width == 0 || height == 0) return std::nullopt;
        try {
            return Grid(width, height);
        } catch (...) {
            return std::nullopt;
        }
    }

    size_t width()  const noexcept { return width_; }
    size_t height() const noexcept { return height_; }
    size_t size()   const noexcept { return cells_.size(); } 
    bool empty()    const noexcept { return cells_.empty(); }

    GridView view() const noexcept {
        return GridView(width_, height_, std::span<const Cell>(cells_));
    }

    Cell& operator()(size_t x, size_t y) noexcept {
        return cells_[y * width_ + x];
    }
    const Cell& operator()(size_t x, size_t y) const noexcept {
        return cells_[y * width_ + x];
    }

    std::vector<Cell>& cells() noexcept { return cells_; }
    const std::vector<Cell>& cells() const noexcept { return cells_; }

    Grid(const Grid&) = delete;            
    Grid& operator=(const Grid&) = delete;

    Grid(Grid&&) noexcept = default;
    Grid& operator=(Grid&&) noexcept = default;

    size_t memory_usage() const noexcept {
        return cells_.size() * sizeof(Cell);
    }
};

/**
 * @brief Maps externally provided SceneConfig parameters onto the Field Grid cells.
 * Performs no simulation math — only assigns visual state per cell.
 */
inline void populate_field_grid(Grid& grid, const GenConfig& cfg) noexcept {
    const int N = (int)grid.width();
    if (N < 3) return;

    // 1. Soil base RGB palette by SoilType, darkened by soil_moisture
    static constexpr uint8_t SOIL_BASE_RGB[4][3] = {
        {168, 138, 94}, // Sandy
        {118,  88, 58}, // Loam
        {135,  78, 52}, // Clay
        { 62,  52, 46}  // Chernozem
    };
    const int soil_idx = std::clamp((int)std::round(cfg.soil_type), 0, 3);
    const float moist_factor = 1.0f - (( std::clamp(cfg.soil_moisture, 10.0f, 60.0f) - 10.0f ) / 75.0f) * 0.42f;
    const uint8_t base_sr = (uint8_t)std::clamp(std::round(SOIL_BASE_RGB[soil_idx][0] * moist_factor), 0.0f, 255.0f);
    const uint8_t base_sg = (uint8_t)std::clamp(std::round(SOIL_BASE_RGB[soil_idx][1] * moist_factor), 0.0f, 255.0f);
    const uint8_t base_sb = (uint8_t)std::clamp(std::round(SOIL_BASE_RGB[soil_idx][2] * moist_factor), 0.0f, 255.0f);

    // 2. Crop canopy palette by CropType, interpolated by NDVI health
    struct CropPalette {
        uint8_t low[3];
        uint8_t high[3];
    };
    static constexpr CropPalette CROP_PALETTES[4] = {
        { {140, 155, 75}, {228, 190, 68} }, // Wheat (golden harvest)
        { { 95, 150, 80}, { 65, 185, 75} }, // Rice (lush paddy)
        { {110, 150, 65}, { 48, 145, 55} }, // Corn (deep green)
        { {120, 145, 70}, { 75, 165, 62} }  // Soybean
    };
    const int crop_idx = std::clamp((int)std::round(cfg.crop_type), 0, 3);
    const float ndvi_norm = std::clamp((cfg.ndvi_index - 0.10f) / 0.75f, 0.0f, 1.0f);
    const float yield_norm = std::clamp((cfg.yield_ext - 10.0f) / 60.0f, 0.0f, 1.0f);

    const auto& pal = CROP_PALETTES[crop_idx];
    const uint8_t cr = (uint8_t)std::round(pal.low[0] + (pal.high[0] - pal.low[0]) * ndvi_norm);
    const uint8_t cg = (uint8_t)std::round(pal.low[1] + (pal.high[1] - pal.low[1]) * ndvi_norm);
    const uint8_t cb = (uint8_t)std::round(pal.low[2] + (pal.high[2] - pal.low[2]) * ndvi_norm);

    // 3. Boustrophedon (zig-zag) harvest threshold
    const int interior_w = N - 2;
    const int total_plot_cells = interior_w * interior_w;
    const int harvested_limit = (int)std::floor((std::clamp(cfg.harvested_pct, 0.0f, 100.0f) / 100.0f) * total_plot_cells);

    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            Cell& cell = grid(x, y);
            const bool is_border = (x == 0 || y == 0 || x == N - 1 || y == N - 1);

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

            if (cfg.harvested_pct > 0.0f && cfg.harvested_pct < 100.0f && seq_idx == harvested_limit) {
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
                if (crop_idx == 1 && cfg.soil_moisture > 38.0f) {
                    cell.r = (uint8_t)std::clamp(base_sr * 0.7f + 35.0f, 0.0f, 255.0f);
                    cell.g = (uint8_t)std::clamp(base_sg * 0.8f + 65.0f, 0.0f, 255.0f);
                    cell.b = (uint8_t)std::clamp(base_sb * 0.8f + 80.0f, 0.0f, 255.0f);
                }
                // Deterministic subtle per-cell organic variation (not procedural terrain)
                uint32_t hsh = (uint32_t)(x * 374761393U + y * 668265263U);
                hsh = (hsh ^ (hsh >> 13)) * 1274126177U;
                float cell_var = 0.90f + (float)(hsh & 0xffff) / 65535.0f * 0.20f;

                cell.crop_height = (0.10f + yield_norm * 0.48f) * cell_var;
                cell.canopy_density = std::clamp(0.35f + yield_norm * 0.42f + ndvi_norm * 0.22f, 0.28f, 0.94f);
                cell.crop_r = cr;
                cell.crop_g = cg;
                cell.crop_b = cb;
            }
        }
    }
}

} // namespace generation
