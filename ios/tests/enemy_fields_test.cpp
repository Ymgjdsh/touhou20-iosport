#include "gameplay/enemy_fields.hpp"
#include <cstdio>
#include <limits>

int main() {
    using namespace th20::source::gameplay;
    EnemyState state;
    state.initialize();
    int checks=0;
    const auto check=[&](bool okay,const char* name){++checks;if(!okay){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}};
    // High pointer bits deliberately differ from adjacent gameplay values.
    state.context_address=std::uintptr_t{0x123456789abcdef0ULL};
    state.update_callback=std::uintptr_t{0x223456789abcdef0ULL};
    state.mesh_owner_address=std::uintptr_t{0x323456789abcdef0ULL};
    set_enemy_scalar(state,0x3c,.25f);
    set_enemy_scalar(state,0x50,std::uint32_t{3});
    set_enemy_scalar(state,0x110,th20::source::sprite::Vec3{16.f,32.f,-4.f});
    set_enemy_scalar(state,0x178,12.f);
    set_enemy_scalar(state,0x270,std::uint32_t{2});
    set_enemy_scalar(state,0x2cc,std::uint32_t{0x400});
    check(enemy_scalar<float>(state,0x3c)==.25f,"slowdown scalar");
    check(state.fields_1c[13]==3,"rank scalar");
    th20::source::sprite::Vec3 position;
    std::memcpy(&position,&state.motion_110,sizeof(position));
    check(position.x==16.f&&position.y==32.f&&position.z==-4.f,"motion targets native member");
    check(enemy_scalar<float>(state,0x178)==12.f,"bounding region");
    check(state.fields_250[8]==2&&state.fields_2c8[1]==0x400,"boss index and flags");
    check(state.context_address==0x123456789abcdef0ULL&&state.update_callback==0x223456789abcdef0ULL&&state.mesh_owner_address==0x323456789abcdef0ULL,"pointer fields not overwritten");
    check(state.movements.size()==1&&state.animations.size()==1,"containers not overwritten");
    for(unsigned offset:{0x08u,0x0cu,0x158u,0x168u,0x2b8u,0x2d4u,0x2dcu,0x2ecu}) {
        bool rejected=false;try{set_enemy_scalar(state,offset,std::uint32_t{0});}catch(const std::out_of_range&){rejected=true;}
        check(rejected,"raw pointer/container operand rejected");
    }
    bool rejected=false;try{set_enemy_scalar(state,0x2d0,std::uint64_t{0});}catch(const std::out_of_range&){rejected=true;}
    check(rejected,"cross-member access rejected");
    std::printf("PASS %d native enemy scalar and pointer-boundary checks\n",checks);
}
