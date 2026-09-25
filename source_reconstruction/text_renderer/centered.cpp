#include "centered.hpp"
#include "centered_constants.hpp"
#include "raster.hpp"
#include "../sprite_renderer/binding.hpp"
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <emmintrin.h>
#include <stdexcept>
#if defined(TH20_IOS)
#include "native_font.hpp"
#include "ios_language.h"
#include <algorithm>
#endif
namespace th20::source::text {
SIZE write_midpoint_animation_text(sprite::Controller& controller,sprite::Animation& animation,std::uint32_t foreground,
    std::uint32_t background,int font,int spacing,std::uint8_t* ready,std::function<void()> completion,const char* format,...){
    std::lock_guard lock(runtime::shared_locks().slot(9));char formatted[1280];va_list arguments;va_start(arguments,format);vsprintf_s(formatted,sizeof(formatted),format,arguments);va_end(arguments);
    if(font<0||font>=22)throw std::out_of_range("Midpoint text font outside original table");
    auto& data=sprite::current_sprite(controller,animation);RECT rectangle;std::memcpy(&rectangle,animation.base.fields_3a0+2,sizeof(rectangle));
    animation.base.flags[0]|=0x10000u;const bool outline=(animation.base.flags[1]&0x400u)==0;
    const auto advance=recovered::add32(recovered::mul32(centered_font_widths[font],2),-1);
    auto measured=_mm_cvtss_f32(_mm_div_ss(_mm_set_ss(recovered::mul32(static_cast<float>(std::strlen(formatted)),advance)),_mm_set_ss(4)));
#if defined(TH20_IOS)
    if(th20::ios::language::effective()==2)measured=std::min(float(measure_native_text(formatted,font).width),data.extent_4c-8.f)*.5f;
#endif
    const int x=_mm_cvttss_si32(_mm_set_ss(recovered::add32(recovered::mul32(data.extent_4c,.5f),-measured)));
    return queue_text(rectangle,x,foreground,outline?background:0,formatted,*sprite::texture(controller,data.texture_id),font,recovered::signed_bits(std::uint32_t(spacing)<<1),outline,ready,std::move(completion));
}
SIZE write_centered_animation_text(sprite::Controller& controller,sprite::Animation& animation,std::uint32_t foreground,
    std::uint32_t background,int font,int spacing,std::uint8_t* ready,std::function<void()> completion,const char* format,...){
    std::lock_guard lock(runtime::shared_locks().slot(9));char formatted[1280];va_list arguments;va_start(arguments,format);vsprintf_s(formatted,sizeof(formatted),format,arguments);va_end(arguments);
    if(font<0||font>=22)throw std::out_of_range("Centered text font outside original table");
    auto& data=sprite::current_sprite(controller,animation);RECT rectangle;std::memcpy(&rectangle,animation.base.fields_3a0+2,sizeof(rectangle));
    animation.base.flags[0]|=0x10000u;const bool outline=(animation.base.flags[1]&0x400u)==0;
    const auto advance=recovered::add32(recovered::mul32(centered_font_widths[font],2),-1);
    auto measured=_mm_cvtss_f32(_mm_div_ss(_mm_set_ss(recovered::mul32(static_cast<float>(std::strlen(formatted)),advance)),_mm_set_ss(2)));
#if defined(TH20_IOS)
    if(th20::ios::language::effective()==2)measured=std::min(float(measure_native_text(formatted,font).width),data.extent_4c-8.f);
#endif
    const int x=_mm_cvttss_si32(_mm_set_ss(recovered::add32(data.extent_4c,-measured)));
    return queue_text(rectangle,x,foreground,outline?background:0,formatted,*sprite::texture(controller,data.texture_id),font,recovered::signed_bits(std::uint32_t(spacing)<<1),outline,ready,std::move(completion));
}
}
