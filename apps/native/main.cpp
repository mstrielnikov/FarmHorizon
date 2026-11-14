// FarmHorizon — native platform entry point.
// Only owns window flags, the loop driver and the C export shim.
#include <raylib.h>

#include "farmhorizon/app/app.hpp"

static farmhorizon::App g_app;

extern "C" void SetExternalSceneState(
    int field_size, int soil_type, float soil_moisture,
    int crop_type, float ndvi_index, float yield_ext, float harvested_pct
) {
    g_app.set_external_state(field_size, soil_type, soil_moisture,
                             crop_type, ndvi_index, yield_ext, harvested_pct);
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    if (!g_app.init("FarmHorizon", 1200, 840)) return 1;
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        g_app.frame();
    }

    g_app.shutdown();
    return 0;
}
