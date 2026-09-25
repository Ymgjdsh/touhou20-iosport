#pragma once
#include "background.hpp"
#include <cstring>
namespace th20::source::background {
// STD files contain 32-bit relative offsets, including in a 64-bit runtime.
// Resolve into separately owned runtime pointer storage; never rewrite a file
// offset table with native pointers.
inline Object* resolve_object_reference(Header* file,std::size_t bytes,unsigned index) noexcept {
    if(!file || bytes<sizeof(Header) || file->object_count<0 || index>=static_cast<unsigned>(file->object_count))return nullptr;
    const std::size_t table_bytes=static_cast<unsigned>(file->object_count)*sizeof(std::uint32_t);
    if(table_bytes>bytes-sizeof(Header))return nullptr;
    auto* begin=reinterpret_cast<std::uint8_t*>(file);
    std::uint32_t offset;std::memcpy(&offset,begin+sizeof(Header)+index*sizeof(offset),sizeof(offset));
    if(offset<sizeof(Header)+table_bytes || offset>bytes-sizeof(Object) || offset%alignof(Object)!=0)return nullptr;
    return reinterpret_cast<Object*>(begin+offset);
}
}
