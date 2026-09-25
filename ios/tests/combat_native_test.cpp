#include "../../source_reconstruction/bullet_system/command.hpp"
#include "../../source_reconstruction/laser_system/type2.hpp"
#include "../../source_reconstruction/damage_regions/regions.hpp"
#include "../../source_reconstruction/effect_system/effect.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>

using namespace th20::source;
// Test fixture supplies the abstract beam-specific slots; constructor, position
// dispatch, allocation cleanup and the vptr under test are production Laser.
class ProbeLaser final:public laser::Laser {
public:
    int update() override{return 0;} int draw() override{return 0;}
    int finish() override{return 0;}
    int cancel_rectangle(const sprite::Vec3&,const sprite::Vec3&,float,int,int) override{return 0;}
    int cancel_circle(const sprite::Vec3&,float,int,int) override{return 0;}
    int erase(int,int) override{return 0;} int cancel_all(int) override{return 0;}
};
int main() {
    static_assert(sizeof(void*)==8);
    auto name=std::make_unique<char[]>(64);
    std::strcpy(name.get(),"native_enemy_script");
    assert(reinterpret_cast<std::uintptr_t>(name.get())>UINT32_MAX);
    bullet::ShotMetadata metadata;
    assert(metadata.commands.size()==2);
    metadata.commands[0].words[4]=0xa5b6c7d8u;
    metadata.commands[0].set_script_name(name.get());
    metadata.commands.reserve(256);
    auto copy=metadata.commands;
    metadata.commands.clear();
    assert(copy[0].get_script_name()==name.get());
    assert(std::strcmp(copy[0].get_script_name(),"native_enemy_script")==0);
    assert(copy[0].words[4]==0xa5b6c7d8u);
    assert(copy[1].get_script_name()==nullptr);

    auto nodes=std::make_unique<laser::CurveNode[]>(3);
    nodes[0].next=&nodes[1];nodes[1].next=&nodes[2];
    nodes[1].field_04=reinterpret_cast<std::uintptr_t>(&nodes[0]);
    nodes[2].field_04=reinterpret_cast<std::uintptr_t>(&nodes[1]);
    assert(reinterpret_cast<laser::CurveNode*>(nodes[2].field_04)->next==&nodes[2]);

    nodes[0].begin=0;nodes[0].end=10;
    nodes[1].begin=10;nodes[1].end=20;nodes[1].kind=0;
    nodes[1].position={10,20,0};nodes[1].direction={1,0,0};nodes[1].speed=2;
    sprite::Vec3 sample{};float sample_speed=0,sample_angle=0;
    laser::sample_curve_path(nodes.get(),sample,sample_speed,sample_angle,{},0,0,12,false);
    assert(sample.x==14&&sample.y==20&&sample_speed==2);
    {
        std::unique_ptr<laser::Laser> beam=std::make_unique<ProbeLaser>();
        beam->set_position({3,4,5});
        assert(beam->position.x==3&&beam->position.z==5&&beam->flags==0);
        assert(beam->age.current==0&&beam->allocated_6d8==nullptr);
        beam->allocated_6d8=runtime::allocate_bytes(32);
        assert(beam->allocated_6d8);
    }

    auto first=std::make_unique<bullet::Bullet>();
    auto second=std::make_unique<bullet::Bullet>();
    first->field_60=reinterpret_cast<std::uintptr_t>(second.get());
    first->field_58=reinterpret_cast<std::uintptr_t>(name.get());
    first->metadata=std::make_shared<unsigned>(17);
    assert(first->link.value==reinterpret_cast<scheduler::Node*>(first.get()));
    assert(reinterpret_cast<bullet::Bullet*>(first->field_60)==second.get());
    assert(reinterpret_cast<char*>(first->field_58)==name.get());
    assert(*std::static_pointer_cast<unsigned>(first->metadata)==17);

    struct GuardedRegion {std::uint64_t before;damage::Region value;std::uint64_t after;};
    GuardedRegion guarded;std::memset(&guarded,0xa5,sizeof(guarded));
    damage::construct_region(guarded.value);
    assert(guarded.before==0xa5a5a5a5a5a5a5a5ull&&guarded.after==guarded.before);
    assert(guarded.value.link.value==reinterpret_cast<scheduler::Node*>(&guarded.value));
    assert(guarded.value.context==nullptr&&guarded.value.flags==0);
    damage::set_circle(guarded.value,{31,47,0},12,0.25f,120,44);
    assert(guarded.value.flags==3&&guarded.value.motion.position.x==31);
    assert(guarded.value.radius==12&&guarded.value.radius_step==0.25f);
    assert(guarded.value.lifetime.current==120&&guarded.value.damage==44&&guarded.value.period==1);
    assert(guarded.after==guarded.before);
    damage::set_rectangle(guarded.value,{21,24,0},16,20,0,30,99);
    assert(guarded.value.flags==1&&guarded.value.size.x==16&&guarded.value.size.y==20);
    assert(guarded.value.lifetime.current==30&&guarded.value.damage==99);

    effects::Parameters parameters{};parameters.value_20=0xf00dcafe;
    effects::Request request{};request.original_parameters=&parameters;
    request.animation=reinterpret_cast<sprite::Animation*>(name.get());
    request.parameters=parameters;
    const auto request_copy=request;
    assert(request_copy.original_parameters==&parameters);
    assert(request_copy.animation==reinterpret_cast<sprite::Animation*>(name.get()));
    assert(request_copy.parameters.value_20==0xf00dcafe);
    std::puts("PASS: native command copies, high pointers, bullet/curve links and path sampling, Laser construction/vptr/cleanup, guarded region initialization and damage parameters, effect request pointers");
}
