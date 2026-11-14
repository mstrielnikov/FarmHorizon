// Geometry tests: projection round-trip, mesh size, winding, bounds, draw order.
#include "test_runner.hpp"

#include "farmhorizon/core/field_grid.hpp"
#include "farmhorizon/geometry/field_mesh.hpp"
#include "farmhorizon/geometry/iso_projection.hpp"

using namespace farmhorizon::core;
using namespace farmhorizon::geometry;

namespace {

FieldMesh build(int n, const FieldState& state, float hours = 14.5f) {
    FieldGrid grid = *FieldGrid::create(static_cast<size_t>(n), static_cast<size_t>(n));
    populate_field_grid(grid, state);
    return build_field_mesh(grid.view(), IsoProjection{}, Lighting::from_time_of_day(hours));
}

bool same_color(Rgba8 a, Rgba8 b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

constexpr Rgba8 kCabTop{ 230, 57, 70, 255 };

} // namespace

TEST(projection_round_trips) {
    const IsoProjection proj;
    const float pts[][3] = { { 0, 0, 0 }, { 7, 12, 0 }, { 3.5f, 1.25f, 0.4f }, { 63, 2, -0.3f } };
    for (const auto& p : pts) {
        const Vec2 back = proj.unproject(proj.project(p[0], p[1], p[2]), p[2]);
        CHECK_NEAR(back.x, p[0], 1e-4f);
        CHECK_NEAR(back.y, p[1], 1e-4f);
    }
}

TEST(projection_matches_classic_2_to_1_iso) {
    const Vec2 s = IsoProjection{}.project(3.0f, 1.0f, 0.5f);
    CHECK_NEAR(s.x, (3.0f - 1.0f) * 32.0f, 1e-5f);
    CHECK_NEAR(s.y, (3.0f + 1.0f) * 16.0f - 0.5f * 80.0f, 1e-5f);
}

TEST(mesh_triangle_count_for_default_field) {
    // 22x22 at 30%: soil 484*2 + stubble 121*2 + crops 279*6 + skirt 44*2 + harvester 4*2
    FieldState state;
    state.harvested_pct = 30.0f;
    const FieldMesh mesh = build(22, state);
    CHECK_EQ(mesh.triangles.size(), size_t(968 + 242 + 1674 + 88 + 8));
    CHECK(mesh.has_harvester);
}

TEST(all_triangles_share_one_winding) {
    FieldState state;
    state.harvested_pct = 45.0f;
    const FieldMesh mesh = build(18, state);
    for (const Triangle& t : mesh.triangles) {
        CHECK(cross(t.p[0], t.p[1], t.p[2]) <= 0.0f);
    }
}

TEST(bounds_contain_every_vertex) {
    const FieldMesh mesh = build(20, FieldState{});
    CHECK(mesh.bounds.valid);
    for (const Triangle& t : mesh.triangles) {
        for (const Vec2& p : t.p) {
            CHECK(p.x >= mesh.bounds.min.x && p.x <= mesh.bounds.max.x);
            CHECK(p.y >= mesh.bounds.min.y && p.y <= mesh.bounds.max.y);
        }
    }
}

TEST(no_harvester_geometry_without_active_cell) {
    FieldState state;
    state.harvested_pct = 0.0f;
    const FieldMesh mesh = build(16, state);
    CHECK(!mesh.has_harvester);
    for (const Triangle& t : mesh.triangles) CHECK(!same_color(t.color, kCabTop));
}

TEST(crops_in_front_are_drawn_after_harvester) {
    // Regression: the harvester used to be appended after the whole field,
    // painting over taller crops standing in front of it.
    FieldState state;
    state.harvested_pct = 30.0f;
    state.yield_ext = 65.0f;
    const FieldMesh mesh = build(22, state);

    size_t cab = mesh.triangles.size();
    for (size_t i = 0; i < mesh.triangles.size(); ++i) {
        if (same_color(mesh.triangles[i].color, kCabTop)) { cab = i; break; }
    }
    CHECK(cab < mesh.triangles.size());

    // Everything in rows y > 7 (in front of the harvester) must come after it:
    // at least the remaining 13 full rows of crops plus the skirt.
    const size_t after = mesh.triangles.size() - cab;
    CHECK(after > size_t(13 * 20 * 6));
}

TEST(lighting_is_brightest_at_noon) {
    const Lighting dawn = Lighting::from_time_of_day(6.0f);
    const Lighting noon = Lighting::from_time_of_day(12.0f);
    const Lighting dusk = Lighting::from_time_of_day(18.0f);
    CHECK(noon.ambient > dawn.ambient);
    CHECK(noon.ambient > dusk.ambient);
    CHECK(dawn.sun_dir_x > 0.0f);
    CHECK(dusk.sun_dir_x < 0.0f);
}
