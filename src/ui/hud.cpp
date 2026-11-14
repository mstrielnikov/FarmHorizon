#include "farmhorizon/ui/hud.hpp"

#include <raylib.h>

namespace farmhorizon::ui {

void draw_hud(const core::FieldState& state, const char* renderer_name) {
    DrawRectangle(0, 0, GetScreenWidth(), 60, Fade(BLACK, 0.8f));
    DrawText("FarmHorizon", 20, 10, 20, GOLD);
    DrawText(
        TextFormat(
            "Renderer: %s | Soil: %s (%.0f%%) | Crop: %s | Ext Yield: %.1f t/ha",
            renderer_name,
            core::soil_name(state.soil_type),
            state.soil_moisture,
            core::crop_name(state.crop_type),
            state.yield_ext
        ),
        20, 35, 14, LIME
    );
    DrawFPS(GetScreenWidth() - 80, 10);
}

} // namespace farmhorizon::ui
