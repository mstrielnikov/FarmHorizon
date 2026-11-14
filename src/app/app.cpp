#include <iostream>
#include <algorithm>
#include <memory>
#include <cmath>

#include <raylib.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include "farmhorizon/app/app.hpp"
#include "farmhorizon/geometry/field_mesh.hpp"
#include "farmhorizon/render/field_renderer.hpp"
#include "generation/grid.hpp"
#include "platform/diagnostics.hpp"
#include "generation_config.hpp"

using namespace generation;
namespace geo = farmhorizon::geometry;

static std::unique_ptr<Grid> g_grid;
static GridView g_grid_view;
static GenConfig g_config;
static GenConfig g_last_config;

static const geo::IsoProjection g_projection{};
static farmhorizon::render::FieldRenderer g_renderer;
static float g_last_time_of_day = -1.0f;
static int g_preset_idx = 0;

static Camera2D g_camera = {};

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

void RebuildGrid() {
    int new_size = std::max(4, (int)std::round(g_config.field_size));
    auto grid_res = Grid::create(new_size, new_size);
    if (!grid_res.has_value()) {
        std::cerr << "Failed to allocate Grid\n";
        return;
    }
    g_grid = std::make_unique<Grid>(std::move(grid_res.value()));
    populate_field_grid(*g_grid, g_config);
    g_grid_view = g_grid->view();

    g_last_config = g_config;
    g_last_time_of_day = -1.0f; // Force mesh rebuild on the next frame
}

// Must run outside BeginDrawing(): uploading bakes into an off-screen texture.
void RebuildMeshCache() {
    g_renderer.upload(geo::build_field_mesh(g_grid_view, g_projection,
                                            geo::Lighting::from_time_of_day(g_config.time_of_day)));
    g_last_time_of_day = g_config.time_of_day;
}

bool farmhorizon::App::init(const char* title, int width, int height) {
    InitWindow(width, height, title);
    if (!IsWindowReady()) return false;

    RebuildGrid();
    g_camera = { .offset = { 540, 140 }, .target = { 0, 0 }, .rotation = 0.0f, .zoom = 1.05f };
    return true;
}

void farmhorizon::App::shutdown() {
    g_renderer.release();
    CloseWindow();
}

void farmhorizon::App::frame() {
    Camera2D& camera = g_camera;
    {
        // Check if mouse is over UI
        int panelWidth = 310;
        int panelHeight = 405;
        int panelX = GetScreenWidth() - panelWidth - 20;
        int panelY = 80;
        Rectangle uiBounds = { (float)panelX, (float)panelY, (float)panelWidth, (float)panelHeight };
        bool isMouseOverUI = CheckCollisionPointRec(GetMousePosition(), uiBounds);

        if (!isMouseOverUI && (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))) {
            Vector2 delta = GetMouseDelta();
            camera.target.x -= delta.x / camera.zoom;
            camera.target.y -= delta.y / camera.zoom;
        }
        camera.zoom = std::clamp(camera.zoom + GetMouseWheelMove() * 0.05f, 0.5f, 1.8f);

        // Rebuild mesh cache if needed
        if (g_config.time_of_day != g_last_time_of_day) {
            RebuildMeshCache();
        }

        BeginDrawing();
            ClearBackground({ 30, 30, 35, 255 });
            
            g_renderer.draw(camera);

            int soilIdx = (int)std::round(g_config.soil_type);
            int cropIdx = (int)std::round(g_config.crop_type);
            int fldSize = (int)std::round(g_config.field_size);

            DrawRectangle(0, 0, GetScreenWidth(), 60, Fade(BLACK, 0.8f));
            DrawText("FarmHorizon", 20, 10, 20, GOLD);
            DrawText(
                TextFormat(
                    "Renderer: %s | Soil: %s (%.0f%%) | Crop: %s | Ext Yield: %.1f t/ha",
                    diagnostics::get_gpu_name().c_str(),
                    get_soil_name(soilIdx),
                    g_config.soil_moisture,
                    get_crop_name(cropIdx),
                    g_config.yield_ext
                ),
                20, 35, 14, LIME
            );
            DrawFPS(GetScreenWidth() - 80, 10);

            // Top-Right Floating UI Panel
            DrawRectangle(panelX, panelY, panelWidth, panelHeight, Fade(BLACK, 0.85f));

            GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
            GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(RAYWHITE));
            
            int currY = panelY + 15;
            DrawText("Zoom", panelX + 15, currY, 16, RAYWHITE);
            float currentZoom = camera.zoom;
            GuiSliderBar({ (float)panelX + 110, (float)currY - 3, 140, 22 }, "", TextFormat("%.2f", currentZoom), &currentZoom, 0.5f, 1.8f);
            if (currentZoom != camera.zoom) camera.zoom = currentZoom;

            currY += 35;
            DrawText("Time of Day", panelX + 15, currY, 16, RAYWHITE);
            GuiSliderBar({ (float)panelX + 110, (float)currY - 3, 140, 22 }, "", TextFormat("%.1f", g_config.time_of_day), &g_config.time_of_day, 6.0f, 18.0f);

            currY += 35;
            DrawText("Field Size", panelX + 15, currY, 16, RAYWHITE);
            GuiSliderBar({ (float)panelX + 110, (float)currY - 3, 140, 22 }, "", TextFormat("%dx%d", fldSize, fldSize), &g_config.field_size, 12.0f, 64.0f);

            currY += 35;
            DrawText("Soil Type", panelX + 15, currY, 16, RAYWHITE);
            GuiSliderBar({ (float)panelX + 110, (float)currY - 3, 140, 22 }, "", get_soil_name(soilIdx), &g_config.soil_type, 0.0f, 3.0f);

            currY += 35;
            DrawText("Soil Moist", panelX + 15, currY, 16, RAYWHITE);
            GuiSliderBar({ (float)panelX + 110, (float)currY - 3, 140, 22 }, "", TextFormat("%.0f%%", g_config.soil_moisture), &g_config.soil_moisture, 10.0f, 60.0f);

            currY += 35;
            DrawText("Crop Type", panelX + 15, currY, 16, RAYWHITE);
            GuiSliderBar({ (float)panelX + 110, (float)currY - 3, 140, 22 }, "", get_crop_name(cropIdx), &g_config.crop_type, 0.0f, 3.0f);

            currY += 35;
            DrawText("NDVI Index", panelX + 15, currY, 16, RAYWHITE);
            GuiSliderBar({ (float)panelX + 110, (float)currY - 3, 140, 22 }, "", TextFormat("%.2f", g_config.ndvi_index), &g_config.ndvi_index, 0.10f, 0.85f);

            Color yieldGreen = { 126, 231, 135, 255 };
            currY += 35;
            DrawText("Yield (Ext)", panelX + 15, currY, 16, yieldGreen);
            GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(yieldGreen));
            GuiSliderBar({ (float)panelX + 110, (float)currY - 3, 140, 22 }, "", TextFormat("%.0ft", g_config.yield_ext), &g_config.yield_ext, 10.0f, 70.0f);

            Color harvAmber = { 255, 184, 108, 255 };
            currY += 35;
            DrawText("Harvested", panelX + 15, currY, 16, harvAmber);
            GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(harvAmber));
            GuiSliderBar({ (float)panelX + 110, (float)currY - 3, 140, 22 }, "", TextFormat("%.0f%%", g_config.harvested_pct), &g_config.harvested_pct, 0.0f, 100.0f);

            GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(RAYWHITE));

            currY += 36;
            if (GuiButton({ (float)panelX + 15, (float)currY, 175, 28 }, "Cycle Crop Preset")) {
                g_preset_idx = (g_preset_idx + 1) % 4;
                apply_crop_preset(g_config, g_preset_idx);
            }
            if (GuiButton({ (float)panelX + 198, (float)currY, 97, 28 }, "Reset View")) {
                camera.zoom = 1.05f;
                camera.target = { 0, 0 };
            }

            if (!g_config.state_equals(g_last_config)) {
                RebuildGrid();
            }

        EndDrawing();
    }
}
