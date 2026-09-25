#pragma once
#include <cstdint>
namespace th20::source::text {
struct NativeTextExtent {int width,height;};
void initialize_native_fonts();
void release_native_fonts();
NativeTextExtent measure_native_text(const char* cp932,int font);
NativeTextExtent raster_native_text(std::uint8_t* bgra,int pitch,int width,int height,
    const char* cp932,int font,int spacing,std::uint32_t foreground,
    std::uint32_t background,bool outline,float radius,int x,int top);
}
