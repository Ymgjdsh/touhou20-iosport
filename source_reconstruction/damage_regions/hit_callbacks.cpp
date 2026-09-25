#include "../player_entity/owner.hpp"
#include "hit_callbacks.hpp"
#include "../sprite_renderer/pool.hpp"
#include "../sprite_renderer/loading_interrupt.hpp"
#include <cstring>
#include <emmintrin.h>
namespace th20::source::damage {
namespace {
template<class T>T read(const void* object,std::size_t offset){T value;std::memcpy(&value,static_cast<const std::uint8_t*>(object)+offset,sizeof(value));return value;}
template<class T>void write(void* object,std::size_t offset,T value){std::memcpy(static_cast<std::uint8_t*>(object)+offset,&value,sizeof(value));}
}
void* find_player_shot(Region& region){
    if(!region.field_90)return nullptr;
    auto& list=static_cast<player_entity::Player*>(region.context->objects_04[0])->shots.active;
    for(scheduler::Iterator iterator(list.sentinel.next);iterator.current;iterator.advance())if(read<int>(iterator.current->value,offsetof(player_entity::Shot,flags))==region.field_90)return iterator.current->value;
    return nullptr;
}
int default_shot_hit(void* shot,sprite::Controller& sprites){
    if(!read<std::uint8_t>(shot,offsetof(player_entity::Shot,byte_114))){
        auto handle=read<unsigned>(shot,offsetof(player_entity::Shot,handle_18));auto* animation=sprite::resolve_animation_handle(sprites,handle);write(shot,offsetof(player_entity::Shot,handle_18),handle);
        write(shot,offsetof(player_entity::Shot,motion)+offsetof(state::Motion,position)+8,.1f);sprite::set_animation_interrupt(*animation,1);write(shot,offsetof(player_entity::Shot,fields_98)+4,2);
        write(shot,offsetof(player_entity::Shot,motion)+0x18,_mm_cvtss_f32(_mm_div_ss(_mm_set_ss(read<float>(shot,offsetof(player_entity::Shot,motion)+0x18)),_mm_set_ss(8))));
        animation->vector_5bc=read<sprite::Vec3>(shot,offsetof(player_entity::Shot,motion));
        auto damage_handle=read<unsigned>(shot,offsetof(player_entity::Shot,fields_b8)+36);retire_handle(damage_handle);write(shot,offsetof(player_entity::Shot,fields_b8)+36,0u);
    }
    return read<int>(shot,offsetof(player_entity::Shot,fields_98)+20);
}
int shot_hit_callback(Region& region,const sprite::Vec3& position,const sprite::Vec2* size,float angle,float radius,sprite::Controller& sprites){
    auto* shot=find_player_shot(region);using Callback=int(__thiscall*)(void*,const sprite::Vec3*,const sprite::Vec2*,float,float);
    const auto callback=read<Callback>(shot,offsetof(player_entity::Shot,hit_callback));return callback?callback(shot,&position,size,angle,radius):default_shot_hit(shot,sprites);
}
int diminish_shot_hit(Region& region,const sprite::Vec3& position){
    auto* shot=find_player_shot(region);
    // Original computes and discards the midpoint before changing the damage.
    volatile float midpoint[3];for(unsigned i=0;i<3;++i)midpoint[i]=_mm_cvtss_f32(_mm_div_ss(_mm_add_ss(_mm_set_ss((&region.motion.position.x)[i]),_mm_set_ss((&position.x)[i])),_mm_set_ss(2.f)));midpoint[2]=0;
    const auto damage=read<int>(shot,offsetof(player_entity::Shot,fields_98)+20);write(shot,offsetof(player_entity::Shot,fields_98)+20,1);return damage;
}
}
