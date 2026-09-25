#include "../overlay_system/overlay.hpp"
#include "hit_callbacks.hpp"
#include <cstring>
namespace th20::source::damage {
namespace {
template<class T>T read(const void* object,std::size_t offset){T value;std::memcpy(&value,static_cast<const std::uint8_t*>(object)+offset,sizeof(value));return value;}
template<class T>void write(void* object,std::size_t offset,T value){std::memcpy(static_cast<std::uint8_t*>(object)+offset,&value,sizeof(value));}
}
void accumulate_damage_reward(void* overlay,const sprite::Vec3& position,int damage,int type){
    auto& counter=static_cast<th20::source::overlay::WeaponStoneInf*>(overlay)->counter;
    counter.current=recovered::signed_bits(static_cast<unsigned>(counter.current)+static_cast<unsigned>(damage));
    while(counter.threshold<counter.current){
        unrecovered::spawn_item_004c45b0(game_session::context(0).objects_04[2],position,1,type);
        counter.current=recovered::signed_bits(static_cast<unsigned>(counter.current)-static_cast<unsigned>(counter.threshold));
    }
}
}
