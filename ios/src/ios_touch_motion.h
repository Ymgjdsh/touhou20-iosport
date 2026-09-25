#pragma once
#include <algorithm>
#include <cmath>

namespace th20::ios::input {
// UIKit may deliver several coalesced moves between two fixed game updates.
// Keep the displacement until the ordinary player integrator consumes it;
// smoothing also keeps the lean animation active between sparse touch events.
class DragMotion {
    double pending_x_=0, pending_y_=0;
public:
    struct Delta { double x=0,y=0; };
    void clear() noexcept { pending_x_=pending_y_=0; }
    void add(double x,double y) noexcept {
        if(!std::isfinite(x)||!std::isfinite(y))return;
        pending_x_+=x;pending_y_+=y;
    }
    Delta step() noexcept {
        const double distance=std::hypot(pending_x_,pending_y_);
        if(distance==0)return {};
        // 1/2 smoothing settles normal movement within a few frames. A long
        // event-delivery stall must never become a one-frame teleport.
        const double amount=distance<=1.0/128.0?distance:std::min(distance*.5,24.0);
        const double fraction=amount/distance;
        Delta result{pending_x_*fraction,pending_y_*fraction};
        pending_x_-=result.x;pending_y_-=result.y;
        return result;
    }
};
}
