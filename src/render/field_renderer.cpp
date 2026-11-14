#include "farmhorizon/render/field_renderer.hpp"

#include <algorithm>

namespace farmhorizon::render {

namespace {

Color to_color(geometry::Rgba8 c) { return { c.r, c.g, c.b, c.a }; }

} // namespace

void FieldRenderer::upload(const geometry::FieldMesh& mesh) {
    has_label_ = mesh.has_harvester;
    label_anchor_ = { mesh.harvester_label_anchor.x, mesh.harvester_label_anchor.y };
    if (mesh.triangles.empty() || !mesh.bounds.valid) return;

    const float min_x  = mesh.bounds.min.x - kMargin;
    const float min_y  = mesh.bounds.min.y - kMargin;
    const float world_w = mesh.bounds.width()  + 2.0f * kMargin;
    const float world_h = mesh.bounds.height() + 2.0f * kMargin;

    // Downscale huge fields so the texture stays within GPU limits.
    const float scale = std::min({ 1.0f, kMaxTexture / world_w, kMaxTexture / world_h });
    const int w = static_cast<int>(world_w * scale);
    const int h = static_cast<int>(world_h * scale);

    if (w != tex_w_ || h != tex_h_) {
        release();
        target_ = LoadRenderTexture(w, h);
        tex_w_ = w;
        tex_h_ = h;
    }
    world_rect_ = { min_x, min_y, world_w, world_h };

    auto to_texture = [&](geometry::Vec2 p) -> Vector2 {
        return { (p.x - min_x) * scale, (p.y - min_y) * scale };
    };

    BeginTextureMode(target_);
        ClearBackground(BLANK);
        for (const auto& tri : mesh.triangles) {
            DrawTriangle(to_texture(tri.p[0]), to_texture(tri.p[1]), to_texture(tri.p[2]), to_color(tri.color));
        }
    EndTextureMode();
}

void FieldRenderer::draw(const Camera2D& camera) const {
    BeginMode2D(camera);
        if (ready()) {
            // Render textures are stored upside down in OpenGL; flip via negative source height.
            const Rectangle src = { 0, 0, static_cast<float>(tex_w_), -static_cast<float>(tex_h_) };
            DrawTexturePro(target_.texture, src, world_rect_, { 0, 0 }, 0.0f, WHITE);
        }
        if (has_label_) {
            DrawText("HARVESTER", static_cast<int>(label_anchor_.x) - 36, static_cast<int>(label_anchor_.y) - 10, 14, GOLD);
        }
    EndMode2D();
}

void FieldRenderer::release() {
    if (target_.id > 0) UnloadRenderTexture(target_);
    target_ = {};
    tex_w_ = 0;
    tex_h_ = 0;
}

} // namespace farmhorizon::render
