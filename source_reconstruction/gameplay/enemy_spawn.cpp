#include "enemy_spawn.hpp"
#include "enemy_entity.hpp"
#include "enemy_damage.hpp"
#include "player_state.hpp"
#include <cstring>
namespace th20::source::gameplay {
namespace {
template<class T>T read(const void* p,unsigned offset){T result;std::memcpy(&result,static_cast<const std::uint8_t*>(p)+offset,sizeof(T));return result;}
template<class T>void write(void* p,unsigned offset,T value){std::memcpy(static_cast<std::uint8_t*>(p)+offset,&value,sizeof(T));}
}
void construct_spawn_parameters(SpawnParameters& parameters) noexcept{std::memset(&parameters,0,sizeof(parameters));}
int apply_enemy_spawn(void* entity,const SpawnParameters& parameters,game_session::Session& session,EnemyFrameServices& host){
    //The input may alias the object's own saved parameters. memmove preserves
    //the identical source/destination case of the original21-word copy.
    auto& enemy=*static_cast<Enemy*>(entity);
    std::memmove(&enemy.spawn_parameters,&parameters,sizeof(parameters));
    auto& state=enemy.state;
    state.movements.resize(1);std::memcpy(state.movements[0].motion.words,&parameters.position,12);
    state.field_188=parameters.field_0c;state.auxiliary_18c.words[0]=parameters.health;
    state.auxiliary_18c.words[1]=parameters.health;state.auxiliary_18c.words[2]=parameters.health;
    state.pattern_1a8.fields_00[1]=parameters.field_10;
    auto& flags=state.fields_2c8[1];flags=(flags&~8u)|((parameters.flags_18&1u)<<3);
    const auto difficulty=player_state::difficulty(session.player_table);state.fields_1c[(0x50-0x1c)/4]=static_cast<std::uint32_t>(difficulty);
    const std::uint8_t rank=difficulty<4?static_cast<std::uint8_t>(1u<<(static_cast<std::uint32_t>(difficulty)&31u)):std::uint8_t{2};enemy.main.rank=rank;
    std::memcpy(state.fields_78,parameters.variables,48);recovered::timer_set(state.timer_288,2);
    flags=(flags&~0x400u)|((parameters.flags_1c&1u)<<10);state.fields_1c[(0x30-0x1c)/4]=0;state.field_04=parameters.field_50;
    if(parameters.health>999)flags|=0x4000u;
    state.fields_250[12]=enemy_phase_stage(session);
    (void)update_enemy_with_time_scale(entity,host);            //Original intentionally discards update's return.
    //The original bit7 branch copies pattern+4 to itself; no state changes.
    state.fields_250[0]=(state.identifier&1u)+3;
    if(!state.fields_250[1]){
        state.fields_250[1]=37;
        if(state.fields_1c[1]==2)switch(state.fields_1c[2]){
        case 0:case 20:case 59:case 62:case 104:state.fields_250[1]=37;break;
        case 5:case 25:case 53:case 94:state.fields_250[1]=33;break;
        case 10:case 56:case 99:state.fields_250[1]=41;break;
        case 15:case 109:state.fields_250[1]=45;break;
        case 30:state.fields_250[1]=51;break;
        case 35:state.fields_250[1]=50;break;
        case 40:state.fields_250[1]=49;break;
        }
        state.fields_250[2]=1;
    }
    return 0;
}
}
