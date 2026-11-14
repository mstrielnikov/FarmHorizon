#pragma once

namespace farmhorizon {

/**
 * @brief Platform-agnostic application core.
 *
 * Owns scene state, rendering and UI. Platform entry points only create the
 * window flags, drive the loop by calling frame(), and forward external calls.
 */
class App {
public:
    bool init(const char* title, int width, int height);
    void frame();
    void shutdown();

    /// Bridge for an external math/ML engine pushing new field state.
    void set_external_state(int field_size, int soil_type, float soil_moisture,
                            int crop_type, float ndvi_index, float yield_ext,
                            float harvested_pct);
};

} // namespace farmhorizon
