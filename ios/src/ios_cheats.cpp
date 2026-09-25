#include "ios_cheats.h"
#include "ios_hit_guard.h"
#include "ios_host.h"
#include "program_entry/program_entry.hpp"
#include "player_entity/owner.hpp"
#include "player_entity/power.hpp"
#include "bomb_system/bomb.hpp"
#include "bullet_system/player_cancellation.hpp"
#include "laser_system/laser.hpp"
#include "gameplay/player_state.hpp"
#include "item_system/rewards.hpp"
#include "hud_system/hud.hpp"
#include "damage_regions/damage.hpp"
#include "replay_system/replay.hpp"
#include <algorithm>
#include <exception>

namespace th20::ios::cheats {
namespace {
using namespace source;
namespace ps=gameplay::player_state;
bool developer_enabled=false, auto_bomb=false, invulnerable=false;
player_entity::Player* player() {
    if(program_entry::graphics_state.field_0b08!=7) return nullptr;
    if(auto* replay=replay::controller();replay&&replay->mode==1) return nullptr;
    return static_cast<player_entity::Player*>(game_session::context(0).objects_04[0]);
}
bool protect(void* value) {
    if(!auto_bomb&&!invulnerable) return false;
    auto* p=player();
    if(!p||value!=p||p->state!=1||!p->context||!p->context->current_player) return false;
    if(developer_enabled&&invulnerable) return true;
    if(!auto_bomb) return false;
    auto* controller=static_cast<bomb::Controller*>(p->context->objects_04[5]);
    if(!controller||!controller->can_trigger()) return false;
    // Use the real character bomb: spend stock, cancel spell bonus and start
    // its effects. Reimu normally sets invulnerability in the next update;
    // bridge that interval so another collision in this frame cannot kill us.
    controller->trigger();
    recovered::timer_set(p->timers_2050[0],std::max(40,p->timers_2050[0].current));
    th20_ios_log("assist autobomb: collision intercepted before hit sound/death; remaining=%d",bomb::bomb_count(*p->context->current_player));
    return true;
}
void score(game_session::Player& p) {
    damage::add_score(p,9999999990ULL);
    if(hud::controller) hud::controller->score=ps::score(p);
}
void items(game_session::Player& p) {
    // TH20 point/special item totals, point-item value, stone gauge and
    // four special counters, bounded by the game's recovered setters.
    for(const auto address:{0x4bdf80u,0x4bea50u,0x4be050u}) ps::set(p,address,1000000);
    ps::set(p,0x4be940,ps::read<int>(p,0x50));
    ps::set(p,0x4be620,10000);
    for(unsigned i=0;i<4;++i) item::add_special_counter(p,i,1000);
}
void power(player_entity::Player& p) {
    auto& record=*p.context->current_player;
    ps::set(record,0x4be0a0,400);
    player_entity::refresh_power(p,-1);
}
void stock(game_session::Player& p) {
    ps::set(p,0x4bdee0,7);ps::set(p,0x4bda30,7);
    item::add_lives(p,7,item::reward_environment());
    item::add_bombs(p,7,item::reward_environment());
}
void clear(player_entity::Player& p) {
    auto& context=*p.context;
    if(auto* bullets=static_cast<bullet::Controller*>(context.primary_owner))
        bullet::cancel_near_circle(*bullets,{0,224,0},4096,0);
    if(auto* lasers=static_cast<laser::Controller*>(context.objects_04[4])) lasers->erase_all(1,0);
}
}
void install(){prevent_player_hit=&protect;}
void configure(bool developer,bool autobomb){
    developer_enabled=developer;auto_bomb=autobomb;
    if(!developer)invulnerable=false;
}
bool invincible_enabled(){return developer_enabled&&invulnerable;}
int perform(int action){
    auto* p=player();
    if(!developer_enabled||!p||!p->context||!p->context->current_player) return 0;
    auto& record=*p->context->current_player;
    try {
        switch(action){
        case invincible: invulnerable=!invulnerable;return invulnerable?2:1;
        case max_score: score(record);break;
        case max_items: items(record);break;
        case max_power: power(*p);break;
        case full_stock: stock(record);break;
        case max_all: score(record);items(record);power(*p);stock(record);break;
        case clear_bullets: clear(*p);break;
        default:return 0;
        }
        th20_ios_log("dev action=%d applied",action);return 1;
    }catch(const std::exception& e){th20_ios_log("dev action=%d failed: %s",action,e.what());return -1;}
}
}
