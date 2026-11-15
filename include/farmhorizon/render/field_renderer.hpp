#pragma once

#include <raylib.h>

#include "farmhorizon/geometry/field_mesh.hpp"
#include "farmhorizon/geometry/view_fit.hpp"

namespace farmhorizon::render {

/**
 * @brief Draws a FieldMesh with raylib.
 *
 * The mesh is baked once into an off-screen RenderTexture (upload) at the
 * fitted screen scale, then blitted 1:1 every frame with a single textured
 * quad (draw), so the field stays crisp at any widget size. Owns no scene
 * state: it only turns geometry into pixels.
 *
 * GPU resources must be freed with release() while the window is still open;
 * the destructor deliberately does not touch the GL context.
 */
class FieldRenderer {
public:
    FieldRenderer() = default;
    FieldRenderer(const FieldRenderer&) = delete;
    FieldRenderer& operator=(const FieldRenderer&) = delete;

    /// Bakes the mesh as it appears under `fit`. Call outside BeginDrawing().
    void upload(const geometry::FieldMesh& mesh, const geometry::ViewFit& fit);

    /// Draws the baked field and the harvester label in screen space.
    /// `ui_scale` scales label text (e.g. the device pixel ratio).
    void draw(float ui_scale = 1.0f) const;

    /// Frees the render texture. Call before CloseWindow().
    void release();

    bool ready() const noexcept { return target_.id > 0; }

private:
    static constexpr float kMaxTexture = 8192.0f; ///< Safe texture limit for most GPUs / WebGL2
    static constexpr int   kBleed      = 1;       ///< Extra texels around the mesh so edges are not clipped

    RenderTexture2D target_{};
    int tex_w_ = 0;
    int tex_h_ = 0;
    Rectangle screen_rect_{};      ///< Where the texture lands on screen
    bool has_label_ = false;
    Vector2 label_anchor_{};       ///< Screen-space anchor above the harvester cab
};

} // namespace farmhorizon::render
