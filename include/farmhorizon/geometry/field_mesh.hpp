#pragma once

#include <vector>

#include "farmhorizon/core/field_grid.hpp"
#include "farmhorizon/geometry/iso_projection.hpp"
#include "farmhorizon/geometry/math.hpp"

namespace farmhorizon::geometry {

/// Directional lighting derived from the time of day (6:00 .. 18:00).
struct Lighting {
    float ambient   = 1.0f; ///< Base brightness multiplier
    float sun_dir_x = 0.0f; ///< Horizontal sun component, shades furrow sides

    static Lighting from_time_of_day(float hours) noexcept;
};

/// CPU-side scene geometry, ready for any renderer backend.
struct FieldMesh {
    std::vector<Triangle> triangles; ///< Painter-ordered (back to front), counter-clockwise on screen
    Bounds2 bounds;                  ///< World-pixel bounds of all triangles
    bool has_harvester = false;
    Vec2 harvester_label_anchor;     ///< World-pixel anchor above the harvester cab
};

/**
 * @brief Builds the isometric field mesh: soil furrows, crop prisms, stubble,
 *        soil skirt and the active harvester. Pure CPU work, no rendering.
 */
FieldMesh build_field_mesh(const core::FieldGridView& grid,
                           const IsoProjection& projection,
                           const Lighting& lighting);

} // namespace farmhorizon::geometry
