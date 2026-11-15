// FarmHorizon — WebAssembly widget entry point.
// Owns the browser loop driver and the C exports called from the page (see
// apps/web/widget/farmhorizon-widget.ts). Sizes are in CSS pixels.
#include <algorithm>
#include <cstdlib>

#include <emscripten/emscripten.h>

#include "farmhorizon/app/app.hpp"

static farmhorizon::App g_app;

// Emscripten's GLFW listens for keys on the whole page and for the wheel on
// the canvas. The widget needs neither, and keeping them would swallow
// Backspace / Tab in page inputs and block page scrolling over the canvas.
EM_JS(void, release_page_input, (), {
    if (typeof GLFW === 'undefined') return;
    window.removeEventListener('keydown', GLFW.onKeydown, true);
    window.removeEventListener('keypress', GLFW.onKeyPress, true);
    window.removeEventListener('keyup', GLFW.onKeyup, true);
    const canvas = Module['canvas'];
    if (canvas) {
        canvas.removeEventListener('wheel', GLFW.onMouseWheel, true);
        canvas.removeEventListener('mousewheel', GLFW.onMouseWheel, true);
    }
});

extern "C" {

EMSCRIPTEN_KEEPALIVE void SetExternalSceneState(
    int field_size, int soil_type, float soil_moisture,
    int crop_type, float ndvi_index, float yield_ext, float harvested_pct
) {
    g_app.set_external_state(field_size, soil_type, soil_moisture,
                             crop_type, ndvi_index, yield_ext, harvested_pct);
}

EMSCRIPTEN_KEEPALIVE void SetTimeOfDay(float hours) {
    g_app.set_time_of_day(hours);
}

EMSCRIPTEN_KEEPALIVE void SetWidgetSize(int width, int height, float pixel_ratio) {
    g_app.resize(width, height, pixel_ratio);
}

EMSCRIPTEN_KEEPALIVE void SetWidgetOptions(int show_controls, int show_hud) {
    g_app.set_overlays(show_controls != 0, show_hud != 0);
}

EMSCRIPTEN_KEEPALIVE void SetBackgroundColor(int r, int g, int b) {
    g_app.set_background({ static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                           static_cast<unsigned char>(b), 255 });
}

} // extern "C"

// argv (from Module.arguments): [width, height, pixel_ratio], so the canvas
// starts at its final size instead of flashing a default one.
int main(int argc, char** argv) {
    farmhorizon::AppOptions options;
    options.show_controls = false; // The host page draws its own UI
    options.show_hud = false;

    int width = 800;
    int height = 480;
    if (argc > 2) {
        width = std::max(1, std::atoi(argv[1]));
        height = std::max(1, std::atoi(argv[2]));
    }
    if (argc > 3) options.pixel_ratio = static_cast<float>(std::atof(argv[3]));

    if (!g_app.init("FarmHorizon", width, height, options)) return 1;
    release_page_input();

    // Browser owns the loop; frame() is called once per animation frame.
    emscripten_set_main_loop([] { g_app.frame(); }, 0, 1);

    g_app.shutdown();
    return 0;
}
