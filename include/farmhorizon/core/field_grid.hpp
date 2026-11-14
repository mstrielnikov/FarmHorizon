#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "farmhorizon/core/field_state.hpp"

namespace farmhorizon::core {

enum class CellRole : uint8_t {
    BorderHeadland = 0,
    StandingCrop,
    HarvestedStubble,
    HarvesterActive
};

struct Cell {
    CellRole role = CellRole::StandingCrop;
    float height = 0.0f;                          ///< Subtle plough furrow elevation only
    float crop_height = 0.0f;                     ///< Extruded crop canopy height driven by external yield
    float canopy_density = 0.0f;                  ///< Row canopy fill ratio (0.28 .. 0.94)
    uint8_t r = 118, g = 88, b = 58;              ///< Base soil RGB colour
    uint8_t crop_r = 200, crop_g = 175, crop_b = 70; ///< Crop canopy / stubble RGB colour
};

/**
 * @brief Read-only view of a FieldGrid for geometry and rendering systems.
 */
class FieldGridView {
public:
    FieldGridView() = default;
    FieldGridView(size_t w, size_t h, std::span<const Cell> c) : width_(w), height_(h), cells_(c) {}

    size_t width()  const noexcept { return width_; }
    size_t height() const noexcept { return height_; }

    const Cell& operator()(size_t x, size_t y) const noexcept {
        return cells_[y * width_ + x];
    }

private:
    size_t width_ = 0;
    size_t height_ = 0;
    std::span<const Cell> cells_;
};

/**
 * @brief 2D Field Grid storing externally parametrized crop and soil cell states.
 */
class FieldGrid {
public:
    static std::optional<FieldGrid> create(size_t width, size_t height) noexcept {
        if (width == 0 || height == 0) return std::nullopt;
        try {
            return FieldGrid(width, height);
        } catch (...) {
            return std::nullopt;
        }
    }

    size_t width()  const noexcept { return width_; }
    size_t height() const noexcept { return height_; }
    size_t size()   const noexcept { return cells_.size(); }
    bool empty()    const noexcept { return cells_.empty(); }

    FieldGridView view() const noexcept {
        return FieldGridView(width_, height_, std::span<const Cell>(cells_));
    }

    Cell& operator()(size_t x, size_t y) noexcept {
        return cells_[y * width_ + x];
    }
    const Cell& operator()(size_t x, size_t y) const noexcept {
        return cells_[y * width_ + x];
    }

    std::vector<Cell>& cells() noexcept { return cells_; }
    const std::vector<Cell>& cells() const noexcept { return cells_; }

    FieldGrid(const FieldGrid&) = delete;
    FieldGrid& operator=(const FieldGrid&) = delete;

    FieldGrid(FieldGrid&&) noexcept = default;
    FieldGrid& operator=(FieldGrid&&) noexcept = default;

    size_t memory_usage() const noexcept {
        return cells_.size() * sizeof(Cell);
    }

private:
    size_t width_;
    size_t height_;
    std::vector<Cell> cells_;

    explicit FieldGrid(size_t width, size_t height)
        : width_(width), height_(height), cells_(width * height) {}
};

/**
 * @brief Maps externally provided FieldState parameters onto the FieldGrid cells.
 * Performs no simulation math — only assigns visual state per cell.
 */
void populate_field_grid(FieldGrid& grid, const FieldState& state) noexcept;

} // namespace farmhorizon::core
