#include "../src/ios_presentation_layout.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>

namespace p = th20::ios::presentation;
unsigned checks = 0;
void check(bool value, const char* description) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}
bool near(double a, double b) { return std::abs(a - b) < 1e-8; }
void portrait(double width, double height, double top, double source_scale = 1) {
    const auto layout = p::make_layout(width, height, 640 * source_scale, 480 * source_scale, true, top);
    check(layout.count == 3 && layout.portrait_battle, "portrait composition enabled");
    const auto& play = layout.regions[2];
    check(near(play.destination.width, width) && near(play.destination.y + play.destination.height, height), "playfield fills lower screen");
    check(near(layout.regions[0].destination.width + layout.regions[1].destination.width, width), "HUD halves fill screen width");
    check(near(layout.regions[0].destination.y, top), "HUD clears top safe area");
    const auto& upper = layout.regions[0].source;
    const auto& lower = layout.regions[1].source;
    check(near(upper.y, 0) && near(upper.y + upper.height, lower.y) &&
          near(lower.y + lower.height, 480 * source_scale), "entire sidebar retained without missing rows");
    check(near(upper.x, lower.x) && near(upper.x + upper.width, 640 * source_scale) &&
          near(lower.x + lower.width, 640 * source_scale), "sidebar reaches original right edge");
    for (const auto& region : layout.regions) {
        for (const p::Point fraction : {p::Point{0, 0}, {0.1, 0.8}, {0.5, 0.5}, {0.999, 0.999}}) {
            p::Point screen{region.destination.x + region.destination.width * fraction.x,
                            region.destination.y + region.destination.height * fraction.y};
            const auto actual = layout.logical_point(screen);
            check(near(actual.x, region.source.x + region.source.width * fraction.x) &&
                  near(actual.y, region.source.y + region.source.height * fraction.y), "visible pixel maps to corresponding original coordinate");
        }
    }
    const auto delta = layout.logical_delta({play.destination.width / 4, play.destination.height / 4});
    check(near(delta.x, 96 * source_scale) && near(delta.y, 112 * source_scale), "drag scales by gameplay area, not full 640x480 frame");
    const auto center = layout.logical_point({width / 2, play.destination.y + play.destination.height / 2});
    check(near(center.x, 224 * source_scale) && near(center.y, 240 * source_scale), "battle center stays at original screen coordinate");
    const auto boundary = layout.logical_point({width / 2, play.destination.y});
    check(near(boundary.x, 224 * source_scale) && near(boundary.y, 16 * source_scale), "header boundary belongs to gameplay");
}
int main(int argc, char** argv) {
    for (const double source_scale : {0.5, 0.75, 1.0, 1.5, 2.0}) {
        portrait(768, 1024, 0, source_scale); portrait(390, 844, 47, source_scale);
        portrait(375, 667, 0, source_scale); portrait(834, 1194, 24, source_scale);
    }
    for (const bool battle : {false, true}) {
        const auto landscape = p::make_layout(1024, 768, 640, 480, battle);
        check(landscape.count == 1 && !landscape.portrait_battle, "landscape keeps complete original frame");
        check(near(landscape.logical_point({512, 384}).x, 320), "landscape coordinate unchanged");
    }
    const auto menu = p::make_layout(768, 1024, 640, 480, false);
    check(menu.count == 1 && near(menu.regions[0].destination.y, 224), "portrait title menu fits complete 4:3 frame");
    check(near(menu.logical_point({768, 800}).x, 640) && near(menu.logical_point({768, 800}).y, 480), "menu lower corner mapping");
    const auto alternate = p::make_layout(768, 1024, 800, 500, true);
    check(alternate.count == 1, "different source aspect never uses TH20-specific crop");
    const auto tiny = p::make_layout(1, 1, 640, 480, true, 100);
    check(tiny.regions[0].destination.height > 0 && tiny.regions[2].destination.height > 0,
          "transient tiny drawable never produces a zero or negative input denominator");
    if (argc > 1) {
        std::ofstream output(argv[1]); output << std::setprecision(15) << "{\"width\":768,\"height\":1024,\"regions\":[";
        const auto layout = p::make_layout(768, 1024, 640, 480, true);
        for (unsigned index = 0; index != layout.count; ++index) {
            if (index) output << ',';
            const auto& region = layout.regions[index];
            output << "{\"source\":[" << region.source.x << ',' << region.source.y << ',' << region.source.width << ',' << region.source.height
                   << "],\"destination\":[" << region.destination.x << ',' << region.destination.y << ',' << region.destination.width << ',' << region.destination.height << "]}";
        }
        output << "]}\n";
        check(bool(output), "write layout composition evidence");
    }
    std::printf("PASS: %u presentation and touch mapping checks\n", checks);
}
