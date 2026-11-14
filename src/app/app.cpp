#include "farmhorizon/app/app.hpp"

#include <algorithm>
#include <iostream>
#include <utility>

#include "farmhorizon/geometry/field_mesh.hpp"
#include "farmhorizon/platform/diagnostics.hpp"
#include "farmhorizon/ui/hud.hpp"

namespace farmhorizon {

namespace {

constexpr Camera2D kDefaultCamera = {
    .offset   = { 540, 140 },
    .target   = { 0, 0 },
    .rotation = 0.0f,
    .zoom     = 1.05f
};
constexpr float kMinZoom = 0.5f;
constexpr float kMaxZoom = 1.8f;
constexpr Color kBackground = { 30, 30, 35, 255 };

} // namespace

void App::set_external_state(
    int field_size, int soil_type, float soil_moisture,
    int crop_type, float ndvi_index, float yield_ext, float harvested_pct
) {
    state_.field_size    = std::clamp(field_size, 12, 64);
    state_.soil_type     = core::soil_type_from_index(soil_type);
    state_.soil_moisture = std::clamp(soil_moisture, 10.0f, 60.0f);
    state_.crop_type     = core::crop_type_from_index(crop_type);
    state_.ndvi_index    = std::clamp(ndvi_index, 0.10f, 0.85f);
    state_.yield_ext     = std::clamp(yield_ext, 10.0f, 70.0f);
    state_.harvested_pct = std::clamp(harvested_pct, 0.0f, 100.0f);
}

bool App::init(const char* title, int width, int height) {
    InitWindow(width, height, title);
    if (!IsWindowReady()) return false;

    gpu_name_ = platform::get_gpu_name(); // Query once, not every frame
    camera_ = kDefaultCamera;
    rebuild_grid();
    return true;
}

void App::shutdown() {
    renderer_.release();
    CloseWindow();
}

void App::frame() {
    // 1. Input: the UI gets first claim on the mouse.
    update_camera(panel_.wants_mouse());

    // 2. Rebuild grid and/or off-screen mesh outside BeginDrawing() when dirty.
    if (!grid_ || !state_.approx_equals(last_state_)) {
        rebuild_grid();
    }
    if (view_.time_of_day != last_time_of_day_) {
        rebuild_mesh();
    }

    BeginDrawing();
        ClearBackground(kBackground);

        // 3. World pass.
        renderer_.draw(camera_);

        // 4. Screen-space overlay pass.
        ui::draw_hud(state_, gpu_name_.c_str());
        const ui::PanelResult panel = panel_.draw(state_, view_, camera_.zoom);
        if (panel.reset_view) {
            camera_.zoom   = kDefaultCamera.zoom;
            camera_.target = kDefaultCamera.target;
        }
    EndDrawing();
}

void App::rebuild_grid() {
    const int size = std::max(4, state_.field_size);
    auto grid = core::FieldGrid::create(static_cast<size_t>(size), static_cast<size_t>(size));
    if (!grid.has_value()) {
        std::cerr << "Failed to allocate FieldGrid\n";
        return;
    }
    grid_ = std::make_unique<core::FieldGrid>(std::move(*grid));
    core::populate_field_grid(*grid_, state_);
    grid_view_ = grid_->view();

    last_state_ = state_;
    last_time_of_day_ = -1.0f; // Force mesh bake before drawing
}

void App::rebuild_mesh() {
    renderer_.upload(geometry::build_field_mesh(
        grid_view_,
        projection_,
        geometry::Lighting::from_time_of_day(view_.time_of_day)
    ));
    last_time_of_day_ = view_.time_of_day;
}

void App::update_camera(bool ui_wants_mouse) {
    if (ui_wants_mouse) return;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        const Vector2 delta = GetMouseDelta();
        camera_.target.x -= delta.x / camera_.zoom;
        camera_.target.y -= delta.y / camera_.zoom;
    }
    camera_.zoom = std::clamp(camera_.zoom + GetMouseWheelMove() * 0.05f, kMinZoom, kMaxZoom);
}

} // namespace farmhorizon
