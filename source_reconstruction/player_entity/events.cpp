#include "events.hpp"
#include "owner.hpp"
#include "../overlay_system/overlay.hpp"
#include <cstring>
namespace th20::source::player_entity {
namespace {
template<class T>T get(const void* p,std::size_t offset){T value;std::memcpy(&value,static_cast<const std::uint8_t*>(p)+offset,sizeof(value));return value;}
template<class T>void put(void* p,std::size_t offset,T value){std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(value));}
recovered::Timer& timer(void* p,std::size_t offset){return *reinterpret_cast<recovered::Timer*>(static_cast<std::uint8_t*>(p)+offset);}
}
void add_graze_count(game_session::Player& player,int amount) noexcept{
    int count=recovered::signed_bits(get<std::uint32_t>(&player,0xe4)+static_cast<std::uint32_t>(amount));
    if(count<0)count=0;if(count>99999999)count=99999999;put(&player,0xe4,count);
}
void hit(void* player,EventServices& host){
    auto& entity=*static_cast<Player*>(player);auto& context=*entity.context;
    host.mark_enemies();host.notify_secondary(context.objects_04[3]);
    auto* overlay=static_cast<overlay::WeaponStoneInf*>(host.session().contexts[0].overlay_owner);
    if(overlay->phase==1){
        overlay->phase=2;host.sound(0x2d);recovered::timer_set(entity.timers_2050[0],4);
    }else{
        if(!(entity.entity_flags&8u))host.sound(2);
        host.spawn_hit_effect(context,position(player));host.notify_secondary(context.objects_04[3]);
        recovered::timer_set(entity.timers_644[0],0);entity.state=4;recovered::timer_set(entity.timers_2050[0],6);
        host.reset_player_animation(player);entity.field_2204=8;
    }
}
void graze(void* player,const sprite::Vec3& position,std::uint32_t color,EventServices& host){
    auto& context=*static_cast<Player*>(player)->context;add_graze_count(*context.current_player,1);
    const auto delay=host.random_next()%4;
    host.enqueue_graze(context,position,(color&0x00ffffffu)|0xff000000u,static_cast<int>(delay));
    auto* overlay=host.session().contexts[0].overlay_owner;
    host.accumulate_reward(overlay,position,host.special_active()?1500:1000,0xd);
    const int gain=host.selected_enemy_present()?50:10;
    host.add_special_items(*host.session().contexts[0].current_player,gain);host.sound_at(0x2a,position.x);
}
}
