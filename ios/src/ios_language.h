#pragma once
// Keep Apple TRUE/FALSE macros out of the recovered Windows-compatible code.
typedef const struct __CFString* CFStringRef;
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace th20::ios::language {
// 0 follows the first preferred system language, 1 Japanese, 2 Simplified Chinese.
int resolve(int preference, std::string_view preferred_language);
void initialize(const char* resources);
bool available();
int preference();
int effective();
void select(int preference);
std::optional<std::vector<std::uint8_t>> resource(std::string_view name);
// Returns an owned CFString, or null when the original CP932 should be used.
CFStringRef copy_text(const char* cp932);
std::string spell(int id,const char* original);
}
