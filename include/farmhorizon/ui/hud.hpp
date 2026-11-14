#pragma once

#include "generation_config.hpp"

namespace farmhorizon::ui {

/// Top status bar: title, renderer name, current field summary and FPS.
void draw_hud(const generation::GenConfig& config, const char* renderer_name);

} // namespace farmhorizon::ui
