#include "farmhorizon/app/app.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>

#include "farmhorizon/platform/diagnostics.hpp"
#include "farmhorizon/ui/hud.hpp"

namespace farmhorizon {

namespace {

constexpr float kFitPadding    = 16.0f;  ///< Logical pixels kept free around the fitted field
constexpr float kPanelGap      = 12.0f;  ///< Gap between the field and the control panel
constexpr float kMinSceneWidth = 240.0f; ///< Below this, let the panel overlay the field instead

int to_device_pixels(int logical, float ratio) {
    return std::max(1, static_cast<int>(std::lround(static_cast<float>(logical) * ratio)));
}

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

void App::set_time_of_day(float hours) {
    view_.time_of_day = std::clamp(hours, 6.0f, 18.0f);
}

void App::resize(int width, int height, float pixel_ratio) {
    options_.pixel_ratio = std::clamp(pixel_ratio, 0.5f, 4.0f);
    const int w = to_device_pixels(width, options_.pixel_ratio);
    const int h = to_device_pixels(height, options_.pixel_ratio);
    if (w != GetScreenWidth() || h != GetScreenHeight()) {
        SetWindowSize(w, h);
    }
    baked_viewport_ = {}; // Pixel ratio may have changed even if the size did not
}

void App::set_overlays(bool show_controls, bool show_hud) {
    options_.show_controls = show_controls;
    options_.show_hud = show_hud;
}

void App::set_background(Color color) {
    options_.background = color;
}

bool App::init(const char* title, int width, int height, const AppOptions& options) {
    options_ = options;
    options_.pixel_ratio = std::clamp(options_.pixel_ratio, 0.5f, 4.0f);

    InitWindow(to_device_pixels(width, options_.pixel_ratio),
               to_device_pixels(height, options_.pixel_ratio), title);
    if (!IsWindowReady()) return false;

    gpu_name_ = platform::get_gpu_name(); // Query once, not every frame
    rebuild_grid();
    return true;
}

void App::shutdown() {
    renderer_.release();
    CloseWindow();
}

void App::frame() {
    // 1. Rebuild grid, mesh and baked texture outside BeginDrawing() when dirty.
    if (!grid_ || !state_.approx_equals(last_state_)) {
        rebuild_grid();
    }
    if (view_.time_of_day != last_time_of_day_) {
        rebuild_mesh();
    }
    const geometry::Viewport viewport = scene_viewport();
    if (!(viewport == baked_viewport_)) {
        bake(viewport);
    }

    BeginDrawing();
        ClearBackground(options_.background);

        // 2. World pass (already fitted to the viewport).
        renderer_.draw(options_.pixel_ratio);

        // 3. Optional screen-space overlays. Field edits take effect on the next frame.
        if (options_.show_hud) ui::draw_hud(state_, gpu_name_.c_str());
        if (options_.show_controls) panel_.draw(state_, view_);
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
    last_time_of_day_ = -1.0f; // Force a mesh rebuild before drawing
}

void App::rebuild_mesh() {
    mesh_ = geometry::build_field_mesh(
        grid_view_,
        projection_,
        geometry::Lighting::from_time_of_day(view_.time_of_day)
    );
    last_time_of_day_ = view_.time_of_day;
    baked_viewport_ = {}; // Force a re-bake: the bounds may have changed
}

// Must run outside BeginDrawing(): baking renders into an off-screen texture.
void App::bake(const geometry::Viewport& viewport) {
    const float padding = kFitPadding * options_.pixel_ratio;
    renderer_.upload(mesh_, geometry::fit_bounds(mesh_.bounds, viewport, padding));
    baked_viewport_ = viewport;
}

/// Free screen area for the field: below the status bar, left of the panel (when shown).
geometry::Viewport App::scene_viewport() const {
    const float screen_w = static_cast<float>(GetScreenWidth());
    const float screen_h = static_cast<float>(GetScreenHeight());
    const float top = options_.show_hud ? static_cast<float>(ui::kHudHeight) : 0.0f;

    float right = screen_w;
    if (options_.show_controls) {
        right = panel_.bounds().x - kPanelGap;
        if (right < kMinSceneWidth) right = screen_w;
    }

    return { 0.0f, top, right, std::max(0.0f, screen_h - top) };
}

} // namespace farmhorizon
