#pragma once
#include "../program_entry/program_entry.hpp"
#include "../runtime_core/callback_owner.hpp"
#include "../sprite_renderer/animation.hpp"
#include <cstdint>
namespace th20::source::sprite {struct RenderMesh;}
namespace th20::source::background {
class Background;
struct FogState {float near_distance,far_distance,blue,green,red,alpha;std::uint32_t color;};
static_assert(sizeof(FogState)==28);
#pragma pack(push,4)
struct Header {
    std::int16_t object_count,animation_count;
    std::uint32_t instance_offset,script_offset,field_0c;
    char animation_name[128];
};
struct Object {std::uint16_t id;std::uint8_t layer,flags;float parameters[11];};
struct Primitive {std::int16_t type,size,script_index,animation_index;};
struct Instance {std::uint16_t object_id,unknown; sprite::Vec3 position;};
struct Instruction {std::int32_t time;std::int16_t opcode,size;};
static_assert(sizeof(Header)==0x90 && sizeof(Object)==0x30 && sizeof(Primitive)==8 && sizeof(Instance)==16 && sizeof(Instruction)==8);
#if defined(TH20_IOS)
#pragma pack(pop)
#endif
struct ScriptState {
    recovered::Timer timer;
    std::uint32_t instruction_offset;
    std::uint8_t camera_motion,padding_15[3];
    recovered::Timer motion_timer,secondary_motion_timer;
    sprite::Interpolation<sprite::Vec3> direction_interpolation,position_interpolation,up_interpolation;
    sprite::Interpolation<FogState> fog_interpolation;
    sprite::Interpolation<float> fov_interpolation;
    program_entry::ViewportState camera;
    Background* owner;
    sprite::Animation animations[8];
    std::uint32_t fields_3294[17];
    recovered::Timer mesh_timers[2];
    float mesh_phase_x[2],mesh_phase_y[2];
    std::uint32_t mesh_mode,overlay_color;
#if defined(TH20_IOS)
    // Runtime addresses live separately from the original numeric VM words.
    sprite::RenderMesh* meshes[2];
#endif
    sprite::RenderMesh* mesh(unsigned index) const noexcept {
#if defined(TH20_IOS)
        return meshes[index];
#else
        return reinterpret_cast<sprite::RenderMesh*>(fields_3294[9+index]);
#endif
    }
    void set_mesh(unsigned index,sprite::RenderMesh* value) noexcept {
#if defined(TH20_IOS)
        meshes[index]=value;
#else
        fields_3294[9+index]=reinterpret_cast<std::uintptr_t>(value);
#endif
    }
};
static_assert(offsetof(ScriptState,direction_interpolation)==0x38);
static_assert(offsetof(ScriptState,fog_interpolation)==0x134);
static_assert(offsetof(ScriptState,fov_interpolation)==0x1d8);
static_assert(offsetof(ScriptState,camera)==0x204);
static_assert(offsetof(ScriptState,owner)==0x370);
#if defined(TH20_IOS)
static_assert(offsetof(ScriptState,animations)==0x378);
static_assert(offsetof(ScriptState,fields_3294)==0x36f8);
static_assert(offsetof(ScriptState,mesh_timers)==0x373c);
static_assert(offsetof(ScriptState,meshes)==0x3778 && sizeof(ScriptState)==0x3788);
#else
static_assert(offsetof(ScriptState,animations)==0x374);
static_assert(offsetof(ScriptState,fields_3294)==0x3294);
static_assert(offsetof(ScriptState,mesh_timers)==0x32d8);
static_assert(sizeof(ScriptState)==0x3310);
#endif
class Background : public runtime::CallbackOwner {
public:
    ScriptState state;
    sprite::Animation* primitive_animations;
    Header* file;
    Object** objects;
    Instance* instances;
    Instruction* instructions;
    sprite::AnimationFile* animation_file;
    std::uint32_t rendered[3],state_flags;
    recovered::Timer fade_timer;
    std::int32_t stage_id;
    std::uint32_t frame_count;
    std::uint8_t* original_data;
    std::uint32_t data_size;
    scheduler::Node* additional_draw;
    Background();                                    //471480
    ~Background() override;                          //471980
    void enable_callbacks() override;                //473490
    int load(const char*);                           //474e70/474e40
    int initialize(const char*,int slot);             //473160
    int update();                                    //472100
    int draw_geometry();                             //472d10
    int draw_foreground();                           //472a30
};
#if !defined(TH20_IOS)
#pragma pack(pop)
static_assert(offsetof(Background,state)==0x10 && offsetof(Background,primitive_animations)==0x3320);
static_assert(offsetof(Background,file)==0x3324 && offsetof(Background,state_flags)==0x3344);
static_assert(offsetof(Background,stage_id)==0x3358 && sizeof(Background)==0x336c);
#else
static_assert(offsetof(Background,state)==0x20 && offsetof(Background,primitive_animations)==0x37a8);
static_assert(offsetof(Background,file)==0x37b0 && offsetof(Background,state_flags)==0x37e4);
static_assert(offsetof(Background,stage_id)==0x37f8 && sizeof(Background)==0x3818);
#endif
void construct_camera(program_entry::ViewportState&) noexcept; //471790, preserves the two matrices
void construct_script_state(ScriptState&) noexcept;             //4715c0
void construct_background_members(Background&) noexcept;       //471480 after CallbackOwner
Background* create_background(const char* file,int slot=0);     //477880/471280
extern Background* primary;                                  //5c069c
extern Background* secondary;                                //5c06a0
void draw_object_layer(Background&,int);                       //474880
namespace unrecovered {
void destroy_render_mesh(void*);                              //471210's24-byte RenderMeshInf
}
}
