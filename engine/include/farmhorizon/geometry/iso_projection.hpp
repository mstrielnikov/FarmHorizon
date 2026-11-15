#pragma once

#include "farmhorizon/geometry/math.hpp"

namespace farmhorizon::geometry {

/**
 * @brief 2:1 isometric projection between grid space (x, y, height) and world pixels.
 *
 *   screen.x = (x - y) * tile_half_w
 *   screen.y = (x + y) * tile_half_h - height * height_scale
 */
struct IsoProjection {
    float tile_half_w  = 32.0f;
    float tile_half_h  = 16.0f;
    float height_scale = 80.0f;

    constexpr Vec2 project(float x, float y, float height) const noexcept {
        return {
            (x - y) * tile_half_w,
            (x + y) * tile_half_h - height * height_scale
        };
    }

    /// Inverse projection onto the plane at `height` (used for picking cells under the cursor).
    constexpr Vec2 unproject(Vec2 screen, float height = 0.0f) const noexcept {
        const float diff = screen.x / tile_half_w;                            // x - y
        const float sum  = (screen.y + height * height_scale) / tile_half_h;  // x + y
        return { (sum + diff) * 0.5f, (sum - diff) * 0.5f };
    }
};

} // namespace farmhorizon::geometry
