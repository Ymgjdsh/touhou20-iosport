#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace th20::ios::presentation {
struct Point { double x{}, y{}; };
struct Rect {
    double x{}, y{}, width{}, height{};
    bool contains(Point p) const {
        return p.x >= x && p.y >= y && p.x < x + width && p.y < y + height;
    }
};
struct Region { Rect source, destination; };
struct Layout {
    std::array<Region, 3> regions{};
    unsigned count{1};
    bool portrait_battle{};

    Point logical_point(Point point) const {
        // Relative movement may continue outside its original region, while
        // taps on the two header panels still map to their original HUD rows.
        const Region* region = &regions[portrait_battle ? 2 : 0];
        for (unsigned i = 0; i != count; ++i)
            if (regions[i].destination.contains(point)) { region = &regions[i]; break; }
        return {region->source.x + (point.x - region->destination.x) * region->source.width / region->destination.width,
                region->source.y + (point.y - region->destination.y) * region->source.height / region->destination.height};
    }
    Point logical_delta(Point delta) const {
        const auto& region = regions[portrait_battle ? 2 : 0];
        return {delta.x * region.source.width / region.destination.width,
                delta.y * region.source.height / region.destination.height};
    }
};

inline Layout make_layout(double width, double height, double source_width, double source_height,
                          bool battle, double safe_top = 0) {
    Layout layout;
    width = std::max(1.0, width); height = std::max(2.0, height);
    source_width = std::max(1.0, source_width); source_height = std::max(1.0, source_height);
    if (battle && height > width && std::abs(source_width * 3 - source_height * 4) < 1e-8) {
        // TH20 set_render_offsets() uses (32,16,384,448), backed by the
        // original constants at 56cd94/56cd90/56cda4/56d7c0. HUD draw() places
        // score/life/bomb/power/resource rows at y=42..211 in x=416..640.
        // The remaining right-hand panel contains the equipped stone display.
        // Follow TH06/TH07/TH08's two-column header, retaining TH20's entire
        // y=0..480 sidebar so its difficulty and bottom FPS row are not lost.
        const double header = std::clamp(std::round(height * 0.22), 1.0, height - 1);
        safe_top = std::clamp(safe_top, 0.0, std::max(0.0, header - 1));
        const double left = width / 2;
        const double scale = source_width / 640;
        layout.regions[0] = {{416 * scale, 0, 224 * scale, 240 * scale}, {0, safe_top, left, header - safe_top}};
        layout.regions[1] = {{416 * scale, 240 * scale, 224 * scale, 240 * scale}, {left, safe_top, width - left, header - safe_top}};
        layout.regions[2] = {{32 * scale, 16 * scale, 384 * scale, 448 * scale}, {0, header, width, height - header}};
        layout.count = 3; layout.portrait_battle = true;
    } else {
        const double scale = std::min(width / source_width, height / source_height);
        const double target_width = source_width * scale, target_height = source_height * scale;
        layout.regions[0] = {{0, 0, source_width, source_height},
                            {(width - target_width) / 2, (height - target_height) / 2, target_width, target_height}};
    }
    return layout;
}
}
