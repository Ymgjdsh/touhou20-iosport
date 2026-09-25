#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// All callbacks run on UIKit's main thread, with the GLES context current.
// Logical coordinates use a top-left origin, independent of Retina scale.
typedef enum TH20IOSInputMode {
    TH20_IOS_INPUT_LOADING = 0,
    TH20_IOS_INPUT_MENU = 1,
    TH20_IOS_INPUT_GAMEPLAY = 2,
    TH20_IOS_INPUT_DIALOGUE = 3
} TH20IOSInputMode;

typedef enum TH20IOSTouchPhase {
    TH20_IOS_TOUCH_BEGIN = 0,
    TH20_IOS_TOUCH_MOVE = 1,
    TH20_IOS_TOUCH_END = 2,
    TH20_IOS_TOUCH_CANCEL = 3,
    TH20_IOS_TOUCH_MENU_TAP = 4,
    TH20_IOS_TOUCH_MENU_SWIPE = 5
} TH20IOSTouchPhase;

typedef struct TH20IOSCallbacks {
    size_t struct_size;
    void *userdata;
    // Return false on failure and call th20_ios_set_error with the cause.
    // Resources is the app bundle's resource directory; saves is Documents.
    bool (*initialize)(void *userdata, const char *resources, const char *saves);
    // Called at a fixed 60 Hz. Never receives a variable simulation timestep.
    void (*update)(void *userdata, double seconds);
    // Optional when update itself renders and calls graphics_present.
    void (*render)(void *userdata);
    // Windows virtual-key values: Z=0x5a, Shift=0x10, X=0x58, Escape=0x1b.
    void (*key)(void *userdata, int virtual_key, bool down);
    // Only one finger owns movement. dx/dy are relative logical pixels with
    // user sensitivity already applied. Menu taps are sent only on touch-up.
    void (*touch)(void *userdata, TH20IOSTouchPhase phase, uint64_t identifier,
                  float x, float y, float dx, float dy);
    void (*pause)(void *userdata, bool paused);
    void (*shutdown)(void *userdata);
    // Optional; return false if a requested internal render scale is unsupported.
    bool (*set_render_scale)(void *userdata, float scale);
    // Clear engine-side edge latches on scene and lifecycle transitions.
    void (*clear_input)(void *userdata);
    // Settings-only local unlock code. 1 = saved, 0 = invalid code,
    // -1 = game unavailable, -2 = failed (details in the diagnostic log).
    int (*cheat_code)(void *userdata, const char *code);
} TH20IOSCallbacks;

// Call from the real engine's main(). There is deliberately no placeholder main.
int th20_ios_run_app(int argc, char **argv, const TH20IOSCallbacks *callbacks);
bool th20_ios_graphics_make_current(void);
bool th20_ios_graphics_present(unsigned source_fbo, int width, int height);
void th20_ios_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
const char *th20_ios_log_path(void);
void th20_ios_flush_log(void);
// These status functions may be called from a worker; UI work is marshalled.
void th20_ios_set_stage(const char *stage);
void th20_ios_set_error(const char *message);
void th20_ios_set_ready(bool ready);
// Set mode on actual scene transitions so held/toggled inputs cannot leak.
void th20_ios_set_input_mode(TH20IOSInputMode mode);
// Keep portrait battlefield composition through dialogue and battle pause.
void th20_ios_set_combat_scene(bool active);
void th20_ios_set_logical_size(int width, int height);
void th20_ios_clear_input(void);
void th20_ios_open_settings(void);
// Recovered input backend exports. Bind the host key callback to key_event;
// invoke clear_keys on scene/lifecycle resets to drop pending sampled edges.
void th20_ios_key_event(unsigned virtual_key, int down);
void th20_ios_clear_keys(void);

#ifdef __cplusplus
}
#endif
