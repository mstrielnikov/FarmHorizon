#include <iostream>
#include <algorithm>
#include <memory>
#include <cmath>

#include <raylib.h>
#include <raymath.h>

#include <emscripten/emscripten.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include "generation/grid.hpp"
#include "platform/diagnostics.hpp"
#include "generation_config.hpp"

using namespace generation;

static std::unique_ptr<Grid> g_grid;
static GridView g_grid_view;
static Camera2D g_camera = {};
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
static Vector2 g_terrain_offset = {0, 0};  // Translation to place terrain in texture
static float g_terrain_world_w = 0.0f;     // Original world width
static float g_terrain_world_h = 0.0f;     // Original world height
static float g_terrain_render_scale = 1.0f; // Downscaling factor for huge maps
static int g_terrain_rt_w = 0;
static int g_terrain_rt_h = 0;

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

    float sun_theta = (std::clamp(g_config.time_of_day, 6.0f, 18.0f) - 6.0f) / 12.0f * PI;
    Vector3 lightDir = { std::cos(sun_theta), std::cos(sun_theta) * 0.5f, std::sin(sun_theta) };
    lightDir = Vector3Normalize(lightDir);
    float ambient = 0.35f;

    size_t quads = (g_grid_view.height() - 1) * (g_grid_view.width() - 1);
    g_mesh_cache.reserve(quads * 2);

    for (size_t y = 0; y < g_grid_view.height() - 1; ++y) {
        for (size_t x = 0; x < g_grid_view.width() - 1; ++x) {
            const Cell& c00 = g_grid_view(x, y);
            const Cell& c10 = g_grid_view(x+1, y);
            const Cell& c01 = g_grid_view(x, y+1);
            const Cell& c11 = g_grid_view(x+1, y+1);

            Color baseColor = { c00.r, c00.g, c00.b, 255 };

            float fx00 = (float)x,     fy00 = (float)y;
            float fx10 = (float)(x+1), fy10 = (float)y;
            float fx01 = (float)x,     fy01 = (float)(y+1);
            float fx11 = (float)(x+1), fy11 = (float)(y+1);

            Vector3 p00 = { fx00, fy00, c00.height * 2.5f };
            Vector3 p10 = { fx10, fy10, c10.height * 2.5f };
            Vector3 p01 = { fx01, fy01, c01.height * 2.5f };
            Vector3 p11 = { fx11, fy11, c11.height * 2.5f };

            Vector2 s00 = ProjectIso(fx00, fy00, c00.height);
            Vector2 s10 = ProjectIso(fx10, fy10, c10.height);
            Vector2 s01 = ProjectIso(fx01, fy01, c01.height);
            Vector2 s11 = ProjectIso(fx11, fy11, c11.height);

            Vector3 n1 = CalculateNormal(p00, p01, p10);
            float light1 = std::clamp(Vector3DotProduct(n1, lightDir), 0.0f, 1.0f);
            float i1 = std::clamp(ambient + light1 * 0.8f, 0.0f, 1.0f);
            Color col1 = { (unsigned char)(baseColor.r * i1), (unsigned char)(baseColor.g * i1), (unsigned char)(baseColor.b * i1), 255 };
            g_mesh_cache.push_back({ {s00, s01, s10}, col1 });

            Vector3 n2 = CalculateNormal(p10, p01, p11);
            float light2 = std::clamp(Vector3DotProduct(n2, lightDir), 0.0f, 1.0f);
            float i2 = std::clamp(ambient + light2 * 0.8f, 0.0f, 1.0f);
            Color col2 = { (unsigned char)(baseColor.r * i2), (unsigned char)(baseColor.g * i2), (unsigned char)(baseColor.b * i2), 255 };
            g_mesh_cache.push_back({ {s01, s11, s10}, col2 });
        }
    }

    g_last_time_of_day = g_config.time_of_day;
    g_terrain_rt_dirty = true;
}

void RebuildTerrainTexture() {
    // Compute bounding box of all cached triangles
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (const auto& tri : g_mesh_cache) {
        for (int i = 0; i < 3; ++i) {
            minX = std::min(minX, tri.p[i].x);
            minY = std::min(minY, tri.p[i].y);
            maxX = std::max(maxX, tri.p[i].x);
            maxY = std::max(maxY, tri.p[i].y);
        }
    }

    // Add margin
    float margin = 16.0f;
    minX -= margin; minY -= margin;
    maxX += margin; maxY += margin;

    float real_w = maxX - minX;
    float real_h = maxY - minY;
    
    // Scale-aware RTT: Downscale if terrain exceeds texture limits (8k)
    // Most GPUs/WebGL2 support 8k. 16k is risky and uses 512MB+ VRAM.
    const float MAX_RT_SIZE = 8192.0f; 
    g_terrain_render_scale = 1.0f;
    if (real_w > MAX_RT_SIZE) g_terrain_render_scale = std::min(g_terrain_render_scale, MAX_RT_SIZE / real_w);
    if (real_h > MAX_RT_SIZE) g_terrain_render_scale = std::min(g_terrain_render_scale, MAX_RT_SIZE / real_h);

    int rt_w = (int)(real_w * g_terrain_render_scale);
    int rt_h = (int)(real_h * g_terrain_render_scale);

    // Recreate texture if size changed
    if (rt_w != g_terrain_rt_w || rt_h != g_terrain_rt_h) {
        if (g_terrain_rt.id > 0) UnloadRenderTexture(g_terrain_rt);
        g_terrain_rt = LoadRenderTexture(rt_w, rt_h);
        g_terrain_rt_w = rt_w;
        g_terrain_rt_h = rt_h;
    }

    g_terrain_offset = { minX, minY };
    g_terrain_world_w = real_w;
    g_terrain_world_h = real_h;

    // Render all triangles into the texture (one-time cost)
    BeginTextureMode(g_terrain_rt);
        ClearBackground(BLANK);
        // Apply scaling so huge maps fit in the texture
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
    if (n.z < 0) n = Vector3Scale(n, -1.0f);
    return n;
}

void UpdateDrawFrame() {
    int panelWidth = 280;
    int panelHeight = 360;
    int panelX = GetScreenWidth() - panelWidth - 20;
    int panelY = 80;
    Rectangle uiBounds = { (float)panelX, (float)panelY, (float)panelWidth, (float)panelHeight };
    bool isMouseOverUI = CheckCollisionPointRec(GetMousePosition(), uiBounds);

    if (!isMouseOverUI && (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))) {
        Vector2 delta = GetMouseDelta();
        g_camera.target.x -= delta.x / g_camera.zoom;
        g_camera.target.y -= delta.y / g_camera.zoom;
    }
    g_camera.zoom = std::clamp(g_camera.zoom + GetMouseWheelMove() * 0.05f, 0.1f, 3.0f);

    // Rebuild mesh cache if needed (only when params change)
    if (g_config.time_of_day != g_last_time_of_day) {
        RebuildMeshCache();
    }

    // Render terrain to off-screen texture if dirty (one-time cost)
    if (g_terrain_rt_dirty && !g_mesh_cache.empty()) {
        RebuildTerrainTexture();
    }

    BeginDrawing();
        ClearBackground({ 30, 30, 35, 255 });
        
        BeginMode2D(g_camera);
            // Draw the pre-rendered terrain texture (1 draw call!)
            if (g_terrain_rt.id > 0) {
                // Upsample the texture back to world size if it was downscaled
                Rectangle src = { 0, 0, (float)g_terrain_rt_w, -(float)g_terrain_rt_h };
                Rectangle dst = { g_terrain_offset.x, g_terrain_offset.y, g_terrain_world_w, g_terrain_world_h };
                DrawTexturePro(g_terrain_rt.texture, src, dst, {0, 0}, 0.0f, WHITE);
            }
        EndMode2D();

        DrawRectangle(0, 0, GetScreenWidth(), 60, Fade(BLACK, 0.8f));
        DrawText("Donjourno", 20, 10, 20, GOLD);
        DrawText(TextFormat("Renderer: %s | VRAM: %.1f%%", diagnostics::get_gpu_name().c_str(), diagnostics::get_vram_usage_percent()), 20, 35, 14, LIME);
        DrawFPS(GetScreenWidth() - 80, 10);

        DrawRectangle(panelX, panelY, panelWidth, panelHeight, Fade(BLACK, 0.85f));

        GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
        GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(RAYWHITE));
        
        int currY = panelY + 15;
        DrawText("Zoom", panelX + 15, currY, 16, RAYWHITE);
        float currentZoom = g_camera.zoom;
        GuiSliderBar({ (float)panelX + 100, (float)currY - 5, 160, 25 }, "", TextFormat("%.2f", currentZoom), &currentZoom, 0.1f, 3.0f);
        if (currentZoom != g_camera.zoom) g_camera.zoom = currentZoom;

        currY += 35;
        DrawText("Time of Day", panelX + 15, currY, 16, RAYWHITE);
        GuiSliderBar({ (float)panelX + 100, (float)currY - 5, 160, 25 }, "", TextFormat("%.1f", g_config.time_of_day), &g_config.time_of_day, 6.0f, 18.0f);

        currY += 35;
        DrawText("Field Size", panelX + 15, currY, 16, RAYWHITE);
        GuiSliderBar({ (float)panelX + 100, (float)currY - 5, 160, 25 }, "", TextFormat("%.0f", g_config.field_size), &g_config.field_size, 12.0f, 64.0f);

        if (!g_config.state_equals(g_last_config)) {
            RebuildGrid();
        }

        currY += 30;
        if (GuiButton({ (float)panelX + 15, (float)currY, 245, 25 }, "Reset View")) {
            g_camera.zoom = 0.5f;
            g_camera.target = { 0, 0 };
        }

    EndDrawing();
}

int main() {
    const int screenWidth = 1200;
    const int screenHeight = 840;

    InitWindow(screenWidth, screenHeight, "Donjourno - Web Edition");

    RebuildGrid();

    g_camera = { .offset = { 600, 200 }, .target = { 0, 0 }, .rotation = 0.0f, .zoom = 0.5f };

    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);

    CloseWindow();
    return 0;
}
