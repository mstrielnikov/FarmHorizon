#pragma once

#include <raylib.h>

#include "farmhorizon/core/field_state.hpp"

namespace farmhorizon::ui {

/// Requests the panel makes to the app; the panel never touches the camera itself.
struct PanelResult {
    bool reset_view = false;
};

/**
 * @brief Floating top-right parameter panel (immediate-mode, raygui).
 *
 * Drawn in screen space after the world pass. Owns the float slider proxies
 * needed by raygui for discrete integer / enum fields so the core model stays
 * strongly typed.
 */
class ControlPanel {
public:
    /// Screen-space rectangle of the panel for the current window size.
    Rectangle bounds() const;

    /// True when the mouse is over the panel, so world input (pan/zoom) should be ignored.
    bool wants_mouse() const;

    PanelResult draw(core::FieldState& state, core::ViewSettings& view, float& zoom);

private:
    static constexpr int kWidth  = 310;
    static constexpr int kHeight = 405;
    static constexpr int kMargin = 20; ///< Gap to the right window edge
    static constexpr int kTop    = 80; ///< Below the status bar

    int   preset_idx_  = 0;
    float size_slider_ = 22.0f;
    float soil_slider_ = 1.0f;
    float crop_slider_ = 0.0f;
};

} // namespace farmhorizon::ui
