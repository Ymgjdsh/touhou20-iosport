#pragma once
namespace th20::ios::cheats {
enum Action { invincible=0, max_score, max_items, max_power, full_stock, max_all, clear_bullets };
void install();
void configure(bool developer, bool autobomb);
// Returns 1 on success, 0 when unavailable, -1 on failure; invincibility
// returns 2 when switched on and 1 when switched off.
int perform(int action);
bool invincible_enabled();
}
