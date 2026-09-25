#include "../card_system/card.hpp"
#include "../gameplay/enemy_entity.hpp"
#include "bomb.hpp"
#include "../gameplay/enemy.hpp"
#include "../gameplay/player_state.hpp"
#include <algorithm>
#include <cstring>
namespace th20::source::bomb {
namespace ps=gameplay::player_state;
void add_enemy_bomb_counter(gameplay::EnemyController& enemy,std::int32_t amount) noexcept {
    // EnemyData+3c == EnemyController+4c. ADD uses modulo32-bit arithmetic.
    enemy.data.fields_30[3]+=static_cast<std::uint32_t>(amount);
}
void set_enemy_bomb_flag(void* entity,std::uint32_t value) noexcept {
    auto& flags=static_cast<gameplay::Enemy*>(entity)->state.fields_2c8[2];
    flags=(flags&~32u)|((value&1u)<<5);
}
void mark_enemies_for_bomb(gameplay::EnemyController& enemy){
    scheduler::Iterator iterator(enemy.enemies.sentinel.next);
    while(iterator.current){set_enemy_bomb_flag(iterator.current->value,1);iterator.advance();}
}
void add_player_meter(game_session::Player& player,std::int32_t amount) noexcept {
    const auto bits=ps::read<std::uint32_t>(player,0x5c)+static_cast<std::uint32_t>(amount);
    ps::write(player,0x5c,std::clamp(recovered::signed_bits(bits),0,10000));
}
std::int32_t bomb_count(game_session::Player& player) noexcept {
    const auto count=std::clamp(ps::read<std::int32_t>(player,0xcc),0,10);ps::write(player,0xcc,count);return count;
}
void notify_bomb_start(void* secondary_owner){
    auto& card=*static_cast<card::CardInf*>(secondary_owner);
    if(!(card.flags&1u))return;
    if(card.age.current>=60){card.bonus=0;card.flags&=~34u;}
    else if(static_cast<Controller*>(card.context->objects_04[5])->active())card.flags|=32u;
}
}
