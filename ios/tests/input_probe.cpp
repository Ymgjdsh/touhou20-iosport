// Exercises the real recovered keyboard polling/mapping and iOS event queue.
#include "input/input.hpp"
#include "ios_host.h"
#include <cstring>

extern "C" int th20_ios_input_probe() {
    using namespace th20::source;
    int passed = 0, total = 0;
    const auto check = [&](const char *name, bool okay) {
        ++total; if (okay) ++passed;
        th20_ios_log("input-probe %s %s", okay ? "PASS" : "FAIL", name);
    };
    auto &host = input::win32_host();
    std::uint8_t keys[256]{};
    th20_ios_clear_keys();
    check("keyboard endpoint and empty initial state", host.keyboard(keys) && keys[0x5a] == 0);
    th20_ios_key_event(0x5a, 1); th20_ios_key_event(0x5a, 0);
    host.keyboard(keys); check("short Z tap retains a sample", keys[0x5a] == 0x80);
    host.keyboard(keys); check("short Z tap clears after observation", keys[0x5a] == 0);
    th20_ios_key_event(0x10, 1); host.keyboard(keys); host.keyboard(keys);
    check("held Shift survives repeated samples", keys[0x10] == 0x80);
    th20_ios_key_event(0x10, 0); host.keyboard(keys); check("Shift release", keys[0x10] == 0);
    th20_ios_key_event(0x58, 1); th20_ios_clear_keys(); host.keyboard(keys);
    check("scene clear removes pending X edge", keys[0x58] == 0);
    th20_ios_key_event(256, 1); th20_ios_key_event(0xffffffffU, 1); host.keyboard(keys);
    bool allZero = true; for (auto value : keys) allZero &= value == 0;
    check("out of range key ignored", allZero);
    keys[0x5a] = 0x80; host.set_keyboard(keys); std::memset(keys, 0, sizeof(keys)); host.keyboard(keys);
    check("set keyboard preserves native mapping", keys[0x5a] == 0x80);
    input::clear_keyboard_high_bits(host); host.keyboard(keys); check("recovered clear helper", keys[0x5a] == 0);
    check("null input reports failure", !host.keyboard(nullptr) && !host.set_keyboard(nullptr));
    XINPUT_STATE xinput{}; check("disconnected pad reports device not connected", host.xinput(0, &xinput) == 1167);
    input::Device device{}; input::initialize(device); input::initialize_keyboard(device, 0);
    platform::KeyBindings mappings[2]{};
    for (auto &mapping : mappings) platform::initialize_bindings(mapping);
    std::int32_t kind = 0;
    input::PollContext context{host, true, 600, 600, mappings, kind};
    th20_ios_key_event(0x5a, 1); th20_ios_key_event(0x10, 1); th20_ios_key_event(0x25, 1);
    input::poll_device(device, context);
    check("real polling maps Z Shift Left to recovered bits", device.buttons.current == (1 | 8 | 0x40));
    check("first sample produces pressed flags", device.buttons.pressed == (1 | 8 | 0x40));
    input::poll_device(device, context); check("held sample has no duplicate pressed edge", device.buttons.pressed == 0);
    th20_ios_clear_keys(); input::poll_device(device, context);
    check("clear produces released flags", device.buttons.current == 0 && device.buttons.released == (1 | 8 | 0x40));
    th20_ios_key_event(0x58, 1); context.active = false; input::poll_device(device, context);
    check("inactive game ignores held keys", device.buttons.current == 0);
    th20_ios_clear_keys();
    th20_ios_log("input-probe RESULT %d/%d %s", passed, total, passed == total ? "PASS" : "FAIL");
    return passed == total ? 0 : 1;
}
