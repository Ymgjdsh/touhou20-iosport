#include "archive.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <iomanip>
#include <CommonCrypto/CommonDigest.h>
using namespace th20::source;
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("Usage: archive_stream_test /path/to/th20.dat hashes.tsv");
        std::ofstream hashes(argv[2]);if(!hashes)throw std::runtime_error("Cannot write hash evidence");
        std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
        if(!input)throw std::runtime_error("Cannot open test archive");
        const auto size=input.tellg();Bytes bytes(static_cast<std::size_t>(size));input.seekg(0);
        if(!input.read(reinterpret_cast<char*>(bytes.data()),size))throw std::runtime_error("Cannot read reference archive");
        Archive memory(std::move(bytes)),disk{std::filesystem::path(argv[1])};
        if(memory.entries().size()!=disk.entries().size())throw std::runtime_error("Catalog count differs");
        std::uint64_t decoded=0;
        for(std::size_t i=0;i<memory.entries().size();++i){
            const auto a=memory.read(i),b=disk.read(i);
            if(a!=b)throw std::runtime_error("Streaming bytes differ: "+disk.entries()[i].name);
            unsigned char hash[CC_SHA256_DIGEST_LENGTH];CC_SHA256(b.data(),CC_LONG(b.size()),hash);
            hashes<<std::dec<<i<<'\t'<<b.size()<<'\t';
            for(auto byte:hash)hashes<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);
            hashes<<'\n';
            decoded+=a.size();
        }
        // Nonsequential reads verify seeking and decoder state are preserved.
        for(std::size_t i=memory.entries().size();i>0;--i)
            if(memory.read(i-1)!=disk.read(i-1))throw std::runtime_error("Reverse read differs");
        std::cout<<"PASS archive file/memory parity entries="<<disk.entries().size()<<" decoded_bytes="<<decoded<<" reverse_reads=passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
