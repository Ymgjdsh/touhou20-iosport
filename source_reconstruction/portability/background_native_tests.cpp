#include "../stage_background/resource_layout.hpp"
#include "../sprite_renderer/render_mesh.hpp"
#include "../archive/archive.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
namespace b=th20::source::background;namespace s=th20::source::sprite;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
unsigned objects_tested=0,primitives_tested=0;
void file_test(const th20::source::Bytes& bytes){
    auto copy=bytes;require(copy.size()>=sizeof(b::Header),"STD header");auto* header=reinterpret_cast<b::Header*>(copy.data());
    std::vector<b::Object*> objects(header->object_count);
    for(unsigned i=0;i<objects.size();++i){
        objects[i]=b::resolve_object_reference(header,copy.size(),i);require(objects[i]!=nullptr,"STD 32-bit object offset resolution");++objects_tested;
        auto offset=static_cast<std::size_t>(reinterpret_cast<std::uint8_t*>(objects[i]+1)-copy.data());
        for(;;){require(offset+sizeof(b::Primitive)<=copy.size(),"STD primitive bounds");b::Primitive primitive;std::memcpy(&primitive,copy.data()+offset,sizeof(primitive));if(primitive.type<0)break;require(primitive.size>=sizeof(b::Primitive),"STD primitive stride");offset+=primitive.size;++primitives_tested;}
    }
    require(copy==bytes,"native pointers changed serialized STD offset table");
    require(!b::resolve_object_reference(header,copy.size(),objects.size()),"STD invalid object index");
    if(!objects.empty()){
        const std::uint32_t invalid=0xfffffffcu;std::memcpy(copy.data()+sizeof(b::Header),&invalid,4);
        require(!b::resolve_object_reference(header,copy.size(),0),"STD invalid object offset");
        require(!b::resolve_object_reference(header,sizeof(b::Header),0),"STD truncated object table");
    }
}
void state_test(){
    b::ScriptState state;std::memset(&state,0xa5,sizeof(state));
    b::construct_script_state(state);require(state.owner==nullptr&&state.mesh(0)==nullptr&&state.mesh(1)==nullptr,"background native pointer initialization");
    for(auto& a:state.animations)for(auto& link:a.links)require(link.value==&a&&link.next==nullptr,"background embedded animation link stride");
    const auto words_before=state.fields_3294[9];s::RenderMesh meshes[2]{};
    state.set_mesh(0,&meshes[0]);state.set_mesh(1,&meshes[1]);require(state.mesh(0)==&meshes[0]&&state.mesh(1)==&meshes[1],"background mesh pointer roundtrip");
#if defined(TH20_IOS)
    require(reinterpret_cast<std::uintptr_t>(&meshes[0])>0xffffffffu,"mesh test must exercise high pointer bits");
    require(state.fields_3294[9]==words_before,"native mesh pointer overwrote numeric script state");
#else
    (void)words_before;
#endif
    state.set_mesh(0,nullptr);state.set_mesh(1,nullptr);
}
}
int main(int argc,char** argv){
    require(argc>=2,"supply archive or STD fixture directory");state_test();unsigned files=0;
    const std::filesystem::path input(argv[1]);
    if(std::filesystem::is_directory(input)){
        for(const auto& entry:std::filesystem::directory_iterator(input))if(entry.path().extension()==".std"){
            std::ifstream stream(entry.path(),std::ios::binary);th20::source::Bytes bytes((std::istreambuf_iterator<char>(stream)),{});file_test(bytes);++files;
        }
    }else {
        th20::source::Archive archive(input);
        for(const auto& entry:archive.entries())if(std::filesystem::path(entry.name).extension()==".std"){
            auto bytes=archive.read(entry.name);file_test(bytes);++files;
            if(argc>2){std::filesystem::create_directories(argv[2]);std::ofstream out(std::filesystem::path(argv[2])/entry.name,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
        }
    }
    require(files==10,"expected ten original STD resources");
    std::printf("PASS: %u original STD files, %u object references, %u primitives; disk offset tables unchanged; embedded animation links and full-width mesh pointers valid\n",files,objects_tested,primitives_tested);
}
