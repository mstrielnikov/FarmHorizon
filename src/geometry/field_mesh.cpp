#include "farmhorizon/geometry/field_mesh.hpp"

#include <cmath>
#include <numbers>
#include <utility>

namespace farmhorizon::geometry {

using generation::Cell;
using generation::CellRole;

Lighting Lighting::from_time_of_day(float hours) noexcept {
    const float sun_theta = (std::clamp(hours, 6.0f, 18.0f) - 6.0f) / 12.0f * std::numbers::pi_v<float>;
    return {
        .ambient   = 0.48f + 0.45f * std::sin(sun_theta),
        .sun_dir_x = std::cos(sun_theta),
    };
}

namespace {

/// Accumulates triangles in a consistent screen-space winding and tracks bounds.
class MeshWriter {
public:
    explicit MeshWriter(FieldMesh& mesh) : mesh_(mesh) {}

    void tri(Vec2 a, Vec2 b, Vec2 c, Rgba8 col) {
        // Normalise to counter-clockwise on screen (y down), as expected by raylib.
        if (cross(a, b, c) > 0.0f) std::swap(b, c);
        mesh_.triangles.push_back({ { a, b, c }, col });
        mesh_.bounds.add(a);
        mesh_.bounds.add(b);
        mesh_.bounds.add(c);
    }

    void quad(Vec2 p00, Vec2 p10, Vec2 p11, Vec2 p01, Rgba8 c1, Rgba8 c2) {
        tri(p00, p01, p10, c1);
        tri(p10, p01, p11, c2);
    }

    void quad(Vec2 p00, Vec2 p10, Vec2 p11, Vec2 p01, Rgba8 c) { quad(p00, p10, p11, p01, c, c); }

private:
    FieldMesh& mesh_;
};

constexpr float kSkirtDepth = -0.32f; ///< Height of the soil cross-section below ground

void emit_soil(MeshWriter& out, const IsoProjection& proj, const Cell& cell,
               float fx, float fy, float furrow, float shade_k) {
    const Vec2 s00 = proj.project(fx,        fy,        furrow);
    const Vec2 s10 = proj.project(fx + 1.0f, fy,        -furrow);
    const Vec2 s11 = proj.project(fx + 1.0f, fy + 1.0f, -furrow);
    const Vec2 s01 = proj.project(fx,        fy + 1.0f, furrow);
    out.quad(s00, s10, s11, s01,
             shade(cell.r, cell.g, cell.b, shade_k),
             shade(cell.r, cell.g, cell.b, shade_k * 0.94f));
}

void emit_stubble(MeshWriter& out, const IsoProjection& proj, const Cell& cell,
                  float fx, float fy, float ambient) {
    const float h = cell.crop_height;
    out.quad(proj.project(fx + 0.25f, fy + 0.25f, h),
             proj.project(fx + 0.75f, fy + 0.25f, h),
             proj.project(fx + 0.75f, fy + 0.75f, h),
             proj.project(fx + 0.25f, fy + 0.75f, h),
             shade(cell.crop_r, cell.crop_g, cell.crop_b, ambient));
}

void emit_crop(MeshWriter& out, const IsoProjection& proj, const Cell& cell,
               float fx, float fy, float shade_k) {
    const float pad = (1.0f - cell.canopy_density) * 0.36f;
    const float ch  = cell.crop_height;
    const float x0 = fx + pad, x1 = fx + 1.0f - pad;
    const float y0 = fy + 0.12f, y1 = fy + 0.88f;

    const Vec2 b01 = proj.project(x0, y1, 0.0f);
    const Vec2 b11 = proj.project(x1, y1, 0.0f);
    const Vec2 b10 = proj.project(x1, y0, 0.0f);

    const Vec2 c00 = proj.project(x0, y0, ch);
    const Vec2 c10 = proj.project(x1, y0, ch * 0.92f);
    const Vec2 c11 = proj.project(x1, y1, ch * 0.92f);
    const Vec2 c01 = proj.project(x0, y1, ch);

    const auto& r = cell.crop_r; const auto& g = cell.crop_g; const auto& b = cell.crop_b;
    out.quad(c01, c11, b11, b01, shade(r, g, b, shade_k * 0.68f));                          // front-left wall
    out.quad(c10, c11, b11, b10, shade(r, g, b, shade_k * 0.56f));                          // front-right wall
    out.quad(c00, c10, c11, c01, shade(r, g, b, shade_k), shade(r, g, b, shade_k * 0.90f)); // canopy roof
}

void emit_skirt(MeshWriter& out, const IsoProjection& proj, const Cell& cell,
                float fx, float fy, float furrow, bool front_left, bool front_right) {
    if (front_left) {
        const Vec2 s01 = proj.project(fx,        fy + 1.0f, furrow);
        const Vec2 s11 = proj.project(fx + 1.0f, fy + 1.0f, -furrow);
        out.quad(s01, s11,
                 proj.project(fx + 1.0f, fy + 1.0f, kSkirtDepth),
                 proj.project(fx,        fy + 1.0f, kSkirtDepth),
                 shade(cell.r, cell.g, cell.b, 0.62f));
    }
    if (front_right) {
        const Vec2 s10 = proj.project(fx + 1.0f, fy,        -furrow);
        const Vec2 s11 = proj.project(fx + 1.0f, fy + 1.0f, -furrow);
        out.quad(s10, s11,
                 proj.project(fx + 1.0f, fy + 1.0f, kSkirtDepth),
                 proj.project(fx + 1.0f, fy,        kSkirtDepth),
                 shade(cell.r, cell.g, cell.b, 0.48f));
    }
}

void emit_harvester(MeshWriter& out, const IsoProjection& proj, FieldMesh& mesh, float hx, float hy) {
    // Footprint stays inside its own cell so the back-to-front cell loop keeps a
    // correct painter's order: crops in front of the harvester are drawn after it.
    constexpr float h_base = 0.06f;
    constexpr float h_top  = 0.48f;
    constexpr Rgba8 cab_top   { 230, 57,  70,  255 };
    constexpr Rgba8 cab_left  { 185, 42,  54,  255 };
    constexpr Rgba8 cab_right { 145, 32,  42,  255 };
    constexpr Rgba8 header    { 255, 209, 102, 255 };

    const float x0 = hx + 0.04f, x1 = hx + 0.96f;
    const float y0 = hy + 0.04f, y1 = hy + 0.80f;

    const Vec2 base01 = proj.project(x0, y1, h_base);
    const Vec2 base11 = proj.project(x1, y1, h_base);
    const Vec2 base10 = proj.project(x1, y0, h_base);
    const Vec2 top00  = proj.project(x0, y0, h_top);
    const Vec2 top10  = proj.project(x1, y0, h_top);
    const Vec2 top11  = proj.project(x1, y1, h_top);
    const Vec2 top01  = proj.project(x0, y1, h_top);

    out.quad(top01, top11, base11, base01, cab_left);
    out.quad(top10, top11, base11, base10, cab_right);
    out.quad(top00, top10, top11, top01, cab_top);

    // Golden cutter bar along the front of the harvester
    const float hh = h_base + 0.08f;
    out.quad(proj.project(hx,        hy + 0.80f, hh),
             proj.project(hx + 1.0f, hy + 0.80f, hh),
             proj.project(hx + 1.0f, hy + 1.00f, hh),
             proj.project(hx,        hy + 1.00f, hh),
             header);

    mesh.has_harvester = true;
    mesh.harvester_label_anchor = proj.project(hx + 0.5f, hy + 0.5f, h_top + 0.18f);
}

} // namespace

FieldMesh build_field_mesh(const generation::GridView& grid,
                           const IsoProjection& projection,
                           const Lighting& lighting) {
    FieldMesh mesh;
    const size_t n = grid.width();
    if (n < 3 || grid.height() != n) return mesh;

    mesh.triangles.reserve(n * n * 10);
    MeshWriter out(mesh);

    // Back-to-front row order doubles as painter's ordering for the isometric view,
    // as long as every object stays within its own cell footprint.
    for (size_t y = 0; y < n; ++y) {
        for (size_t x = 0; x < n; ++x) {
            const Cell& cell = grid(x, y);
            const float fx = static_cast<float>(x);
            const float fy = static_cast<float>(y);

            const bool  even_row  = (x % 2 == 0);
            const float furrow    = even_row ? 0.025f : -0.015f;
            const float slope_dir = even_row ? 1.0f : -1.0f;
            const float shade_k   = std::clamp(lighting.ambient + slope_dir * lighting.sun_dir_x * 0.12f, 0.35f, 1.12f);

            emit_soil(out, projection, cell, fx, fy, furrow, shade_k);

            switch (cell.role) {
                case CellRole::HarvesterActive:
                    emit_stubble(out, projection, cell, fx, fy, lighting.ambient);
                    emit_harvester(out, projection, mesh, fx, fy);
                    break;
                case CellRole::HarvestedStubble:
                    emit_stubble(out, projection, cell, fx, fy, lighting.ambient);
                    break;
                case CellRole::StandingCrop:
                    emit_crop(out, projection, cell, fx, fy, shade_k);
                    break;
                case CellRole::BorderHeadland:
                    break;
            }

            emit_skirt(out, projection, cell, fx, fy, furrow, y == n - 1, x == n - 1);
        }
    }

    return mesh;
}

} // namespace farmhorizon::geometry
