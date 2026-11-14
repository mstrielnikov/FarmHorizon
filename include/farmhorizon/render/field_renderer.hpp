#pragma once

#include <raylib.h>

#include "farmhorizon/geometry/field_mesh.hpp"

namespace farmhorizon::render {

/**
 * @brief Draws a FieldMesh with raylib.
 *
 * The mesh is baked once into an off-screen RenderTexture (upload) and then
 * blitted every frame with a single textured quad (draw). Owns no scene state:
 * it only turns geometry into pixels.
 *
 * GPU resources must be freed with release() while the window is still open;
 * the destructor deliberately does not touch the GL context.
 */
class FieldRenderer {
public:
    FieldRenderer() = default;
    FieldRenderer(const FieldRenderer&) = delete;
    FieldRenderer& operator=(const FieldRenderer&) = delete;

    /// Bakes the mesh into the off-screen texture. Call outside BeginDrawing().
    void upload(const geometry::FieldMesh& mesh);

    /// Draws the baked field and world-space labels through the given camera.
    void draw(const Camera2D& camera) const;

    /// Frees the render texture. Call before CloseWindow().
    void release();

    bool ready() const noexcept { return target_.id > 0; }

private:
    static constexpr float kMargin      = 16.0f;   ///< Padding around the mesh bounds, in world pixels
    static constexpr float kMaxTexture  = 8192.0f; ///< Safe texture limit for most GPUs / WebGL2

    RenderTexture2D target_{};
    int tex_w_ = 0;
    int tex_h_ = 0;
    Rectangle world_rect_{};       ///< Where the texture lands in world space
    bool has_label_ = false;
    Vector2 label_anchor_{};
};

} // namespace farmhorizon::render
