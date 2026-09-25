#include "../../../source_reconstruction/player_entity/firing.hpp"
#include "../../../source_reconstruction/player_entity/shot_data.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace pl=th20::source::player_entity;
namespace {
unsigned checks=0;
void check(bool value,const char* description){
    ++checks;
    if(!value){std::fprintf(stderr,"FAIL: %s\n",description);std::abort();}
}
template<class T>void write(std::vector<std::uint8_t>& bytes,std::size_t offset,T value){std::memcpy(bytes.data()+offset,&value,sizeof(value));}
void constructors(){
    pl::Option option;
    std::memset(&option,0xa5,sizeof(option));pl::construct_option(option);
    std::array<std::uint8_t,sizeof(option)> zero{};
    check(std::memcmp(&option,zero.data(),sizeof(option))==0,"Option constructor clears scalar words and both native callbacks");
    option.fields_f4[9]=0xdeadbeefu;
    option.focused_callback=reinterpret_cast<std::uintptr_t>(&constructors);
    option.unfocused_callback=reinterpret_cast<std::uintptr_t>(&check);
    check(option.fields_f4[9]==0xdeadbeefu,"Option callback writes preserve the final scalar word");
    check(option.focused_callback>0xffffffffu&&option.unfocused_callback>0xffffffffu,"callback addresses retain their upper 32 bits");

    pl::Shot shot;
    std::memset(&shot,0xa5,sizeof(shot));pl::construct_shot(shot);
    auto* bytes=reinterpret_cast<const std::uint8_t*>(&shot);
    for(std::size_t i=0;i<sizeof(shot);++i){
        const bool padding=i>=offsetof(pl::Shot,padding_115)&&i<offsetof(pl::Shot,view_index);
        const bool initialized=i==offsetof(pl::Shot,fields_98)+3*sizeof(std::uint32_t);
        check(bytes[i]==(padding?0xa5:initialized?1:0),"Shot constructor preserves only original padding");
    }
    auto owner=std::make_unique<pl::ShotController>();
    std::memset(owner.get(),0xa5,sizeof(*owner));pl::construct_shot_controller(*owner);
    check(owner->active.tail==&owner->active.sentinel&&owner->free.tail==&owner->free.sentinel,"native intrusive list anchors point inside their owning controller");
    for(const auto& pooled:owner->pool)check(pooled.fields_98[3]==1&&pooled.owner==nullptr&&pooled.context==nullptr,"all 256 shots are initialized at native stride");
    for(auto pointer:owner->counters_124e0)check(pointer==0,"all native shot registry pointers are cleared");
    auto* selected=&owner->pool[255];
    owner->counters_124e0[29]=reinterpret_cast<std::uintptr_t>(selected);
    check(reinterpret_cast<pl::Shot*>(owner->counters_124e0[29])==selected,"last native registry entry round-trips its shot address");
    check(reinterpret_cast<std::uintptr_t>(selected)>0xffffffffu,"shot pointer test exercises memory above 4 GiB");

    pl::Feedback feedback;
    std::memset(&feedback,0xa5,sizeof(feedback));pl::construct_feedback(feedback);
    check(feedback.enabled==0&&feedback.view_index==0&&feedback.context==nullptr,"feedback native context and state are initialized");
    for(auto byte:feedback.padding_51)check(byte==0xa5,"feedback original padding is preserved");
}
void shot_resources(){
    constexpr std::size_t count=3,table=0x5d4,data=table+count*4;
    std::vector<std::uint8_t> bytes(data+3*sizeof(pl::ShotRecord));
    write(bytes,2,std::uint16_t(count));
    write(bytes,table,std::int32_t(data));
    write(bytes,table+4,std::int32_t(-1));
    write(bytes,table+8,std::int32_t(data+2*sizeof(pl::ShotRecord)));
    pl::ShotRecord row{};row.period=7;row.phase=3;row.damage=140;row.callbacks[0]=31;
    std::memcpy(bytes.data()+data,&row,sizeof(row));
    const auto original=bytes;
    pl::relocate_shot_data(bytes);
    check(bytes==original,"ARM64 SHT validation preserves every on-disk byte");
    check(reinterpret_cast<std::uintptr_t>(bytes.data())>0xffffffffu,"SHT test uses an address above 4 GiB");
    const auto* first=pl::shot_pattern(bytes.data(),0);
    check(first->period==7&&first->phase==3&&first->damage==140&&first->callbacks[0]==31,"SHT row resolves through its relative table entry");
    check(pl::shot_pattern(bytes.data(),1)==nullptr,"negative SHT sentinel stays absent");
    check(pl::shot_pattern(bytes.data(),2)==reinterpret_cast<const pl::ShotRecord*>(bytes.data()+data+2*sizeof(pl::ShotRecord)),"last complete SHT record is accepted");
    check(pl::shot_pattern(bytes.data(),-1)==nullptr&&pl::shot_pattern(bytes.data(),3)==nullptr,"out-of-range native pattern indices do not read outside the table");
    for(std::size_t invalid:{bytes.size(),bytes.size()-sizeof(pl::ShotRecord)+1,bytes.size()+1}){
        auto malformed=original;write(malformed,table+8,std::int32_t(invalid));const auto before=malformed;
        bool rejected=false;try{pl::relocate_shot_data(malformed);}catch(const std::runtime_error&){rejected=true;}
        check(rejected&&malformed==before,"truncated and out-of-resource records are rejected before mutation");
    }
    std::vector<std::uint8_t> truncated(table+3);write(truncated,2,std::uint16_t(1));
    bool rejected=false;try{pl::relocate_shot_data(truncated);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"truncated SHT table is rejected");
    std::vector<std::uint8_t> empty_pattern(table+4+1);
    write(empty_pattern,2,std::uint16_t(1));write(empty_pattern,table,std::int32_t(table+4));empty_pattern.back()=0xff;
    pl::relocate_shot_data(empty_pattern);
    check(pl::shot_pattern(empty_pattern.data(),0)->period==-1,"single-byte empty pattern at resource end remains valid");
}
}
int main(){
    constructors();shot_resources();
    std::printf("PASS: %u player native layout, constructor, pointer and SHT checks\n",checks);
    std::printf("Option=%zu Shot=%zu ShotController=%zu Player=%zu player.shots=%zu player.context=%zu\n",sizeof(pl::Option),sizeof(pl::Shot),sizeof(pl::ShotController),sizeof(pl::Player),offsetof(pl::Player,shots),offsetof(pl::Player,context));
}
