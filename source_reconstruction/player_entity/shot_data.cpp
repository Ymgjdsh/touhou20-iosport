#include "shot_data.hpp"
#include "firing.hpp"
#include <cstring>
#include <stdexcept>
namespace th20::source::player_entity {
void relocate_shot_data(std::span<std::uint8_t> bytes){
    if(bytes.size()<0x5d4)throw std::runtime_error("Truncated player SHT header");
    std::uint16_t count;std::memcpy(&count,bytes.data()+2,2);
    if(count>(bytes.size()-0x5d4)/4)throw std::runtime_error("Truncated player SHT offset table");
    for(unsigned i=0;i<count;++i){
        std::int32_t offset;std::memcpy(&offset,bytes.data()+0x5d4+i*4,4);
        if(offset>=0){
            const auto start=std::size_t(offset);
            if(start>=bytes.size())throw std::runtime_error("Player SHT record outside resource");
            // An empty pattern is a single negative period byte, including at
            // the end of the real SHT resource. Only live rows need 0x78 bytes.
            if(bytes[start]<0x80u&&bytes.size()-start<sizeof(ShotRecord))
                throw std::runtime_error("Truncated player SHT record");
        }
    }
#if !defined(TH20_IOS)
    for(unsigned i=0;i<count;++i){std::int32_t offset;auto* entry=bytes.data()+0x5d4+i*4;std::memcpy(&offset,entry,4);if(offset>=0){const auto pointer=reinterpret_cast<std::uint32_t>(bytes.data())+std::uint32_t(offset);std::memcpy(entry,&pointer,4);}}
#endif
}
const ShotRecord* shot_pattern(const void* data,std::int32_t pattern) noexcept {
    const auto* bytes=static_cast<const std::uint8_t*>(data);
#if defined(TH20_IOS)
    std::uint16_t count;std::memcpy(&count,bytes+2,2);
    if(pattern<0||std::uint32_t(pattern)>=count)return nullptr;
    std::int32_t offset;std::memcpy(&offset,bytes+0x5d4+std::uint32_t(pattern)*4u,4);
    return offset<0?nullptr:reinterpret_cast<const ShotRecord*>(bytes+offset);
#else
    const ShotRecord* rows;std::memcpy(&rows,bytes+0x5d4+std::uint32_t(pattern)*4u,sizeof(rows));return rows;
#endif
}
}
