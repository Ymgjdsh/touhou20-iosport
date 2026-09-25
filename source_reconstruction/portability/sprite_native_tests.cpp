#include "../sprite_renderer/animation.hpp"
#include "../sprite_renderer/sprite.hpp"
#include "../sprite_renderer/binding.hpp"
#include "../sprite_renderer/pool.hpp"
#include "../sprite_renderer/render_state.hpp"
#include "../sprite_renderer/loading_interrupt.hpp"
#include "../sprite_renderer/dispatch.hpp"
#include "../core_scheduler/scheduler.hpp"
#include "../runtime_core/runtime_core.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace s=th20::source::sprite;
namespace q=th20::source::scheduler;
namespace r=th20::source::runtime;
namespace {
void require(bool condition,const char* what){if(!condition)throw std::runtime_error(what);}
struct Callback final:s::AnimationCallback {
    unsigned* destroyed;unsigned* interrupts;
    explicit Callback(unsigned& d,unsigned& i):destroyed(&d),interrupts(&i){}
    ~Callback() override {++*destroyed;}
    int update() override{return 0;}
    void draw() override{}
    void retire() override{}
    void interrupt(int) override{++*interrupts;}
};
void check_suffix() {
    s::Animation parent{},child{};s::construct_animation(parent);s::construct_animation(child);
    parent.field_4e8=reinterpret_cast<std::uintptr_t>(&parent.links[0]);
    s::attach_animation_parent(child,&parent);
    require(child.root_parent==reinterpret_cast<std::uintptr_t>(&parent),"root animation pointer truncated");
    require(child.direct_parent==reinterpret_cast<std::uintptr_t>(&parent),"direct animation pointer truncated");
    require(child.field_4e8==parent.field_4e8,"animation list pointer not inherited");
    unsigned destroyed=0,interrupts=0;
    child.callback=reinterpret_cast<std::uintptr_t>(new Callback(destroyed,interrupts));
    child.geometry=reinterpret_cast<std::uintptr_t>(r::allocate_bytes(257));child.geometry_bytes=257;
    std::memset(reinterpret_cast<void*>(child.geometry),0x5a,257);
    s::set_animation_interrupt(child,17);require(interrupts==1&&child.base.field_438==17,"virtual interrupt callback failed");
    s::destroy_animation_contents(child);
    require(destroyed==1&&!child.callback&&!child.geometry&&!child.geometry_bytes,"animation resources were not retired");
    s::reset_animation_state(child);require(!child.root_parent&&!child.direct_parent,"reset retained parent references");
    require(child.base.vector_50.x==1.f&&child.base.fields_444[14]==0x40490fdbu,"numeric animation reset changed");
}
void check_pool_and_buffers() {
    // Sparse value initialization is sufficient; game construction additionally
    // registers callbacks and initializes the full original-capacity pools.
    auto owner=std::make_unique<s::Controller>();auto& c=*owner;
    for(auto& list:c.lists)s::initialize_animation_list(list);
    for(auto& list:c.alternate_lists)s::initialize_animation_list(list);
    s::initialize_animation_link(c.free_sentinel,nullptr);
    auto& pooled=c.pool[3];s::construct_pooled_animation(pooled);pooled.index=3;pooled.animation.index=3;
    s::initialize_animation_link(pooled.free_link,&pooled.animation);
    q::insert_after(reinterpret_cast<q::Link&>(c.free_sentinel),reinterpret_cast<q::Link&>(pooled.free_link));
    auto* animation=s::allocate_animation(c);require(animation==&pooled.animation&&pooled.active,"pool allocation failed");
    const auto handle=s::register_animation(c,c.alternate_lists,*animation,false,false);
    require(s::find_animation(c,handle)==animation,"alternate-group pooled handle lookup failed");
    require(animation->field_4e8==reinterpret_cast<std::uintptr_t>(c.alternate_lists),"native group address truncated");
    auto* heap=s::allocate_animation(c);require(heap->index==0xffff,"exhausted pool did not allocate a native heap animation");
    const auto heap_handle=s::register_animation(c,c.alternate_lists,*heap,false,false);
    require(s::find_animation(c,heap_handle)==heap,"alternate-group heap handle lookup failed");
    animation->base.fields_10_28[1]=7;heap->base.fields_10_28[1]=7;
    require(s::select_layer_animations(c.alternate_lists,7,false)==2,"alternate-group layer selection missed an animation");
    require(c.alternate_lists[2].sentinel.next==&animation->links[1]&&animation->links[1].next==&heap->links[1],"alternate-group draw order changed");
    heap->retirement=1;
    require(s::select_layer_animations(c.alternate_lists,7,false)==1,"retired animation remained in selected layer");
    heap->retirement=0;
    s::retire_animation(c,*heap);require(!s::find_animation(c,heap_handle),"retired heap handle stayed live");
    s::retire_animation(c,*animation);require(!pooled.active&&!s::find_animation(c,handle),"pool retirement failed");
    require(!c.alternate_lists[0].sentinel.next,"alternate group kept a retired node");
    s::prepare_buffers(c);
    const s::Vertex28 quad[4]{{1,2,3,4,5,6,7},{8,9,10,11,12,13,14},{15,16,17,18,19,20,21},{22,23,24,25,26,27,28}};
    require(s::append_textured_quad(c,quad)==0&&c.quad_count==1,"quad append failed");
    require(c.textured_write==c.textured_vertices+6,"quad pointer increment failed");
    const unsigned order[6]{0,1,2,1,2,3};
    for(unsigned i=0;i<6;++i)require(!std::memcmp(c.textured_vertices+i,quad+order[i],sizeof(s::Vertex28)),"triangle vertex order changed");
    c.textured_write=c.textured_vertices+0x100000-6;
    require(s::append_textured_quad(c,quad)==1,"original last-six-vertex reservation changed");
    c.colored_write=c.colored_vertices+0x10000-1;
    require(s::colored_buffer_space(c,0)&&s::colored_buffer_space(c,19)&&!s::colored_buffer_space(c,20),"colored strict upper bound changed");
    require(!s::colored_buffer_space(c,std::size_t(-1)),"colored reservation overflow accepted");
    require(!s::colored_vertex_space(c,std::size_t(-1)),"colored vertex multiplication overflow accepted");
    c.colored_write=c.colored_vertices+0x10000-10;
    require(s::colored_vertex_space(c,9)&&!s::colored_vertex_space(c,10),"colored vertex reservation crossed the endpoint");
    c.colored_write=c.colored_vertices+0x10000;
    require(!s::colored_buffer_space(c,0),"colored end pointer accepted");
}
}
int main(){try{check_suffix();check_pool_and_buffers();std::printf("TH20 native sprite PASS animation=%zu pooled=%zu controller=%zu\n",sizeof(s::Animation),sizeof(s::PooledAnimation),sizeof(s::Controller));return 0;}catch(const std::exception& error){std::fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
