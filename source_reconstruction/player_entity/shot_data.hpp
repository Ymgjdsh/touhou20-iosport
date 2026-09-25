#pragma once
#include <cstdint>
#include <span>
namespace th20::source::player_entity {
struct ShotRecord;
//4f9b80: signed nonnegative entries in the count16 table become actual x86
//pointers. Negative sentinels remain unchanged. Malformed files are rejected
//before mutation; the original valid-resource behavior is the equivalence domain.
void relocate_shot_data(std::span<std::uint8_t>);
void* load_shot_data(const char*);                           //4f9b80 +410aa0
// Native ARM64 retains the on-disk relative-offset table. Never store native
// pointers into its four-byte entries. Windows/Web use the original relocation.
const ShotRecord* shot_pattern(const void* bytes,std::int32_t pattern) noexcept;
}
