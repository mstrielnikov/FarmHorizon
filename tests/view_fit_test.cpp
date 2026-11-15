// View fitting tests: the frozen camera keeps any field fully visible and centred.
#include "test_runner.hpp"

#include "farmhorizon/core/field_grid.hpp"
#include "farmhorizon/geometry/field_mesh.hpp"
#include "farmhorizon/geometry/view_fit.hpp"

using namespace farmhorizon::core;
using namespace farmhorizon::geometry;

namespace {

Bounds2 box(float x0, float y0, float x1, float y1) {
    Bounds2 b;
    b.add({ x0, y0 });
    b.add({ x1, y1 });
    return b;
}

FieldMesh mesh_for(int n) {
    FieldGrid grid = *FieldGrid::create(static_cast<size_t>(n), static_cast<size_t>(n));
    FieldState state;
    state.field_size = n;
    populate_field_grid(grid, state);
    return build_field_mesh(grid.view(), IsoProjection{}, Lighting::from_time_of_day(12.0f));
}

} // namespace

TEST(wide_bounds_fill_the_viewport_width) {
    const ViewFit fit = fit_bounds(box(0, 0, 200, 100), { 0, 0, 400, 400 }, 0.0f);
    CHECK_NEAR(fit.scale, 2.0f, 1e-5f);
    const Vec2 a = fit.to_screen({ 0, 0 });
    const Vec2 b = fit.to_screen({ 200, 100 });
    CHECK_NEAR(a.x, 0.0f, 1e-4f);
    CHECK_NEAR(b.x, 400.0f, 1e-4f);
    CHECK_NEAR(a.y, 100.0f, 1e-4f); // vertically centred
    CHECK_NEAR(b.y, 300.0f, 1e-4f);
}

TEST(tall_bounds_fill_the_viewport_height) {
    const ViewFit fit = fit_bounds(box(-50, -100, 50, 100), { 0, 0, 400, 100 }, 0.0f);
    CHECK_NEAR(fit.scale, 0.5f, 1e-5f);
    const Vec2 c = fit.to_screen({ 0, 0 });
    CHECK_NEAR(c.x, 200.0f, 1e-4f);
    CHECK_NEAR(c.y, 50.0f, 1e-4f);
}

TEST(padding_and_offset_are_respected) {
    const Viewport vp{ 50, 60, 420, 420 };
    const ViewFit fit = fit_bounds(box(0, 0, 200, 100), vp, 10.0f);
    CHECK_NEAR(fit.scale, 2.0f, 1e-5f);
    const Vec2 a = fit.to_screen({ 0, 0 });
    CHECK_NEAR(a.x, 60.0f, 1e-4f);  // 50 + 10 padding
    const Vec2 c = fit.to_screen({ 100, 50 });
    CHECK_NEAR(c.x, 260.0f, 1e-4f); // viewport centre
    CHECK_NEAR(c.y, 270.0f, 1e-4f);
}

TEST(nothing_to_fit_gives_invalid_view) {
    CHECK(!fit_bounds(Bounds2{}, { 0, 0, 100, 100 }, 0.0f).valid());
    CHECK(!fit_bounds(box(0, 0, 1, 1), { 0, 0, 0, 100 }, 0.0f).valid());
}

TEST(small_and_large_fields_both_fit_the_same_widget) {
    const Viewport vp{ 0, 0, 640, 400 };
    const float pad = 16.0f;
    float scales[2];
    int i = 0;
    for (int n : { 12, 64 }) {
        const FieldMesh mesh = mesh_for(n);
        const ViewFit fit = fit_bounds(mesh.bounds, vp, pad);
        CHECK(fit.valid());
        for (const Triangle& t : mesh.triangles) {
            for (const Vec2& p : t.p) {
                const Vec2 s = fit.to_screen(p);
                CHECK(s.x >= pad - 1e-3f && s.x <= vp.width - pad + 1e-3f);
                CHECK(s.y >= pad - 1e-3f && s.y <= vp.height - pad + 1e-3f);
            }
        }
        scales[i++] = fit.scale;
    }
    CHECK(scales[0] > scales[1]); // the small field is drawn bigger
}
