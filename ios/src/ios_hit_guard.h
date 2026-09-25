#pragma once
namespace th20::ios {
// Installed by the game entry only. Platform probes and unmodified desktop
// builds keep the recovered hit graph. Called before any hit sound/effect.
inline bool (*prevent_player_hit)(void*) = nullptr;
}
