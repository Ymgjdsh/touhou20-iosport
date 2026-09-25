#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <dsound.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory_resource>
#include <optional>
#include <vector>
#include "../runtime_core/runtime_core.hpp"
#include "../platform_services/configuration.hpp"

namespace th20::source::audio {
struct EffectDefinition {
    std::int32_t id, file_index;
    std::int16_t volume, cooldown;
    std::uint32_t play_flags, retained_10;
};
static_assert(sizeof(EffectDefinition)==20);
extern const EffectDefinition effect_definitions[90]; // original 0x5ae6e8, id order is NOT sorted
extern const char* const effect_filenames[72];         // original 0x5aedf0
struct EffectRequest {
    std::int32_t id, count;
    std::int32_t pans[128];
};
struct Command {
    std::int32_t type, argument, stage;
    char name[256];
};
struct EffectChannel {
    IDirectSoundBuffer* buffer;
    std::int32_t cooldown;
    const EffectDefinition* definition;
    std::int32_t id, pan, was_playing;
};
#pragma pack(push,1)
struct TrackFormat {                           // records in thbgm.fmt, 52 bytes
    char name[16];
    std::uint32_t file_offset, preload_bytes, loop_start, total_bytes;
    WAVEFORMATEX format;
    std::uint16_t padding;
};
#pragma pack(pop)
struct PreloadedTrack {
    TrackFormat* format;
    std::uint8_t* allocation;
    std::uint8_t* current;
    std::uint32_t size;
};
static_assert(sizeof(EffectRequest)==0x208 && sizeof(Command)==0x10c);
// TrackFormat is serialized in thbgm.fmt and must never acquire native pointers.
static_assert(sizeof(TrackFormat)==0x34 && offsetof(TrackFormat,format)==0x20);
#if defined(TH20_IOS)
static_assert(sizeof(EffectChannel)==0x28 && offsetof(EffectChannel,definition)==0x10);
static_assert(sizeof(PreloadedTrack)==0x20 && offsetof(PreloadedTrack,size)==0x18);
#else
static_assert(sizeof(EffectChannel)==0x18 && sizeof(PreloadedTrack)==16);
#endif
void initialize(EffectRequest&) noexcept;                // 0x425ce0
void initialize(Command&) noexcept;                      // 0x425fc0
void initialize(EffectChannel&) noexcept;                // 0x425d20
void bind_effect(EffectChannel&,std::int32_t);            // 0x426390
void release_effect(EffectChannel&);                     // 0x428380
void stop_effect(EffectChannel&);                        // 0x428810
void play_effect(EffectChannel&,std::int32_t pan,std::int32_t volume); // 0x426ef0
LONG effect_volume(std::int16_t attenuation,std::int32_t volume) noexcept;
LONG music_volume(std::int32_t attenuation,std::int32_t volume) noexcept;
std::int32_t pan_from_position(float) noexcept;          // 0x426eb0/0x429090 arithmetic

struct DeviceOwner {                                     // original 0x4259a0, four bytes
    IDirectSound8* device=nullptr;
    ~DeviceOwner();                                      // 0x4598a0
    HRESULT initialize(HWND,DWORD,WORD,DWORD,WORD);      // 0x45aaa0
    HRESULT set_primary_format(WORD,DWORD,WORD);         // 0x45b850
};
#if defined(TH20_IOS)
static_assert(sizeof(DeviceOwner)==8);
#else
static_assert(sizeof(DeviceOwner)==4);
#endif
class MusicStream;
using ResourceReader=std::function<std::optional<std::vector<std::uint8_t>>(const char*)>;
struct Context {
    platform::Configuration& configuration;
    runtime::Log& log;
    ResourceReader read_resource;
    std::function<double()> read_clock;
    HWND graphics_window;
};

// SoundInf is the real object formerly misidentified as ThreadRegistry.
// The x86 build retains the original offsets through +0x57e8. iOS uses
// naturally aligned, pointer-sized C++ fields; all production accesses below
// and in callers are named field accesses, not original absolute offsets.
struct SoundInf {
    IDirectSound8* direct_sound;                        // +0, borrowed from device_owner
    IDirectSoundBuffer* silent_buffer;                  // +4
    HWND window;                                       // +8
    DeviceOwner* device_owner;                         // +c
    DWORD notify_thread_id;                            // +10
    HANDLE notify_thread;                              // +14
    std::uint32_t retained_18;
    EffectRequest requests[12];                        // +1c
    std::pmr::vector<PreloadedTrack> preloaded;          // +187c
    std::int32_t preloaded_index;                       // +188c
    TrackFormat* track_formats;                        // +1890, owning allocation
    char current_track[256];                           // +1894
    EffectChannel effects[90];                         // +1994
    IDirectSoundBuffer* source_buffers[72];             // +2204
    std::uint32_t duplicate_counts[72];                 // +2324
    char queued_track[256];                            // +2444
    Command commands[32];                              // +2544, slot 31 is sentinel
    char track_names[16][256];                         // +46c4
    char music_file[256];                              // +56c4
    MusicStream* stream;                               // +57c4
    std::uint32_t retained_57c8;
    HANDLE notification;                               // +57cc
    std::uint32_t retained_57d0,retained_57d4,retained_57d8;
    std::int32_t music_level,effect_level;              // +57dc/+57e0 (global 5c000c/5c0010)
    std::uint32_t retained_57e4;
    Context* context;                                  // source-only +57e8

    SoundInf();                                        // 0x425d70, CRT0x4011c0
    ~SoundInf();
    void enqueue(std::int32_t,std::int32_t,const char*); // 0x428c90
    void request_effect(std::int32_t,std::int32_t);      // 0x426d70
    void request_effect_at(std::int32_t,float);          // 0x426eb0
    void set_effect_pan(std::int32_t,float);             // 0x429090
    void stop_effects(std::int32_t);                     // 0x428890, negative stops all immediately
    bool ready() const noexcept {return device_owner!=nullptr;} // 0x428fc0
    int poll();                                        // 0x4277f0, returns first pending command
    void initialize(HWND,Context&);                    // 0x4263e0
    int load_formats(const char*);                     // 0x426840
    int apply_configuration();                        // 0x428560
    int load_wave(unsigned,const char*);               // 0x427050
    int create_effect(EffectChannel&);                 // 0x426760
    void free_preload(unsigned);                       // 0x428ee0
    void preload(unsigned,const char*);                // 0x427360
    int load_track(std::int32_t);                      // 0x426890
    int reopen_track(const char*);                     // 0x426be0
    int start_stream(const char*);                     // 0x4286b0
    void stop_stream();                               // 0x428990
    int shutdown();                                   // 0x426170
    std::size_t find_track(const char*) const;          // 0x428420
    bool uses_preload() const;                         // 0x428fa0
#if !defined(TH20_IOS)
    static DWORD WINAPI notification_thread(void*);    // 0x426c70, Windows/web builds only
#endif
};
#if defined(TH20_IOS)
static_assert(sizeof(std::pmr::vector<PreloadedTrack>)==32);
static_assert(offsetof(SoundInf,requests)==0x34);
static_assert(offsetof(SoundInf,preloaded)==0x1898);
static_assert(offsetof(SoundInf,track_formats)==0x18c0);
static_assert(offsetof(SoundInf,effects)==0x19c8);
static_assert(offsetof(SoundInf,source_buffers)==0x27d8);
static_assert(offsetof(SoundInf,commands)==0x2c38);
static_assert(offsetof(SoundInf,stream)==0x5eb8);
static_assert(offsetof(SoundInf,context)==0x5ee8 && sizeof(SoundInf)==0x5ef0);
#else
static_assert(sizeof(std::pmr::vector<PreloadedTrack>)==16);
static_assert(offsetof(SoundInf,preloaded)==0x187c);
static_assert(offsetof(SoundInf,effects)==0x1994);
static_assert(offsetof(SoundInf,commands)==0x2544);
static_assert(offsetof(SoundInf,stream)==0x57c4);
static_assert(offsetof(SoundInf,context)==0x57e8);
#endif
void bind_game_services(); // source adapter: required services, no initialization reordering
}
