#include "farmhorizon/render/field_renderer.hpp"

#include <algorithm>
#include <cmath>

namespace farmhorizon::render {

namespace {

Color to_color(geometry::Rgba8 c) { return { c.r, c.g, c.b, c.a }; }

} // namespace

void FieldRenderer::upload(const geometry::FieldMesh& mesh, const geometry::ViewFit& fit) {
    has_label_ = false;
    screen_rect_ = {};
    if (mesh.triangles.empty() || !mesh.bounds.valid || !fit.valid()) return;

    const float world_w = mesh.bounds.width();
    const float world_h = mesh.bounds.height();

    // Bake at the on-screen scale so the blit is 1:1; only huge views get capped.
    const float bake_scale = std::min({ fit.scale, kMaxTexture / world_w, kMaxTexture / world_h });
    const int w = static_cast<int>(std::ceil(world_w * bake_scale)) + 2 * kBleed;
    const int h = static_cast<int>(std::ceil(world_h * bake_scale)) + 2 * kBleed;

    if (w != tex_w_ || h != tex_h_) {
        release();
        target_ = LoadRenderTexture(w, h);
        tex_w_ = w;
        tex_h_ = h;
    }

    const geometry::Vec2 origin = {
        mesh.bounds.min.x - kBleed / bake_scale,
        mesh.bounds.min.y - kBleed / bake_scale
    };
    auto to_texture = [&](geometry::Vec2 p) -> Vector2 {
        return { (p.x - origin.x) * bake_scale, (p.y - origin.y) * bake_scale };
    };

    BeginTextureMode(target_);
        ClearBackground(BLANK);
        for (const auto& tri : mesh.triangles) {
            DrawTriangle(to_texture(tri.p[0]), to_texture(tri.p[1]), to_texture(tri.p[2]), to_color(tri.color));
        }
    EndTextureMode();

    // Snap to whole pixels so the 1:1 blit does not shift texels.
    const geometry::Vec2 top_left = fit.to_screen(origin);
    const float blit = fit.scale / bake_scale; // 1.0 unless the texture was capped
    screen_rect_ = {
        std::round(top_left.x),
        std::round(top_left.y),
        static_cast<float>(w) * blit,
        static_cast<float>(h) * blit
    };

    if (mesh.has_harvester) {
        const geometry::Vec2 anchor = fit.to_screen(mesh.harvester_label_anchor);
        label_anchor_ = { anchor.x, anchor.y };
        has_label_ = true;
    }
}

void FieldRenderer::draw(float ui_scale) const {
    if (ready() && screen_rect_.width > 0.0f) {
        // Render textures are stored upside down in OpenGL; flip via negative source height.
        const Rectangle src = { 0, 0, static_cast<float>(tex_w_), -static_cast<float>(tex_h_) };
        DrawTexturePro(target_.texture, src, screen_rect_, { 0, 0 }, 0.0f, WHITE);
    }
    if (has_label_) {
        const int font = std::max(10, static_cast<int>(std::lround(14.0f * ui_scale)));
        const char* text = "HARVESTER";
        const int x = static_cast<int>(label_anchor_.x) - MeasureText(text, font) / 2;
        const int y = static_cast<int>(label_anchor_.y) - font;
        DrawText(text, x, y, font, GOLD);
    }
}

void FieldRenderer::release() {
    if (target_.id > 0) UnloadRenderTexture(target_);
    target_ = {};
    tex_w_ = 0;
    tex_h_ = 0;
}

} // namespace farmhorizon::render
