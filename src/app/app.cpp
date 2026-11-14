#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

#include <raylib.h>

#include "farmhorizon/app/app.hpp"
#include "farmhorizon/geometry/field_mesh.hpp"
#include "farmhorizon/render/field_renderer.hpp"
#include "farmhorizon/ui/control_panel.hpp"
#include "farmhorizon/ui/hud.hpp"
#include "generation/grid.hpp"
#include "platform/diagnostics.hpp"
#include "generation_config.hpp"

using namespace generation;
namespace geo = farmhorizon::geometry;

namespace {

constexpr Camera2D kDefaultCamera = { .offset = { 540, 140 }, .target = { 0, 0 }, .rotation = 0.0f, .zoom = 1.05f };
constexpr float kMinZoom = 0.5f;
constexpr float kMaxZoom = 1.8f;
constexpr Color kBackground = { 30, 30, 35, 255 };

std::unique_ptr<Grid> g_grid;
GridView g_grid_view;
GenConfig g_config;
GenConfig g_last_config;

const geo::IsoProjection g_projection{};
farmhorizon::render::FieldRenderer g_renderer;
farmhorizon::ui::ControlPanel g_panel;
float g_last_time_of_day = -1.0f;

Camera2D g_camera = kDefaultCamera;
std::string g_gpu_name;

void rebuild_grid() {
    const int size = std::max(4, (int)std::round(g_config.field_size));
    auto grid = Grid::create(size, size);
    if (!grid.has_value()) {
        std::cerr << "Failed to allocate Grid\n";
        return;
    }
    g_grid = std::make_unique<Grid>(std::move(grid.value()));
    populate_field_grid(*g_grid, g_config);
    g_grid_view = g_grid->view();

    g_last_config = g_config;
    g_last_time_of_day = -1.0f; // Force mesh rebuild on the next frame
}

// Must run outside BeginDrawing(): uploading bakes into an off-screen texture.
void rebuild_mesh() {
    g_renderer.upload(geo::build_field_mesh(g_grid_view, g_projection,
                                            geo::Lighting::from_time_of_day(g_config.time_of_day)));
    g_last_time_of_day = g_config.time_of_day;
}

/// Pan with mouse drag and zoom with the wheel, unless the UI owns the mouse.
void update_camera(bool ui_wants_mouse) {
    if (ui_wants_mouse) return;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        const Vector2 delta = GetMouseDelta();
        g_camera.target.x -= delta.x / g_camera.zoom;
        g_camera.target.y -= delta.y / g_camera.zoom;
    }
    g_camera.zoom = std::clamp(g_camera.zoom + GetMouseWheelMove() * 0.05f, kMinZoom, kMaxZoom);
}

} // namespace

void farmhorizon::App::set_external_state(
    int field_size, int soil_type, float soil_moisture,
    int crop_type, float ndvi_index, float yield_ext, float harvested_pct
) {
    g_config.field_size    = std::clamp((float)field_size, 12.0f, 64.0f);
    g_config.soil_type     = std::clamp((float)soil_type, 0.0f, 3.0f);
    g_config.soil_moisture = std::clamp(soil_moisture, 10.0f, 60.0f);
    g_config.crop_type     = std::clamp((float)crop_type, 0.0f, 3.0f);
    g_config.ndvi_index    = std::clamp(ndvi_index, 0.10f, 0.85f);
    g_config.yield_ext     = std::clamp(yield_ext, 10.0f, 70.0f);
    g_config.harvested_pct = std::clamp(harvested_pct, 0.0f, 100.0f);
}

bool farmhorizon::App::init(const char* title, int width, int height) {
    InitWindow(width, height, title);
    if (!IsWindowReady()) return false;

    g_gpu_name = diagnostics::get_gpu_name(); // Query once, not every frame
    g_camera = kDefaultCamera;
    rebuild_grid();
    return true;
}

void farmhorizon::App::shutdown() {
    g_renderer.release();
    CloseWindow();
}

void farmhorizon::App::frame() {
    // 1. Input: the UI gets first claim on the mouse.
    update_camera(g_panel.wants_mouse());

    // 2. Rebuild geometry if lighting or field data changed (outside the draw pass).
    if (g_config.time_of_day != g_last_time_of_day) {
        rebuild_mesh();
    }

    BeginDrawing();
        ClearBackground(kBackground);

        // 3. World pass.
        g_renderer.draw(g_camera);

        // 4. Screen-space overlay pass.
        ui::draw_hud(g_config, g_gpu_name.c_str());
        const ui::PanelResult panel = g_panel.draw(g_config, g_camera.zoom);
        if (panel.reset_view) {
            g_camera.zoom = kDefaultCamera.zoom;
            g_camera.target = kDefaultCamera.target;
        }

        // Field data edits take effect on the next frame's mesh rebuild.
        if (!g_config.state_equals(g_last_config)) {
            rebuild_grid();
        }
    EndDrawing();
}
