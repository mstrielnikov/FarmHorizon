#include "farmhorizon/ui/control_panel.hpp"

#include <algorithm>
#include <cmath>

#include <raygui.h>

#include "farmhorizon/core/presets.hpp"

namespace farmhorizon::ui {

namespace {

constexpr Color kYieldGreen   { 126, 231, 135, 255 };
constexpr Color kHarvestAmber { 255, 184, 108, 255 };

constexpr int kRowStep  = 35;
constexpr int kLabelX   = 15;
constexpr int kSliderX  = 110;
constexpr int kSliderW  = 140;
constexpr int kSliderH  = 22;
constexpr int kFontSize = 16;

/// One labelled slider row; advances `y` to the next row.
void slider_row(int panel_x, int& y, const char* label, const char* value_text,
                float* value, float min, float max, Color color = RAYWHITE) {
    DrawText(label, panel_x + kLabelX, y, kFontSize, color);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(color));
    GuiSliderBar({ static_cast<float>(panel_x + kSliderX), static_cast<float>(y - 3), kSliderW, kSliderH },
                 "", value_text, value, min, max);
    y += kRowStep;
}

} // namespace

Rectangle ControlPanel::bounds() const {
    return {
        static_cast<float>(GetScreenWidth() - kWidth - kMargin),
        static_cast<float>(kTop),
        static_cast<float>(kWidth),
        static_cast<float>(kHeight)
    };
}

bool ControlPanel::wants_mouse() const {
    return CheckCollisionPointRec(GetMousePosition(), bounds());
}

PanelResult ControlPanel::draw(core::FieldState& state, core::ViewSettings& view, float& zoom) {
    PanelResult result;

    // Keep float slider proxies aligned if state was updated externally.
    if (static_cast<int>(std::lround(size_slider_)) != state.field_size) {
        size_slider_ = static_cast<float>(state.field_size);
    }
    if (core::soil_type_from_index(static_cast<int>(std::lround(soil_slider_))) != state.soil_type) {
        soil_slider_ = static_cast<float>(state.soil_type);
    }
    if (core::crop_type_from_index(static_cast<int>(std::lround(crop_slider_))) != state.crop_type) {
        crop_slider_ = static_cast<float>(state.crop_type);
    }

    const Rectangle r = bounds();
    const int x = static_cast<int>(r.x);
    DrawRectangleRec(r, Fade(BLACK, 0.85f));

    GuiSetStyle(DEFAULT, TEXT_SIZE, kFontSize);

    int y = static_cast<int>(r.y) + 15;
    slider_row(x, y, "Zoom",        TextFormat("%.2f", zoom),                         &zoom,               0.5f,  1.8f);
    slider_row(x, y, "Time of Day", TextFormat("%.1f", view.time_of_day),             &view.time_of_day,   6.0f,  18.0f);
    slider_row(x, y, "Field Size",  TextFormat("%dx%d", state.field_size, state.field_size), &size_slider_, 12.0f, 64.0f);
    slider_row(x, y, "Soil Type",   core::soil_name(state.soil_type),                 &soil_slider_,       0.0f,  3.0f);
    slider_row(x, y, "Soil Moist",  TextFormat("%.0f%%", state.soil_moisture),        &state.soil_moisture, 10.0f, 60.0f);
    slider_row(x, y, "Crop Type",   core::crop_name(state.crop_type),                 &crop_slider_,       0.0f,  3.0f);
    slider_row(x, y, "NDVI Index",  TextFormat("%.2f", state.ndvi_index),             &state.ndvi_index,   0.10f, 0.85f);
    slider_row(x, y, "Yield (Ext)", TextFormat("%.0ft", state.yield_ext),             &state.yield_ext,    10.0f, 70.0f, kYieldGreen);
    slider_row(x, y, "Harvested",   TextFormat("%.0f%%", state.harvested_pct),        &state.harvested_pct, 0.0f, 100.0f, kHarvestAmber);

    state.field_size = std::clamp(static_cast<int>(std::lround(size_slider_)), 12, 64);
    state.soil_type  = core::soil_type_from_index(static_cast<int>(std::lround(soil_slider_)));
    state.crop_type  = core::crop_type_from_index(static_cast<int>(std::lround(crop_slider_)));

    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(RAYWHITE));

    y += 1;
    if (GuiButton({ static_cast<float>(x + 15), static_cast<float>(y), 175, 28 }, "Cycle Crop Preset")) {
        preset_idx_ = (preset_idx_ + 1) % core::kPresetCount;
        core::apply_crop_preset(state, preset_idx_);
        soil_slider_ = static_cast<float>(state.soil_type);
        crop_slider_ = static_cast<float>(state.crop_type);
    }
    if (GuiButton({ static_cast<float>(x + 198), static_cast<float>(y), 97, 28 }, "Reset View")) {
        result.reset_view = true;
    }

    return result;
}

} // namespace farmhorizon::ui
