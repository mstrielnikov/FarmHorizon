#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

/**
 * @file math.hpp
 * @brief Minimal renderer-agnostic math PODs for the geometry layer.
 *
 * Deliberately free of raylib so geometry can be built and tested headless.
 * Layouts match raylib's Vector2 / Color, so conversion is a trivial copy.
 */
namespace farmhorizon::geometry {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Rgba8 {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return { a.x + b.x, a.y + b.y }; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return { a.x - b.x, a.y - b.y }; }
constexpr Vec2 operator*(Vec2 a, float s) noexcept { return { a.x * s, a.y * s }; }

/// Z component of (b - a) x (c - a); sign gives the screen-space winding.
constexpr float cross(Vec2 a, Vec2 b, Vec2 c) noexcept {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

/// Multiplies an RGB colour by a brightness factor, clamped to [0, 255].
inline Rgba8 shade(uint8_t r, uint8_t g, uint8_t b, float factor) noexcept {
    auto ch = [factor](uint8_t v) {
        return static_cast<uint8_t>(std::clamp(std::round(v * factor), 0.0f, 255.0f));
    };
    return { ch(r), ch(g), ch(b), 255 };
}

struct Triangle {
    Vec2 p[3];
    Rgba8 color;
};

/// Axis-aligned 2D bounds; `valid` is false until the first point is added.
struct Bounds2 {
    Vec2 min{ 1e9f, 1e9f };
    Vec2 max{ -1e9f, -1e9f };
    bool valid = false;

    void add(Vec2 p) noexcept {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
        valid = true;
    }

    float width()  const noexcept { return valid ? max.x - min.x : 0.0f; }
    float height() const noexcept { return valid ? max.y - min.y : 0.0f; }
};

} // namespace farmhorizon::geometry
