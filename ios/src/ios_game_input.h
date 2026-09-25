#pragma once
#include "ios_host.h"
namespace th20::source::player_entity { class Player; }
namespace th20::ios::input {
void clear() noexcept;
void key(int virtual_key, bool down) noexcept;
void touch(TH20IOSTouchPhase, uint64_t id, float x, float y, float dx, float dy);
void before_frame();
void after_frame();
// Called only from the player's ordinary movement branch, before its fixed
// position, animation direction, history and option followers are updated.
void apply_drag(source::player_entity::Player&, int& x, int& y, float clock_scale) noexcept;
}
