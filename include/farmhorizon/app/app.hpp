#pragma once

#include <memory>
#include <string>

#include <raylib.h>

#include "farmhorizon/core/field_grid.hpp"
#include "farmhorizon/core/field_state.hpp"
#include "farmhorizon/geometry/iso_projection.hpp"
#include "farmhorizon/render/field_renderer.hpp"
#include "farmhorizon/ui/control_panel.hpp"

namespace farmhorizon {

/**
 * @brief Platform-agnostic application core.
 *
 * Owns scene state, rendering and UI. Platform entry points only create the
 * window flags, drive the loop by calling frame(), and forward external calls.
 */
class App {
public:
    bool init(const char* title, int width, int height);
    void frame();
    void shutdown();

    /// Bridge for an external math/ML engine pushing new field state.
    void set_external_state(int field_size, int soil_type, float soil_moisture,
                            int crop_type, float ndvi_index, float yield_ext,
                            float harvested_pct);

private:
    void rebuild_grid();
    void rebuild_mesh();
    void update_camera(bool ui_wants_mouse);

    std::unique_ptr<core::FieldGrid> grid_;
    core::FieldGridView grid_view_;
    core::FieldState state_;
    core::FieldState last_state_;
    core::ViewSettings view_;
    float last_time_of_day_ = -1.0f;

    geometry::IsoProjection projection_{};
    render::FieldRenderer renderer_;
    ui::ControlPanel panel_;
    Camera2D camera_{};
    std::string gpu_name_;
};

} // namespace farmhorizon
