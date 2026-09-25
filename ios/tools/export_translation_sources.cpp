// Export read-only inputs for the optional translation-pack compiler.
// Build with archive.cpp and C++17; never distributes the original assets.
#include "../../source_reconstruction/archive/archive.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("Usage: export_translation_sources th20.dat OUTPUT_DIRECTORY");
        th20::source::Archive archive{std::filesystem::path(argv[1])};
        const std::filesystem::path output(argv[2]);std::filesystem::create_directories(output);
        unsigned count=0;
        for(std::size_t i=0;i<archive.entries().size();++i){
            const auto& name=archive.entries()[i].name;
            const std::filesystem::path member(name);const auto ext=member.extension();
            if(ext!=".anm"&&ext!=".msg"&&ext!=".txt")continue;
            if(member.filename()!=member)throw std::runtime_error("Unexpected resource path");
            const auto bytes=archive.read(i);std::ofstream stream(output/member,std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
            if(!stream)throw std::runtime_error("Cannot write translation input");++count;
        }
        std::cout<<count<<" translation inputs exported\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
