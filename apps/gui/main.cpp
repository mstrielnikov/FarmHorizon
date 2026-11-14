#include <iostream>
#include <algorithm>
#include <memory>
#include <cmath>

#include <raylib.h>
#include <raymath.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include "generation/grid.hpp"
#include "platform/diagnostics.hpp"
#include "generation_config.hpp"

using namespace generation;

static std::unique_ptr<Grid> g_grid;
static GridView g_grid_view;
static GenConfig g_config;
static GenConfig g_last_config;

struct CachedTriangle {
    Vector2 p[3];
    Color c;
};
static std::vector<CachedTriangle> g_mesh_cache;
static float g_last_time_of_day = -1.0f;

// Off-screen render texture for terrain (draw once, blit every frame)
static RenderTexture2D g_terrain_rt = {};
static bool g_terrain_rt_dirty = true;
static Vector2 g_terrain_offset = {0, 0};
static float g_terrain_world_w = 0.0f;
static float g_terrain_world_h = 0.0f;
static float g_terrain_render_scale = 1.0f;
static int g_terrain_rt_w = 0;
static int g_terrain_rt_h = 0;
static bool g_has_harvester = false;
static Vector2 g_harvester_label_pos = {0, 0};
static int g_preset_idx = 0;

extern "C" void SetExternalSceneState(
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
    g_last_time_of_day = -1.0f; // Force cache rebuild
    g_terrain_rt_dirty = true;
}

Vector2 ProjectIso(float x, float y, float height);
Vector3 CalculateNormal(Vector3 p0, Vector3 p1, Vector3 p2);

void RebuildMeshCache() {
    g_mesh_cache.clear();
    g_has_harvester = false;

    const size_t N = g_grid_view.width();
    if (N < 3) return;

    float sun_theta = (std::clamp(g_config.time_of_day, 6.0f, 18.0f) - 6.0f) / 12.0f * PI;
    float sun_dir_x = std::cos(sun_theta);
    float sun_elev  = std::sin(sun_theta);
    float ambient   = 0.48f + 0.45f * sun_elev;

    auto ScaleColor = [](uint8_t r, uint8_t g, uint8_t b, float factor) -> Color {
        return {
            (unsigned char)std::clamp(std::round(r * factor), 0.0f, 255.0f),
            (unsigned char)std::clamp(std::round(g * factor), 0.0f, 255.0f),
            (unsigned char)std::clamp(std::round(b * factor), 0.0f, 255.0f),
            255
        };
    };

    auto PushTriCCW = [&](Vector2 a, Vector2 b, Vector2 c, Color col) {
        float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (cross > 0.0f) {
            g_mesh_cache.push_back({ {a, c, b}, col });
        } else {
            g_mesh_cache.push_back({ {a, b, c}, col });
        }
    };

    auto PushQuad = [&](Vector2 p00, Vector2 p10, Vector2 p11, Vector2 p01, Color c1, Color c2) {
        PushTriCCW(p00, p01, p10, c1);
        PushTriCCW(p10, p01, p11, c2);
    };

    g_mesh_cache.reserve(N * N * 10);

    int harv_x = -1, harv_y = -1;

    for (size_t y = 0; y < N; ++y) {
        for (size_t x = 0; x < N; ++x) {
            const Cell& cell = g_grid_view(x, y);
            float fx = (float)x;
            float fy = (float)y;

            // 1. Ploughed Soil Furrow Base Quad
            float furrow_l = (x % 2 == 0) ? 0.025f : -0.015f;
            float furrow_r = -furrow_l;
            float slope_shade = std::clamp(ambient + ((x % 2 == 0 ? 1.0f : -1.0f) * sun_dir_x * 0.12f), 0.35f, 1.12f);

            Vector2 s00 = ProjectIso(fx,        fy,        furrow_l);
            Vector2 s10 = ProjectIso(fx + 1.0f, fy,        furrow_r);
            Vector2 s11 = ProjectIso(fx + 1.0f, fy + 1.0f, furrow_r);
            Vector2 s01 = ProjectIso(fx,        fy + 1.0f, furrow_l);

            Color soilCol1 = ScaleColor(cell.r, cell.g, cell.b, slope_shade);
            Color soilCol2 = ScaleColor(cell.r, cell.g, cell.b, slope_shade * 0.94f);
            PushQuad(s00, s10, s11, s01, soilCol1, soilCol2);

            // 2. Interior Field Crop Canopy or Harvested Stubble
            if (cell.role == CellRole::HarvestedStubble || cell.role == CellRole::HarvesterActive) {
                if (cell.role == CellRole::HarvesterActive) {
                    harv_x = (int)x;
                    harv_y = (int)y;
                }
                float stub_h = cell.crop_height;
                Vector2 st00 = ProjectIso(fx + 0.25f, fy + 0.25f, stub_h);
                Vector2 st10 = ProjectIso(fx + 0.75f, fy + 0.25f, stub_h);
                Vector2 st11 = ProjectIso(fx + 0.75f, fy + 0.75f, stub_h);
                Vector2 st01 = ProjectIso(fx + 0.25f, fy + 0.75f, stub_h);
                Color stubCol = ScaleColor(cell.crop_r, cell.crop_g, cell.crop_b, ambient);
                PushQuad(st00, st10, st11, st01, stubCol, stubCol);
            } else if (cell.role == CellRole::StandingCrop) {
                float pad = (1.0f - cell.canopy_density) * 0.36f;
                float ch  = cell.crop_height;

                Vector2 b01 = ProjectIso(fx + pad,        fy + 0.88f, 0.0f);
                Vector2 b11 = ProjectIso(fx + 1.0f - pad, fy + 0.88f, 0.0f);
                Vector2 b10 = ProjectIso(fx + 1.0f - pad, fy + 0.12f, 0.0f);

                Vector2 c00 = ProjectIso(fx + pad,        fy + 0.12f, ch);
                Vector2 c10 = ProjectIso(fx + 1.0f - pad, fy + 0.12f, ch * 0.92f);
                Vector2 c11 = ProjectIso(fx + 1.0f - pad, fy + 0.88f, ch * 0.92f);
                Vector2 c01 = ProjectIso(fx + pad,        fy + 0.88f, ch);

                Color sideLeft  = ScaleColor(cell.crop_r, cell.crop_g, cell.crop_b, slope_shade * 0.68f);
                Color sideRight = ScaleColor(cell.crop_r, cell.crop_g, cell.crop_b, slope_shade * 0.56f);
                Color topCol1   = ScaleColor(cell.crop_r, cell.crop_g, cell.crop_b, slope_shade);
                Color topCol2   = ScaleColor(cell.crop_r, cell.crop_g, cell.crop_b, slope_shade * 0.90f);

                // Extruded stalk walls + canopy roof
                PushQuad(c01, c11, b11, b01, sideLeft, sideLeft);
                PushQuad(c10, c11, b11, b10, sideRight, sideRight);
                PushQuad(c00, c10, c11, c01, topCol1, topCol2);
            }

            // 3. 3D Soil Cross-Section Skirt along front parcel edges
            const float slab_h = -0.32f;
            if (y == N - 1) {
                Vector2 d01 = ProjectIso(fx,        fy + 1.0f, slab_h);
                Vector2 d11 = ProjectIso(fx + 1.0f, fy + 1.0f, slab_h);
                Color skirtL = ScaleColor(cell.r, cell.g, cell.b, 0.62f);
                PushQuad(s01, s11, d11, d01, skirtL, skirtL);
            }
            if (x == N - 1) {
                Vector2 d10 = ProjectIso(fx + 1.0f, fy,        slab_h);
                Vector2 d11 = ProjectIso(fx + 1.0f, fy + 1.0f, slab_h);
                Color skirtR = ScaleColor(cell.r, cell.g, cell.b, 0.48f);
                PushQuad(s10, s11, d11, d10, skirtR, skirtR);
            }
        }
    }

    // 4. Combine Harvester 3D Isometric Body at the active frontier cell
    if (harv_x >= 0 && harv_y >= 0) {
        float hx = (float)harv_x;
        float hy = (float)harv_y;
        float h_base = 0.06f;
        float h_top  = 0.48f;

        Vector2 base01 = ProjectIso(hx - 0.08f, hy + 1.08f, h_base);
        Vector2 base11 = ProjectIso(hx + 1.08f, hy + 1.08f, h_base);
        Vector2 base10 = ProjectIso(hx + 1.08f, hy - 0.08f, h_base);

        Vector2 top00  = ProjectIso(hx - 0.08f, hy - 0.08f, h_top);
        Vector2 top10  = ProjectIso(hx + 1.08f, hy - 0.08f, h_top);
        Vector2 top11  = ProjectIso(hx + 1.08f, hy + 1.08f, h_top);
        Vector2 top01  = ProjectIso(hx - 0.08f, hy + 1.08f, h_top);

        Color cabTop   = { 230, 57,  70,  255 };
        Color cabLeft  = { 185, 42,  54,  255 };
        Color cabRight = { 145, 32,  42,  255 };
        Color headerC  = { 255, 209, 102, 255 };

        PushQuad(top01, top11, base11, base01, cabLeft, cabLeft);
        PushQuad(top10, top11, base11, base10, cabRight, cabRight);
        PushQuad(top00, top10, top11, top01, cabTop, cabTop);

        // Golden cutter bar stripe along the front of the harvester
        Vector2 hdr00 = ProjectIso(hx - 0.14f, hy + 0.85f, h_base + 0.08f);
        Vector2 hdr10 = ProjectIso(hx + 1.14f, hy + 0.85f, h_base + 0.08f);
        Vector2 hdr11 = ProjectIso(hx + 1.14f, hy + 1.15f, h_base + 0.08f);
        Vector2 hdr01 = ProjectIso(hx - 0.14f, hy + 1.15f, h_base + 0.08f);
        PushQuad(hdr00, hdr10, hdr11, hdr01, headerC, headerC);

        g_has_harvester = true;
        g_harvester_label_pos = ProjectIso(hx + 0.5f, hy + 0.5f, h_top + 0.18f);
    }

    g_last_time_of_day = g_config.time_of_day;
    g_terrain_rt_dirty = true;
}

void RebuildTerrainTexture() {
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (const auto& tri : g_mesh_cache) {
        for (int i = 0; i < 3; ++i) {
            minX = std::min(minX, tri.p[i].x);
            minY = std::min(minY, tri.p[i].y);
            maxX = std::max(maxX, tri.p[i].x);
            maxY = std::max(maxY, tri.p[i].y);
        }
    }

    float margin = 16.0f;
    minX -= margin; minY -= margin;
    maxX += margin; maxY += margin;

    float real_w = maxX - minX;
    float real_h = maxY - minY;

    const float MAX_RT_SIZE = 8192.0f; 
    g_terrain_render_scale = 1.0f;
    if (real_w > MAX_RT_SIZE) g_terrain_render_scale = std::min(g_terrain_render_scale, MAX_RT_SIZE / real_w);
    if (real_h > MAX_RT_SIZE) g_terrain_render_scale = std::min(g_terrain_render_scale, MAX_RT_SIZE / real_h);

    int rt_w = (int)(real_w * g_terrain_render_scale);
    int rt_h = (int)(real_h * g_terrain_render_scale);

    if (rt_w != g_terrain_rt_w || rt_h != g_terrain_rt_h) {
        if (g_terrain_rt.id > 0) UnloadRenderTexture(g_terrain_rt);
        g_terrain_rt = LoadRenderTexture(rt_w, rt_h);
        g_terrain_rt_w = rt_w;
        g_terrain_rt_h = rt_h;
    }

    g_terrain_offset = { minX, minY };
    g_terrain_world_w = real_w;
    g_terrain_world_h = real_h;

    BeginTextureMode(g_terrain_rt);
        ClearBackground(BLANK);
        for (const auto& tri : g_mesh_cache) {
            Vector2 a = { (tri.p[0].x - minX) * g_terrain_render_scale, (tri.p[0].y - minY) * g_terrain_render_scale };
            Vector2 b = { (tri.p[1].x - minX) * g_terrain_render_scale, (tri.p[1].y - minY) * g_terrain_render_scale };
            Vector2 c = { (tri.p[2].x - minX) * g_terrain_render_scale, (tri.p[2].y - minY) * g_terrain_render_scale };
            DrawTriangle(a, b, c, tri.c);
        }
    EndTextureMode();

    g_terrain_rt_dirty = false;
}

Vector2 ProjectIso(float x, float y, float height) {
    return {
        (x - y) * 32.0f,
        (x + y) * 16.0f - height * 80.0f
    };
}

Vector3 CalculateNormal(Vector3 p0, Vector3 p1, Vector3 p2) {
    Vector3 v1 = Vector3Subtract(p1, p0);
    Vector3 v2 = Vector3Subtract(p2, p0);
    Vector3 n = Vector3Normalize(Vector3CrossProduct(v1, v2));
    if (n.z < 0) n = Vector3Scale(n, -1.0f); // Ensure normal points UP
    return n;
}

int main() {
    const int screenWidth = 1200;
    const int screenHeight = 840;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "FarmHorizon");
    SetTargetFPS(60);

    RebuildGrid();

    Camera2D camera = { .offset = { 540, 140 }, .target = { 0, 0 }, .rotation = 0.0f, .zoom = 1.05f };

    while (!WindowShouldClose()) {
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

        // Render terrain to off-screen texture if dirty
        if (g_terrain_rt_dirty && !g_mesh_cache.empty()) {
            RebuildTerrainTexture();
        }

        BeginDrawing();
            ClearBackground({ 30, 30, 35, 255 });
            
            BeginMode2D(camera);
                if (g_terrain_rt.id > 0) {
                    Rectangle src = { 0, 0, (float)g_terrain_rt_w, -(float)g_terrain_rt_h };
                    Rectangle dst = { g_terrain_offset.x, g_terrain_offset.y, g_terrain_world_w, g_terrain_world_h };
                    DrawTexturePro(g_terrain_rt.texture, src, dst, {0, 0}, 0.0f, WHITE);
                }
                if (g_has_harvester) {
                    DrawText("HARVESTER", (int)g_harvester_label_pos.x - 36, (int)g_harvester_label_pos.y - 10, 14, GOLD);
                }
            EndMode2D();

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

    CloseWindow();
    return 0;
}
