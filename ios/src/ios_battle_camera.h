#pragma once

#include <algorithm>
#include <cmath>

namespace th20::ios::camera {
// Shared by pretransformed sprites, projected geometry and CPU visibility
// checks. Apply in clip space once, before the render-texture Y flip.
struct Transform {
    float scale{1}, offset_x{}, offset_y{};
    bool identity() const { return scale == 1 && offset_x == 0 && offset_y == 0; }
    void apply(float& x, float& y, float w) const {
        x = scale * x + offset_x * w;
        y = scale * y + offset_y * w;
    }
    void project(float (&matrix)[4][4]) const {
        for (auto& row : matrix) apply(row[0], row[1], row[3]);
    }
    void source_bounds(const float* destination, float viewport_x, float viewport_y,
                       float width, float height, float* source) const {
        const float cx = viewport_x + width * .5f, cy = viewport_y + height * .5f;
        source[0] = cx + (destination[0] - cx - offset_x * width * .5f) / scale;
        source[2] = cx + (destination[2] - cx - offset_x * width * .5f) / scale;
        source[1] = cy + (destination[1] - cy + offset_y * height * .5f) / scale;
        source[3] = cy + (destination[3] - cy + offset_y * height * .5f) / scale;
    }
};
inline Transform make(float zoom, float anchor_x, float anchor_y,
                      float field_ratio_x = 1, float field_ratio_y = 1, float world_extent = 1) {
    if (!std::isfinite(zoom) || !std::isfinite(anchor_x) || !std::isfinite(anchor_y)) return {};
    zoom = std::clamp(zoom, .1f, 3.f);
    auto center = [zoom,world_extent](float anchor) {
        const float limit = std::max(0.f, world_extent - 1 / zoom);
        return std::clamp(anchor, -limit, limit);
    };
    return {zoom, -zoom * center(anchor_x) * field_ratio_x,
                  -zoom * center(anchor_y) * field_ratio_y};
}
// Renderer/main-thread state. Flush queued vertices before changing this.
// Render-target switches reset it so surface composition cannot zoom twice.
inline Transform draw_transform;
}
