#pragma once

#include <cstdint>
#include <string>

#if defined(PLATFORM_WEB)

namespace farmhorizon::platform {

inline std::string get_gpu_name() {
    return "WebGL (Wasm / Emscripten)";
}

inline float get_vram_usage_percent() {
    // VRAM metrics are not securely accessible via browser APIs
    return 0.0f;
}

} // namespace farmhorizon::platform

#else

#include <fstream>
extern "C" const unsigned char* glGetString(unsigned int name);

namespace farmhorizon::platform {

inline std::string get_gpu_name() {
    // 0x1F01 corresponds to GL_RENDERER
    const char* renderer = reinterpret_cast<const char*>(glGetString(0x1F01));
    return renderer ? std::string(renderer) : "Unknown Native GPU";
}

inline float get_vram_usage_percent() {
    auto read_bytes = [](const char* path, uint64_t fallback) -> uint64_t {
        std::ifstream file(path);
        uint64_t bytes = 0;
        return (file >> bytes) ? bytes : fallback;
    };

    const uint64_t total = read_bytes("/sys/class/drm/card0/device/mem_info_vram_total",
                                      3072ULL * 1024ULL * 1024ULL);
    if (total == 0) return 0.0f;
    const uint64_t used = read_bytes("/sys/class/drm/card0/device/mem_info_vram_used", 0);
    return static_cast<float>(used) / static_cast<float>(total) * 100.0f;
}

} // namespace farmhorizon::platform

#endif
