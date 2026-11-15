#pragma once

#include <algorithm>

#include "farmhorizon/geometry/math.hpp"

namespace farmhorizon::geometry {

/// Screen-space rectangle available to the scene, in pixels.
struct Viewport {
    float x      = 0.0f;
    float y      = 0.0f;
    float width  = 0.0f;
    float height = 0.0f;

    bool operator==(const Viewport&) const = default;
};

/**
 * @brief Uniform world -> screen mapping that fits some world bounds into a viewport.
 *
 * Replaces a free camera: the scene is always centred and scaled so its whole
 * bounding box is visible, growing or shrinking together with the field.
 * `scale == 0` means there is nothing to fit (empty bounds or viewport).
 */
struct ViewFit {
    float scale = 0.0f;
    Vec2 world_center;
    Vec2 screen_center;

    bool valid() const noexcept { return scale > 0.0f; }

    constexpr Vec2 to_screen(Vec2 p) const noexcept {
        return {
            (p.x - world_center.x) * scale + screen_center.x,
            (p.y - world_center.y) * scale + screen_center.y
        };
    }
};

/// Fits `world` into `viewport`, keeping the aspect ratio and `padding` pixels on every side.
inline ViewFit fit_bounds(const Bounds2& world, const Viewport& viewport, float padding) noexcept {
    ViewFit fit;
    if (!world.valid || viewport.width <= 0.0f || viewport.height <= 0.0f) return fit;

    const float avail_w = std::max(1.0f, viewport.width  - 2.0f * padding);
    const float avail_h = std::max(1.0f, viewport.height - 2.0f * padding);
    const float world_w = std::max(world.width(),  1e-6f);
    const float world_h = std::max(world.height(), 1e-6f);

    fit.scale         = std::min(avail_w / world_w, avail_h / world_h);
    fit.world_center  = { (world.min.x + world.max.x) * 0.5f, (world.min.y + world.max.y) * 0.5f };
    fit.screen_center = { viewport.x + viewport.width * 0.5f, viewport.y + viewport.height * 0.5f };
    return fit;
}

} // namespace farmhorizon::geometry
