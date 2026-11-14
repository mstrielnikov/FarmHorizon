#pragma once

#include "farmhorizon/core/field_state.hpp"

namespace farmhorizon::ui {

/// Top status bar: title, renderer name, current field summary and FPS.
void draw_hud(const core::FieldState& state, const char* renderer_name);

} // namespace farmhorizon::ui
