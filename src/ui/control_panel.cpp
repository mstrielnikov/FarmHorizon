#include "farmhorizon/ui/control_panel.hpp"

#include <cmath>

#include <raygui.h>

namespace farmhorizon::ui {

namespace {

constexpr Color kYieldGreen   { 126, 231, 135, 255 };
constexpr Color kHarvestAmber { 255, 184, 108, 255 };

constexpr int kRowStep     = 35;
constexpr int kLabelX      = 15;
constexpr int kSliderX     = 110;
constexpr int kSliderW     = 140;
constexpr int kSliderH     = 22;
constexpr int kFontSize    = 16;

/// One labelled slider row; advances `y` to the next row.
void slider_row(int panel_x, int& y, const char* label, const char* value_text,
                float* value, float min, float max, Color color = RAYWHITE) {
    DrawText(label, panel_x + kLabelX, y, kFontSize, color);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(color));
    GuiSliderBar({ (float)(panel_x + kSliderX), (float)(y - 3), kSliderW, kSliderH },
                 "", value_text, value, min, max);
    y += kRowStep;
}

} // namespace

Rectangle ControlPanel::bounds() const {
    return { (float)(GetScreenWidth() - kWidth - kMargin), (float)kTop, (float)kWidth, (float)kHeight };
}

bool ControlPanel::wants_mouse() const {
    return CheckCollisionPointRec(GetMousePosition(), bounds());
}

PanelResult ControlPanel::draw(generation::GenConfig& config, float& zoom) {
    using namespace generation;
    PanelResult result;

    const Rectangle r = bounds();
    const int x = (int)r.x;
    DrawRectangleRec(r, Fade(BLACK, 0.85f));

    GuiSetStyle(DEFAULT, TEXT_SIZE, kFontSize);

    const int soil = (int)std::round(config.soil_type);
    const int crop = (int)std::round(config.crop_type);
    const int size = (int)std::round(config.field_size);

    int y = (int)r.y + 15;
    slider_row(x, y, "Zoom",        TextFormat("%.2f", zoom),                &zoom,                 0.5f,  1.8f);
    slider_row(x, y, "Time of Day", TextFormat("%.1f", config.time_of_day), &config.time_of_day,   6.0f,  18.0f);
    slider_row(x, y, "Field Size",  TextFormat("%dx%d", size, size),        &config.field_size,    12.0f, 64.0f);
    slider_row(x, y, "Soil Type",   get_soil_name(soil),                    &config.soil_type,     0.0f,  3.0f);
    slider_row(x, y, "Soil Moist",  TextFormat("%.0f%%", config.soil_moisture), &config.soil_moisture, 10.0f, 60.0f);
    slider_row(x, y, "Crop Type",   get_crop_name(crop),                    &config.crop_type,     0.0f,  3.0f);
    slider_row(x, y, "NDVI Index",  TextFormat("%.2f", config.ndvi_index),  &config.ndvi_index,    0.10f, 0.85f);
    slider_row(x, y, "Yield (Ext)", TextFormat("%.0ft", config.yield_ext),  &config.yield_ext,     10.0f, 70.0f, kYieldGreen);
    slider_row(x, y, "Harvested",   TextFormat("%.0f%%", config.harvested_pct), &config.harvested_pct, 0.0f, 100.0f, kHarvestAmber);

    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(RAYWHITE));

    y += 1;
    if (GuiButton({ (float)(x + 15), (float)y, 175, 28 }, "Cycle Crop Preset")) {
        preset_idx_ = (preset_idx_ + 1) % 4;
        apply_crop_preset(config, preset_idx_);
    }
    if (GuiButton({ (float)(x + 198), (float)y, 97, 28 }, "Reset View")) {
        result.reset_view = true;
    }

    return result;
}

} // namespace farmhorizon::ui
