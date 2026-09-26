#pragma once
#include <algorithm>
#include <cmath>

namespace th20::ios::world {
struct Bounds {
    float left{-192}, top{0}, right{192}, bottom{448};
};
struct Arena {
    float extent{1};
    int stage{-1};
    void update(int next_stage, bool enabled, float zoom) {
        if (!enabled || next_stage < 0) { extent=1; stage=-1; return; }
        if (stage != next_stage) { extent=1; stage=next_stage; }
        if (std::isfinite(zoom)) extent=std::max(extent,1/std::clamp(zoom,.1f,3.f));
    }
    Bounds bounds() const { return {-192*extent,224-224*extent,192*extent,224+224*extent}; }
};
#if defined(TH20_IOS)
// Updated once before each engine frame. Bounds persist through pause/dialogue,
// and zooming back in never shrinks an occupied arena during the same stage.
inline Arena arena;
inline float extent() { return arena.extent; }
#else
inline constexpr float extent() { return 1; }
#endif
inline Bounds bounds() { const float e=extent(); return {-192*e,224-224*e,192*e,224+224*e}; }
inline float extra_x() { return 192*(extent()-1); }
inline float extra_y() { return 224*(extent()-1); }
inline float expand_width(float original) { return extent()==1 ? original : original+2*extra_x(); }
inline float expand_height(float original) { return extent()==1 ? original : original+2*extra_y(); }
}
