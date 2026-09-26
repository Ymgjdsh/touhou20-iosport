#include "../src/ios_battle_camera.h"
#include "../src/ios_battle_world.h"
#include <cstdio>
#include <cstdlib>

namespace c = th20::ios::camera;
unsigned checks{};
void check(bool result, const char* name) {
    ++checks;
    if (!result) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
bool near(float a, float b) { return std::abs(a-b) < .003f; }
int main() {
    for (float zoom : {.1f, .5f, 1.f, 2.f, 3.f})
    for (float anchor : {-1.f, 0.f, .6f, 1.f})
    for (float size : {128.f, 416.f, 832.f}) {
        const auto camera = c::make(zoom, anchor, -anchor);
        float original[4]{32, 16, 32+size, 16+size}, source[4];
        camera.source_bounds(original, 32, 16, size, size, source);
        for (unsigned corner=0; corner<4; ++corner) {
            float x=(source[(corner&1)?2:0]-32)/size*2-1;
            float y=1-(source[(corner&2)?3:1]-16)/size*2;
            camera.apply(x,y,1);
            check(near(x,(corner&1)?1.f:-1.f) && near(y,(corner&2)?-1.f:1.f),
                  "CPU visibility boundary maps to GPU viewport boundary");
        }
        // A CPU-projected billboard and a world vertex must receive the same
        // affine camera once, independent of perspective W.
        float matrix[4][4]{{1,0,0,0},{0,1,0,0},{0,0,1,1},{0,0,0,0}};
        camera.project(matrix);
        float x=.25f,y=-.4f,w=2.f;camera.apply(x,y,w);
        check(near(x,.25f*matrix[0][0]+2*matrix[2][0]) &&
              near(y,-.4f*matrix[1][1]+2*matrix[2][1]), "projected and screen-space paths agree");
    }
    const auto full=c::make(2,.5f,.5f),padded=c::make(2,.5f,.5f,384.f/416,448.f/480);
    check(near(full.offset_x*384,padded.offset_x*416) &&
          near(full.offset_y*448,padded.offset_y*480), "padded intermediate targets track the same field anchor");
    const auto zoomed_out=c::make(.1f,0,0);
    float dest[4]{0,0,128,128},bounds[4];zoomed_out.source_bounds(dest,0,0,128,128,bounds);
    check(bounds[0]<-500 && bounds[2]>700, "zoom-out preserves objects beyond the old screen");
    check(c::make(1,.9f,-.9f).identity(), "1x is unchanged at every player position");
    check(c::make(NAN,0,0).identity(), "non-finite zoom cannot poison rendering");
    th20::ios::world::Arena arena;
    arena.update(1,true,.5f);
    auto b=arena.bounds();
    check(b.left==-384 && b.right==384 && b.top==-224 && b.bottom==672,
          "half zoom opens all four original movement boundaries");
    arena.update(1,true,3);
    check(arena.extent==2, "zooming in cannot shrink an occupied arena");
    for(float zoom : {.5f,1.f,3.f}) {
        // Player is outside the original field, near the enlarged right edge.
        auto camera=c::make(zoom,376.f/192,0,1,1,arena.extent);
        float x=376.f/192,y=0;camera.apply(x,y,1);
        check(x>=-1 && x<=1,"camera keeps an expanded-field player visible");
        float destination[4]{0,0,384,448},source[4];
        camera.source_bounds(destination,0,0,384,448,source);
        check(source[0]>=-192-.003f && source[2]<=576+.003f,
              "camera viewport remains inside the stable expanded arena");
    }
    arena.update(1,true,.1f);arena.update(1,true,2);
    check(arena.extent==10,"minimum zoom opens a tenfold field and stays open");
    arena.update(2,true,2);
    check(arena.extent==1,"new stage resets expansion independently of camera zoom");
    arena.update(2,true,.1f);arena.update(2,false,.1f);
    check(arena.extent==1 && arena.stage==-1,"disabling expansion restores ordinary bounds");
    arena.update(1,true,NAN);
    check(arena.extent==1,"invalid zoom cannot create invalid simulation bounds");
    std::printf("PASS: %u battle camera checks\n",checks);
}
