#pragma once
#include "player.hpp"
#include "../core_scheduler/scheduler.hpp"
#include "../runtime_state/motion.hpp"
namespace th20::source::player_entity {
struct ShotController;
struct Fixed2 {std::int32_t x,y;};
struct Option { //4f49c0, Player+684 ten records, +123c twelve records
    std::uint32_t state;
    sprite::Vec3 position,previous_position,vectors_1c[7];
    Fixed2 vector_70,vector_78,offsets_80[7];
    sprite::Vec2 vector_b8,vector_c0;
    sprite::Vec3 vector_c8;
    std::uint32_t field_d4,field_d8,handle_dc,handle_e0;
    recovered::Timer timer_e4;
    std::uint32_t fields_f4[10];
    std::uintptr_t focused_callback,unfocused_callback;
    std::int32_t view_index;
    game_session::Context* context;
};
struct Shot { //4f4cb0, 256 records in Player+22b4
    scheduler::Link link;
    std::uint32_t flags,handle_18;
    recovered::Timer timer_1c,timer_2c;
    std::uint32_t control_flags;
    std::uintptr_t initialize_callback,update_callback,extra_callback,hit_callback;
    state::Motion motion;
    std::uint32_t fields_98[6];
    sprite::Vec2 vector_b0;
    std::uint32_t fields_b8[18];
    sprite::Vec3 vector_100;
    std::uintptr_t field_10c,field_110; // animation file and attached option addresses
    std::uint8_t byte_114,padding_115[3];
    std::int32_t view_index;
    ShotController* owner;
    game_session::Context* context;
};
struct ShotController { //4f4b80, source of PlayerInf's fixed shot pool
    Shot pool[256];
    recovered::Timer timer_12400,timer_12410,timer_12420;
    scheduler::List active,free;
    std::uint32_t field_12460,field_12464,counters_12468[30];
    std::uintptr_t counters_124e0[30]; // live Shot addresses
    std::uintptr_t field_12558; // SHT byte buffer address
    std::uint32_t field_1255c,handle_12560,field_12564;
    recovered::Timer timer_12568;
    std::uint32_t field_12578;
    std::uint8_t byte_1257c,padding_1257d[3];
    recovered::Timer timer_12580;
    std::int32_t view_index;
    game_session::Context* context;
};
struct Feedback { //4f4580, Player+2244, owns one ANM handle at+4c
    recovered::Timer timers[3];
    std::uint32_t fields_30[4];
    sprite::Vec3 vector_40;
    std::uint32_t handle_4c;
    std::uint8_t enabled,padding_51[3];
    std::int32_t view_index;
    game_session::Context* context;
};
static_assert(offsetof(Option,handle_dc)==0xdc&&offsetof(Option,fields_f4)==0xf4);
#if defined(TH20_IOS)
static_assert(sizeof(Option)==0x140&&offsetof(Option,focused_callback)==0x120&&offsetof(Option,context)==0x138);
static_assert(sizeof(Shot)==0x160&&offsetof(Shot,motion)==0x78&&offsetof(Shot,byte_114)==0x148);
static_assert(sizeof(ShotController)==0x16248&&offsetof(ShotController,active)==0x16030&&offsetof(ShotController,view_index)==0x1623c);
static_assert(sizeof(Feedback)==0x60&&offsetof(Feedback,enabled)==0x50&&offsetof(Feedback,context)==0x58);
#else
static_assert(sizeof(Option)==0x12c&&offsetof(Option,focused_callback)==0x11c&&offsetof(Option,context)==0x128);
static_assert(sizeof(Shot)==0x124&&offsetof(Shot,motion)==0x50&&offsetof(Shot,byte_114)==0x114);
static_assert(sizeof(ShotController)==0x12598&&offsetof(ShotController,active)==0x12430&&offsetof(ShotController,view_index)==0x12590);
static_assert(sizeof(Feedback)==0x5c&&offsetof(Feedback,enabled)==0x50);
#endif
void construct_option(Option&) noexcept;                     //4f49c0
void construct_shot(Shot&) noexcept;                         //4f4cb0
void construct_shot_controller(ShotController&) noexcept;     //4f4b80/4f41d0
void construct_feedback(Feedback&) noexcept;                 //4f4580
}
