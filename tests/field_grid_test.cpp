// Field grid tests: cell roles, boustrophedon harvest order, colour mapping.
#include "test_runner.hpp"

#include "generation/grid.hpp"

using namespace generation;

namespace {

Grid make_grid(int n, const GenConfig& cfg) {
    Grid grid = *Grid::create(n, n);
    populate_field_grid(grid, cfg);
    return grid;
}

int count_role(const Grid& grid, CellRole role) {
    int count = 0;
    for (const Cell& c : grid.cells()) count += (c.role == role);
    return count;
}

bool find_role(const Grid& grid, CellRole role, int& out_x, int& out_y) {
    for (size_t y = 0; y < grid.height(); ++y)
        for (size_t x = 0; x < grid.width(); ++x)
            if (grid(x, y).role == role) { out_x = (int)x; out_y = (int)y; return true; }
    return false;
}

} // namespace

TEST(border_cells_are_headland) {
    const int n = 22;
    Grid grid = make_grid(n, GenConfig{});
    for (int i = 0; i < n; ++i) {
        CHECK(grid(i, 0).role == CellRole::BorderHeadland);
        CHECK(grid(i, n - 1).role == CellRole::BorderHeadland);
        CHECK(grid(0, i).role == CellRole::BorderHeadland);
        CHECK(grid(n - 1, i).role == CellRole::BorderHeadland);
    }
    CHECK_EQ(count_role(grid, CellRole::BorderHeadland), 4 * (n - 1));
}

TEST(harvested_cell_count_matches_percentage) {
    GenConfig cfg;
    cfg.harvested_pct = 30.0f;
    Grid grid = make_grid(22, cfg); // 20x20 interior = 400 plot cells
    CHECK_EQ(count_role(grid, CellRole::HarvestedStubble), 120);
    CHECK_EQ(count_role(grid, CellRole::HarvesterActive), 1);
    CHECK_EQ(count_role(grid, CellRole::StandingCrop), 400 - 120 - 1);
}

TEST(no_harvester_when_untouched_or_finished) {
    GenConfig cfg;
    cfg.harvested_pct = 0.0f;
    Grid fresh = make_grid(22, cfg);
    CHECK_EQ(count_role(fresh, CellRole::HarvesterActive), 0);
    CHECK_EQ(count_role(fresh, CellRole::HarvestedStubble), 0);

    cfg.harvested_pct = 100.0f;
    Grid done = make_grid(22, cfg);
    CHECK_EQ(count_role(done, CellRole::HarvesterActive), 0);
    CHECK_EQ(count_role(done, CellRole::StandingCrop), 0);
    CHECK_EQ(count_role(done, CellRole::HarvestedStubble), 400);
}

TEST(harvester_starts_even_rows_on_the_left) {
    GenConfig cfg;
    cfg.harvested_pct = 30.0f; // 120 cells = 6 full rows -> next row (y=7) is even, starts at x=1
    Grid grid = make_grid(22, cfg);
    int hx = -1, hy = -1;
    CHECK(find_role(grid, CellRole::HarvesterActive, hx, hy));
    CHECK_EQ(hx, 1);
    CHECK_EQ(hy, 7);
}

TEST(harvester_reverses_on_odd_rows) {
    GenConfig cfg;
    cfg.harvested_pct = 7.5f; // 30 cells: row 0 full (20), row 1 has 10 cut right-to-left
    Grid grid = make_grid(22, cfg);
    int hx = -1, hy = -1;
    CHECK(find_role(grid, CellRole::HarvesterActive, hx, hy));
    CHECK_EQ(hy, 2);
    CHECK_EQ(hx, 10);
    // Cells to the right of the harvester in that row are already cut.
    CHECK(grid(20, 2).role == CellRole::HarvestedStubble);
    CHECK(grid(1, 2).role == CellRole::StandingCrop);
}

TEST(higher_yield_grows_taller_crops) {
    GenConfig low, high;
    low.harvested_pct = high.harvested_pct = 0.0f;
    low.yield_ext = 15.0f;
    high.yield_ext = 65.0f;
    Grid a = make_grid(16, low);
    Grid b = make_grid(16, high);
    CHECK(b(8, 8).crop_height > a(8, 8).crop_height);
    CHECK(b(8, 8).canopy_density >= a(8, 8).canopy_density);
}

TEST(wetter_soil_is_darker) {
    GenConfig dry, wet;
    dry.soil_moisture = 12.0f;
    wet.soil_moisture = 55.0f;
    Grid a = make_grid(16, dry);
    Grid b = make_grid(16, wet);
    // Compare a headland cell, which always shows bare soil.
    CHECK(b(0, 0).r < a(0, 0).r);
    CHECK(b(0, 0).g < a(0, 0).g);
}
