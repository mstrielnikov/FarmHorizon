#pragma once

#include "farmhorizon/core/field_state.hpp"

namespace farmhorizon::ui {

inline constexpr int kHudHeight = 60; ///< Height of the top status bar, in pixels

/// Top status bar: title, renderer name, current field summary and FPS.
void draw_hud(const core::FieldState& state, const char* renderer_name);

} // namespace farmhorizon::ui
