#pragma once
namespace th20::ios {
// Uses the recovered hidden-code unlock, plus music and stone availability.
// Saves through the game's score codec; never replaces scores or replays.
int apply_unlock_code(const char* code) noexcept;
}
