#pragma once
#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>
#include <cstddef>
#include <cstdint>
#include "animation.hpp"
#include <memory_resource>
#include <string>
#include "../runtime_core/worker.hpp"

namespace th20::source::sprite {
#if !defined(TH20_IOS)
#pragma pack(push,1)
#endif
struct Vertex20 { float x,y,z,rhw; std::uint32_t color; };
struct Vertex28 { float x,y,z,rhw; std::uint32_t color; float u,v; };
struct TexturedCorner20 { float x,y,z,u,v; };
struct WorldVertex24 {float x,y,z;std::uint32_t color;float u,v;};
struct TextureHeader {
    std::uint8_t unknown_00[10];
    std::uint16_t width,height,format; // +0xa,+0xc,+0xe in original loaded header
};
struct TextureRecord {
    IDirect3DTexture9* texture;
    std::uintptr_t unknown_04;                 // external texture data address
    std::uint32_t unknown_08;
    std::uint32_t bytes_per_pixel;
    TextureHeader* header;
    std::uint32_t flags;
};
struct SpriteData {                             // 0x58-byte sprite descriptor
    std::uint32_t field_00,field_04,texture_id;
    float left,top,right,bottom;                 // +0xc..0x18
    float texture_extent_1c,texture_extent_20;
    std::uint32_t fields_24[5];
    float u0,v0,u1,v1;                          // +0x38..0x44
    float extent_48,extent_4c,scale_50,scale_54;
};
struct AnmInstruction;
struct AnimationFile {
    std::uint32_t id;
    std::pmr::string filename;
#if defined(TH20_WEB)
    std::uint8_t web_filename_abi_padding[12]{};
#endif
    std::pmr::string stem;                       // filesystem filename()/stem(), not parent_path()
#if defined(TH20_WEB)
    std::uint8_t web_stem_abi_padding[12]{};
#endif
    std::uint8_t* bytes;
    Animation* templates;
    std::int32_t texture_count;
    std::uint32_t script_count,sprite_count;
    SpriteData* sprites;
    AnmInstruction** scripts;
    TextureRecord* textures;
    std::uint32_t fields_5c[4];
    std::uintptr_t auxiliary_data;
};
// Constructor and lifetime are recovered in controller.cpp.
// Unknown regions will acquire real types as ANM/pool behavior is recovered.
// Always allocate on the heap: original size is 0x7d40e94 bytes (large pools).
struct Controller {
    std::uint8_t field_00,padding_01[3];
#if defined(TH20_IOS)
    alignas(runtime::Worker) std::uint8_t worker_storage[sizeof(runtime::Worker)];
#else
    std::uint8_t worker_storage[16];
#endif
    std::uint32_t field_14;
    std::uint32_t draw_state[4][10];
    std::uint32_t field_b8,field_bc,field_c0;
    std::uint32_t draw_calls;
    std::uint32_t fields_c8[5];
    Animation animation_dc;
    std::uint32_t field_6c0,field_6c4;
    struct AnimationList { AnimationLink sentinel;AnimationLink* tail; } lists[3];
    PooledAnimation pool[0x10000];
    std::uint32_t field_6000710,field_6000714;
    AnimationLink free_sentinel;
    std::uint32_t field_600072c;
    AnimationFile* files[42];                    // +0x6000730
    Matrix4 matrix_60007d8;
    Animation animation_6000818;
    std::uint32_t padding_6000dfc;
    std::uint32_t field_e00,field_e04,cached_texture;
    std::uint8_t blend_mode,unknown_cached_e0d,unknown_cached_e0e,field_e0f;
    std::uint8_t unknown_cached_e10,field_e11,field_e12,field_e13,field_e14;
    std::uint8_t padding_e15[3];
    std::uintptr_t field_e18;                   // last sprite descriptor address
    IDirect3DVertexBuffer9* corner_buffer;        // +0x6000e1c
    TexturedCorner20 corners[4];                // +0x6000e20, D3DFVF_XYZ|TEX1
    std::uint32_t quad_count;                   // +0x6000e70
    Vertex28 textured_vertices[0x100000];
    Vertex28* textured_write;                   // +0x7c00e74
    Vertex28* textured_batch_start;             // +0x7c00e78
    std::uint32_t colored_primitive_count;       // +0x7c00e7c
    Vertex20 colored_vertices[0x10000];
    Vertex20* colored_write;                    // +0x7d40e80
    Vertex20* colored_batch_start;
    std::uint32_t field_7d40e88,field_7d40e8c,field_7d40e90;
#if defined(TH20_IOS)
    // Original group 1 aliases the beginning of pool[0]. Give native runtime
    // owners independent storage instead of corrupting the first animation.
    AnimationList alternate_lists[3];
#endif
};
#if !defined(TH20_IOS)
#pragma pack(pop)
static_assert(sizeof(void*)==4);
#endif
static_assert(sizeof(Vertex20)==20 && sizeof(Vertex28)==28);
static_assert(sizeof(TexturedCorner20)==20 && sizeof(WorldVertex24)==24);
static_assert(sizeof(SpriteData)==0x58);
#if defined(TH20_IOS)
static_assert(sizeof(TextureRecord)==0x28 && offsetof(TextureRecord,header)==0x18);
static_assert(sizeof(AnimationFile)==0x98 && offsetof(AnimationFile,bytes)==0x48);
static_assert(offsetof(AnimationFile,textures)==0x78 && offsetof(AnimationFile,auxiliary_data)==0x90);
static_assert(offsetof(Controller,worker_storage)%alignof(runtime::Worker)==0);
static_assert(offsetof(Controller,animation_dc)==0xe0 && offsetof(Controller,pool)==0x7e8);
static_assert(offsetof(Controller,files)==0x6a00820);
static_assert(offsetof(Controller,animation_6000818)==0x6a009b0);
static_assert(offsetof(Controller,textured_write)==0x86010a8);
static_assert(offsetof(Controller,colored_write)==0x87410c0);
static_assert(sizeof(Controller)==0x8741170);
#else
static_assert(sizeof(TextureRecord)==0x18);
static_assert(sizeof(AnimationFile)==0x70);
static_assert(offsetof(AnimationFile,bytes)==0x3c);
static_assert(offsetof(AnimationFile,textures)==0x58);
static_assert(offsetof(Controller,files)==0x6000730);
static_assert(offsetof(Controller,animation_dc)==0xdc);
static_assert(offsetof(Controller,pool)==0x710);
static_assert(offsetof(Controller,animation_6000818)==0x6000818);
static_assert(offsetof(Controller,cached_texture)==0x6000e08);
static_assert(offsetof(Controller,corners)==0x6000e20);
static_assert(offsetof(Controller,quad_count)==0x6000e70);
static_assert(offsetof(Controller,textured_write)==0x7c00e74);
static_assert(offsetof(Controller,colored_write)==0x7d40e80);
static_assert(sizeof(Controller)==0x7d40e94);
#endif

// Keep the recovered strict upper bound, without integer-truncating addresses
// or deriving the bound from the original enclosing Controller byte offsets.
inline bool colored_buffer_space(const Controller& c,std::size_t bytes) noexcept {
    const auto begin=reinterpret_cast<std::uintptr_t>(c.colored_vertices);
    const auto end=reinterpret_cast<std::uintptr_t>(c.colored_vertices+0x10000);
    const auto cursor=reinterpret_cast<std::uintptr_t>(c.colored_write);
    return cursor>=begin && cursor<end && bytes<end-cursor;
}
inline bool colored_vertex_space(const Controller& c,std::size_t vertices) noexcept {
    // Check before multiplying so even malformed script counts cannot wrap.
    return vertices<0x10000 && colored_buffer_space(c,vertices*sizeof(Vertex20));
}

void initialize_corner(TexturedCorner20&) noexcept;         // 0x449110
void initialize_colored_vertex(Vertex20&) noexcept;         // 0x449140
void initialize_textured_vertex(Vertex28&) noexcept;        // 0x449170
void prepare_buffers(Controller&) noexcept;                 // 0x445a40
void flush_textured_quads(Controller&,IDirect3DDevice9&);    // 0x4455c0
void release_device_textures(Controller&);                  // 0x41dbe0
void recreate_device_textures(Controller&,IDirect3DDevice9&,D3DFORMAT); // 0x41d9f0
std::int32_t create_render_target(TextureRecord&,IDirect3DDevice9&,UINT,UINT,D3DFORMAT); // 0x44c150
std::uint32_t create_dynamic_texture(TextureRecord&,IDirect3DDevice9&,UINT,UINT,UINT); // 0x44c0b0
}
