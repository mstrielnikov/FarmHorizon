#include "farmhorizon/ui/hud.hpp"

#include <cmath>

#include <raylib.h>

namespace farmhorizon::ui {

void draw_hud(const generation::GenConfig& config, const char* renderer_name) {
    using namespace generation;

    DrawRectangle(0, 0, GetScreenWidth(), 60, Fade(BLACK, 0.8f));
    DrawText("FarmHorizon", 20, 10, 20, GOLD);
    DrawText(
        TextFormat(
            "Renderer: %s | Soil: %s (%.0f%%) | Crop: %s | Ext Yield: %.1f t/ha",
            renderer_name,
            get_soil_name((int)std::round(config.soil_type)),
            config.soil_moisture,
            get_crop_name((int)std::round(config.crop_type)),
            config.yield_ext
        ),
        20, 35, 14, LIME
    );
    DrawFPS(GetScreenWidth() - 80, 10);
}

} // namespace farmhorizon::ui
