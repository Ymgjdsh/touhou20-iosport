#include "../player_entity/owner.hpp"
#include "character_environment.hpp"
#include "../sprite_renderer/pool.hpp"
#include "../sprite_renderer/loading_interrupt.hpp"
#include "../program_entry/program_entry.hpp"
#include "../ecl_vm/math.hpp"
#include <cstring>
namespace th20::source::bomb::character_environment {
namespace {
template<class T>T read(const void* p,std::size_t offset){T value;std::memcpy(&value,static_cast<const std::uint8_t*>(p)+offset,sizeof(value));return value;}
template<class T>void write(void* p,std::size_t offset,const T& value){std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(value));}
}
void* player_entity(){return game_session::context(0).objects_04[0];}
sprite::Vec3 player_position(){return static_cast<th20::source::player_entity::Player*>(player_entity())->position_614;}
float player_horizontal_motion(){return static_cast<th20::source::player_entity::Player*>(player_entity())->vector_20c4.x;}
void set_invulnerability(std::int32_t time){recovered::timer_set(static_cast<th20::source::player_entity::Player*>(player_entity())->timers_2050[0],time);}
void set_movement_scale(float value){auto& bits=static_cast<th20::source::player_entity::Player*>(player_entity())->fields_20e4[2];std::memcpy(&bits,&value,sizeof(value));}
void set_bomb_player_flag(bool value){auto& flags=static_cast<th20::source::player_entity::Player*>(player_entity())->entity_flags;flags=value?(flags|4u):(flags&~4u);}
sprite::AnimationFile& player_animation(){return *static_cast<th20::source::player_entity::Player*>(player_entity())->animation_file;}
sprite::Animation& animation_or_fallback(std::uint32_t& handle){auto& controller=*program_entry::sprite_controller;auto* animation=sprite::resolve_animation_handle(controller,handle);return animation?*animation:controller.animation_dc;}
void interrupt(std::uint32_t handle){sprite::interrupt_animation_children(*program_entry::sprite_controller,handle,1);}
sprite::Animation* animation_child(sprite::Animation& animation,std::int32_t script,std::int32_t ordinal){
    auto* link=&animation.links[3];
    while(link){
        auto* child=link->value;
        if(child&&child!=&animation){
            if(static_cast<std::int16_t>(child->base.field_440)==script||script==-1){if(ordinal==0)return child;ordinal=recovered::signed_bits(static_cast<std::uint32_t>(ordinal)-1u);}
            if(child->links[3].next)if(auto* found=animation_child(*child,script,ordinal))return found;
            if(static_cast<std::int16_t>(animation.base.field_440)==-2&&!link->next)return link->value;
        }
        link=link->next;
    }
    return nullptr;
}
sprite::Vec3 animation_world_position(sprite::Animation& animation){
    auto position=animation.vector_5bc;const auto& own=animation.base.vector_2c;const auto& extra=animation.base.vector_484;
    position={recovered::add32(recovered::add32(position.x,own.x),extra.x),recovered::add32(recovered::add32(position.y,own.y),extra.y),recovered::add32(recovered::add32(position.z,own.z),extra.z)};
    if(animation.root_parent&&!(animation.base.flags[1]&0x1000u)){
        auto& parent=*reinterpret_cast<sprite::Animation*>(animation.root_parent);
        if(animation.base.flags[1]&32u)ecl::math::rotate(position.x,position.y,parent.base.vector_38.z);
        const auto offset=animation_world_position(parent);position={recovered::add32(position.x,offset.x),recovered::add32(position.y,offset.y),recovered::add32(position.z,offset.z)};
    }
    return position;
}
}
