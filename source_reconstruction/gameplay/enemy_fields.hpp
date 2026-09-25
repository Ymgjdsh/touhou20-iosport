#pragma once
#include "enemy_state.hpp"
#include <cstring>
#include <stdexcept>

namespace th20::source::gameplay {
// ECL recovery names scalar operands by their original offsets. Resolve each
// operand inside its actual member, never inside the native object as a whole.
// Pointer/container fields must be accessed by name and cannot enter this view.
inline std::uint8_t* enemy_scalar_address(EnemyState& state, unsigned offset, std::size_t size) {
#define TH20_ENEMY_SCALAR(original, member) \
    if (offset >= original && offset - original <= sizeof(state.member) && \
        size <= sizeof(state.member) - (offset - original)) \
        return reinterpret_cast<std::uint8_t*>(&state.member) + (offset - original)
    TH20_ENEMY_SCALAR(0x00, identifier);
    TH20_ENEMY_SCALAR(0x04, field_04);
    TH20_ENEMY_SCALAR(0x1c, fields_1c);
    TH20_ENEMY_SCALAR(0x5c, bounds_5c);
    TH20_ENEMY_SCALAR(0x64, bounds_64);
    TH20_ENEMY_SCALAR(0x6c, vector_6c);
    TH20_ENEMY_SCALAR(0x78, fields_78);
    TH20_ENEMY_SCALAR(0xa8, timer_a8);
    TH20_ENEMY_SCALAR(0xb8, timer_b8);
    TH20_ENEMY_SCALAR(0xc8, motion_c8);
    TH20_ENEMY_SCALAR(0x110, motion_110);
    TH20_ENEMY_SCALAR(0x170, vector_170);
    TH20_ENEMY_SCALAR(0x178, fields_178);
    TH20_ENEMY_SCALAR(0x188, field_188);
    TH20_ENEMY_SCALAR(0x18c, auxiliary_18c);
    TH20_ENEMY_SCALAR(0x1a8, pattern_1a8);
    TH20_ENEMY_SCALAR(0x250, fields_250);
    TH20_ENEMY_SCALAR(0x288, timer_288);
    TH20_ENEMY_SCALAR(0x298, timer_298);
    TH20_ENEMY_SCALAR(0x2a8, timer_2a8);
    TH20_ENEMY_SCALAR(0x2c8, fields_2c8);
    TH20_ENEMY_SCALAR(0x2d8, callback_mode);
    TH20_ENEMY_SCALAR(0x2e8, view_index);
#undef TH20_ENEMY_SCALAR
    throw std::out_of_range("Enemy scalar operand crosses a member or addresses a runtime pointer/container");
}
template<class T> T enemy_scalar(const EnemyState& state, unsigned offset) {
    T value;
    std::memcpy(&value, enemy_scalar_address(const_cast<EnemyState&>(state), offset, sizeof(T)), sizeof(T));
    return value;
}
template<class T> void set_enemy_scalar(EnemyState& state, unsigned offset, T value) {
    std::memcpy(enemy_scalar_address(state, offset, sizeof(T)), &value, sizeof(T));
}
}
