#include "../src/ios_touch_motion.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using th20::ios::input::DragMotion;
int main(){
    // A 30 Hz touch source feeding a 60 Hz simulation must move on BOTH
    // updates, preserve finger distance, and stop after the release tail.
    DragMotion drag;double x=0,y=0;int moving=0;
    for(int frame=0;frame<120;++frame){
        if(frame%2==0)drag.add(4,-2);
        const auto d=drag.step();x+=d.x;y+=d.y;
        assert(d.x>0&&d.y<0);++moving;
    }
    for(int frame=0;frame<24;++frame){const auto d=drag.step();x+=d.x;y+=d.y;}
    assert(moving==120&&std::fabs(x-240)<1e-9&&std::fabs(y+120)<1e-9);
    assert(drag.step().x==0&&drag.step().y==0);

    // A delayed diagonal event is bounded as a vector, rather than allowing
    // the two axes together to exceed the per-update travel limit.
    drag.add(300,400);x=y=0;
    for(int i=0;i<64;++i){const auto d=drag.step();assert(std::hypot(d.x,d.y)<=24.000001);x+=d.x;y+=d.y;}
    assert(std::fabs(x-300)<1e-9&&std::fabs(y-400)<1e-9);

    // Cancellation/lifecycle changes discard old displacement, while invalid
    // UIKit samples cannot poison subsequent valid movement.
    drag.add(40,20);drag.clear();assert(drag.step().x==0);
    drag.add(std::numeric_limits<double>::quiet_NaN(),1);
    drag.add(1,std::numeric_limits<double>::infinity());
    assert(drag.step().x==0&&drag.step().y==0);
    drag.add(2,0);assert(drag.step().x==1);
    drag.add(-3,0);assert(drag.step().x<0);
    std::puts("PASS touch motion: sparse events, displacement conservation, bounded diagonal bursts, release, cancellation, invalid samples, reversal");
}
