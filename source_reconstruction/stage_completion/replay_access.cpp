#include "replay_access.hpp"
#include "../startup_scene/startup.hpp"
#include "../replay_system/replay.hpp"
namespace th20::source::replay {
namespace {const ReplayInf* owner() noexcept{return static_cast<const ReplayInf*>(startup::unrecovered::owner_005c60fc);}}
bool is_playback() noexcept{return owner()->mode==1;}
void* recording_stage(int index) noexcept{return owner()->stages[index];}
const void* recorded_stage(int index) noexcept{return owner()->playback[index].stage;}
std::uint8_t inherited_stone(int index) noexcept{return owner()->user->inherited[index];}
}
namespace th20::source::stage_completion::unrecovered {bool replay_has_stage(int index){return replay::recorded_stage(index)!=nullptr;}}
namespace th20::source::overlay::unrecovered {std::uint8_t replay_inherited_stone(int index){return replay::inherited_stone(index);}}
