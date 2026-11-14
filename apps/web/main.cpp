// FarmHorizon — WebAssembly platform entry point.
// Only owns the browser loop driver and the exported JS bridge.
#include <emscripten/emscripten.h>

#include "farmhorizon/app/app.hpp"

static farmhorizon::App g_app;

extern "C" EMSCRIPTEN_KEEPALIVE void SetExternalSceneState(
    int field_size, int soil_type, float soil_moisture,
    int crop_type, float ndvi_index, float yield_ext, float harvested_pct
) {
    g_app.set_external_state(field_size, soil_type, soil_moisture,
                             crop_type, ndvi_index, yield_ext, harvested_pct);
}

int main() {
    if (!g_app.init("FarmHorizon - Web Edition", 1200, 840)) return 1;

    // Browser owns the loop; frame() is called once per animation frame.
    emscripten_set_main_loop([] { g_app.frame(); }, 0, 1);

    g_app.shutdown();
    return 0;
}
