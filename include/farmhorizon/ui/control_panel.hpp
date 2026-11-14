#pragma once

#include <raylib.h>

#include "generation_config.hpp"

namespace farmhorizon::ui {

/// Requests the panel makes to the app; the panel never touches the camera itself.
struct PanelResult {
    bool reset_view = false;
};

/**
 * @brief Floating top-right parameter panel (immediate-mode, raygui).
 *
 * Drawn in screen space after the world pass. Sliders edit the scene config in
 * place; the app detects changes and rebuilds whatever depends on them.
 */
class ControlPanel {
public:
    /// Screen-space rectangle of the panel for the current window size.
    Rectangle bounds() const;

    /// True when the mouse is over the panel, so world input (pan/zoom) should be ignored.
    bool wants_mouse() const;

    PanelResult draw(generation::GenConfig& config, float& zoom);

private:
    static constexpr int kWidth  = 310;
    static constexpr int kHeight = 405;
    static constexpr int kMargin = 20;  ///< Gap to the right window edge
    static constexpr int kTop    = 80;  ///< Below the status bar

    int preset_idx_ = 0;
};

} // namespace farmhorizon::ui
