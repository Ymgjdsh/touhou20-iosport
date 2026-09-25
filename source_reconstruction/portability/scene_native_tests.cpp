#include "../stone_menu/cursor.hpp"
#include "../trophy_system/trophy.hpp"
#include "../replay_system/replay.hpp"
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <stdexcept>
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
class CheckedResource final:public std::pmr::memory_resource {
    struct Allocation {std::size_t bytes,alignment;};
    std::map<void*,Allocation> live;
    void* do_allocate(std::size_t size,std::size_t alignment) override {
        auto* result=std::pmr::new_delete_resource()->allocate(size,alignment);
        std::memset(result,0xa5,size);live.emplace(result,Allocation{size,alignment});return result;
    }
    void do_deallocate(void* pointer,std::size_t size,std::size_t alignment) override {
        const auto found=live.find(pointer);require(found!=live.end(),"unowned allocation released");
        require(found->second.bytes==size&&found->second.alignment==alignment,"allocator size/alignment mismatch");
        live.erase(found);std::pmr::new_delete_resource()->deallocate(pointer,size,alignment);
    }
    bool do_is_equal(const memory_resource& other)const noexcept override{return this==&other;}
public:
    ~CheckedResource(){if(!live.empty())std::abort();}
};
void queue_test(){
    CheckedResource resource;auto* previous=std::pmr::set_default_resource(&resource);
    {
        th20::source::trophy::Queue actual;std::deque<int> expected;
        std::uint32_t random=0x13579bdf;
        for(unsigned step=0;step<120000;++step){
            random=random*1664525u+1013904223u;
            if(expected.empty()||(random&7u)<5){const auto value=th20::recovered::signed_bits(random);actual.push(value);expected.push_back(value);}
            else {require(actual.pop_front()==expected.front(),"trophy deque order");expected.pop_front();}
            require(actual.count==expected.size(),"trophy deque count");
        }
        while(!expected.empty()){require(actual.pop_front()==expected.front(),"trophy deque final drain");expected.pop_front();}
        require(actual.pop_front()==0&&actual.offset==0,"empty trophy deque");
    }
    std::pmr::set_default_resource(previous);
}
void cursor_test(){
    th20::source::menu::Cursor cursor;cursor.count=7;cursor.excluded={1,3,6};
    const int forward[]{2,4,5,0,2,4,5,0};for(int value:forward)require(cursor.move(1)==value,"menu cursor forward exclusions");
    const int backward[]{5,4,2,0};for(int value:backward)require(cursor.move(-1)==value,"menu cursor backward exclusions");
    cursor.wrapping=0;require(cursor.move(-1)==0,"menu cursor clamp");
    cursor.snapshot();require(!cursor.changed(),"menu cursor snapshot");cursor.select(2);require(cursor.changed(),"menu cursor changed");
}
void replay_test(){
    using namespace th20::source::replay;
    auto chunk=std::make_unique<RecordingChunk>();
    require(reinterpret_cast<std::uintptr_t>(chunk.get())==reinterpret_cast<std::uintptr_t>(chunk->link.value),"recording owner pointer");
    for(unsigned frame=0;frame<36000;++frame){const auto full=chunk->append(frame,frame>>1,~frame);require(full==(frame==35999),"recording rollover");}
    require(chunk->frame_count()==36000&&chunk->inputs[35999].pressed==(35999>>1),"recording payload");
    bool rejected=false;try{chunk->append(0,0,0);}catch(const std::out_of_range&){rejected=true;}require(rejected,"recording bounds");
    for(unsigned frame=0;frame<1200;++frame)require(chunk->append_fps(60)==(frame==1199),"FPS rollover");
    PlaybackCursor cursor;cursor.inputs=chunk->inputs;cursor.fps=chunk->fps;cursor.rewind(true);require(cursor.input_cursor==chunk->inputs&&cursor.fps_cursor==chunk->fps&&cursor.frame==0,"playback active rewind");cursor.rewind(false);require(cursor.frame==-1,"playback inactive rewind");
    FileHeader header;require(header.magic==0x72303274&&sizeof(header)==48,"replay file header remains original format");
}
}
int main(){queue_test();cursor_test();replay_test();std::puts("PASS: 120000 trophy queue operations with poisoned allocations, menu exclusions, 36000 replay frames and 1200 FPS samples");}
