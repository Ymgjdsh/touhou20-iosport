// Actual native CoreText shaping and CoreGraphics rasterization. CP932 text is
// decoded strictly; no placeholder glyphs or raster dimensions are fabricated.
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include "native_font.hpp"
#include "ios_host.h"
#include "ios_language.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace th20::source::text {
namespace {
std::recursive_mutex font_mutex;
std::array<CTFontRef,22> fonts{};
constexpr int heights[22]={24,24,24,28,32,36,40,44,48,60,64,32,32,32,40,48,60,64,15,15,15,15};
template<class T> struct CFHandle {
    T value{};
    explicit CFHandle(T v=nullptr):value(v){}
    ~CFHandle(){if(value)CFRelease(value);}
    CFHandle(const CFHandle&)=delete;CFHandle& operator=(const CFHandle&)=delete;
    operator T()const{return value;}
};
CTFontRef font_at(int font){
    if(font<0||font>=22)throw std::out_of_range("Native font index outside 22-slot table");
    if(!fonts[font])initialize_native_fonts();
    if(!fonts[font])throw std::runtime_error("Native Japanese font unavailable");
    return fonts[font];
}
CFStringRef decode(const char* source){
    if(!source)throw std::invalid_argument("Null native text string");
    if(auto translated=th20::ios::language::copy_text(source))return translated;
    const auto length=std::strlen(source);
    if(length>std::size_t(std::numeric_limits<CFIndex>::max()))throw std::length_error("Native text exceeds CFString capacity");
    auto* string=CFStringCreateWithBytes(kCFAllocatorDefault,reinterpret_cast<const UInt8*>(source),length,
        kCFStringEncodingDOSJapanese,false);
    if(!string){th20_ios_log("text: invalid CP932 sequence (%zu bytes)",length);throw std::runtime_error("Invalid CP932 text resource");}
    return string;
}
CTLineRef line(CFStringRef string,CTFontRef font){
    const void* keys[]={kCTFontAttributeName,kCTForegroundColorFromContextAttributeName};
    const void* values[]={font,kCFBooleanTrue};
    CFHandle<CFDictionaryRef> attributes(CFDictionaryCreate(kCFAllocatorDefault,keys,values,2,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks));
    CFHandle<CFAttributedStringRef> attributed(CFAttributedStringCreate(kCFAllocatorDefault,string,attributes));
    auto result=CTLineCreateWithAttributedString(attributed);
    if(!result)throw std::runtime_error("CoreText line creation failed");
    // CoreText performs real language fallback. A missing non-control glyph
    // is an error: rendering a box would hide a missing resource/font issue.
    const auto runs=CTLineGetGlyphRuns(result);
    for(CFIndex index=0;index<CFArrayGetCount(runs);++index){
        auto run=static_cast<CTRunRef>(CFArrayGetValueAtIndex(runs,index));const CFIndex count=CTRunGetGlyphCount(run);
        std::vector<CGGlyph> glyphs(count);std::vector<CFIndex> positions(count);
        CTRunGetGlyphs(run,CFRangeMake(0,0),glyphs.data());CTRunGetStringIndices(run,CFRangeMake(0,0),positions.data());
        for(CFIndex n=0;n<count;++n){
            if(glyphs[n])continue;const UniChar ch=CFStringGetCharacterAtIndex(string,positions[n]);
            if(ch<=0x20||ch==0x7f||ch==0x200b||ch==0x200c||ch==0x200d||ch==0xfeff)continue;
            th20_ios_log("text: missing glyph U+%04x",unsigned(ch));CFRelease(result);throw std::runtime_error("CoreText cannot render a required glyph");
        }
    }
    return result;
}
NativeTextExtent extent(CTLineRef text,CTFontRef font){
    const auto width=std::ceil(CTLineGetTypographicBounds(text,nullptr,nullptr,nullptr));
    return {int(std::max(0.0,width)),int(std::ceil(CTFontGetSize(font)))};
}
// Native text is measured when an atlas slot is allocated and then rasterized
// into that slot. Retain the shaped line across these two operations; fixed
// spacing also reuses its composed-character lines instead of shaping each
// character again whenever a dialogue or menu is revisited.
struct ShapedText {
    CFHandle<CFStringRef> string;
    CFHandle<CTLineRef> text;
    NativeTextExtent measured;
    std::vector<CTLineRef> characters;
    std::uint64_t used{};
    ShapedText(const char* bytes,CTFontRef font):string(decode(bytes)),text(line(string,font)),measured(extent(text,font)){}
    ~ShapedText(){for(auto character:characters)CFRelease(character);}
    void prepare_characters(CTFontRef font){
        if(!characters.empty()||!CFStringGetLength(string))return;
        CFIndex cursor=0;
        try {
            while(cursor<CFStringGetLength(string)){
                const auto range=CFStringGetRangeOfComposedCharactersAtIndex(string,cursor);
                CFHandle<CFStringRef> character(CFStringCreateWithSubstring(kCFAllocatorDefault,string,range));
                characters.push_back(line(character,font));cursor=range.location+range.length;
            }
        }catch(...){for(auto character:characters)CFRelease(character);characters.clear();throw;}
    }
};
using ShapeKey=std::pair<int,std::string>;
std::map<ShapeKey,std::shared_ptr<ShapedText>> shaped_text;
using BitmapKey=std::tuple<int,int,int,int,std::uint32_t,std::uint32_t,bool,std::uint32_t,int,int,std::string>;
struct CachedBitmap {std::vector<std::uint8_t> pixels;NativeTextExtent measured;std::uint64_t used;};
std::map<BitmapKey,CachedBitmap> bitmaps;
constexpr std::size_t bitmap_budget=8*1024*1024,shape_limit=256,bitmap_limit=128;
std::uint64_t use_counter{},shape_hits{},shape_misses{},bitmap_hits{},bitmap_misses{};
std::size_t bitmap_bytes{};
std::shared_ptr<ShapedText> shape(const char* bytes,int index,CTFontRef font){
    if(!bytes)throw std::invalid_argument("Null native text string");
    ShapeKey key{index,bytes};
    const auto found=shaped_text.find(key);
    if(found!=shaped_text.end()){++shape_hits;found->second->used=++use_counter;return found->second;}
    ++shape_misses;auto result=std::make_shared<ShapedText>(bytes,font);result->used=++use_counter;
    if(shaped_text.size()>=shape_limit){
        const auto oldest=std::min_element(shaped_text.begin(),shaped_text.end(),[](const auto& a,const auto& b){return a.second->used<b.second->used;});
        shaped_text.erase(oldest);
    }
    shaped_text.emplace(std::move(key),result);return result;
}
void copy_bitmap(std::uint8_t* destination,int pitch,int width,int height,const std::vector<std::uint8_t>& source){
    for(int row=0;row<height;++row)std::memcpy(destination+std::size_t(row)*pitch,source.data()+std::size_t(row)*width*4,std::size_t(width)*4);
}
void cache_bitmap(BitmapKey key,std::vector<std::uint8_t> pixels,NativeTextExtent measured){
    if(pixels.size()>bitmap_budget)return;
    while(!bitmaps.empty()&&(bitmap_bytes+pixels.size()>bitmap_budget||bitmaps.size()>=bitmap_limit)){
        const auto oldest=std::min_element(bitmaps.begin(),bitmaps.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
        bitmap_bytes-=oldest->second.pixels.size();bitmaps.erase(oldest);
    }
    bitmap_bytes+=pixels.size();bitmaps.emplace(std::move(key),CachedBitmap{std::move(pixels),measured,++use_counter});
}
void color(CGContextRef context,std::uint32_t value,bool stroke){
    const CGFloat r=((value>>16)&255)/255.0,g=((value>>8)&255)/255.0,b=(value&255)/255.0;
    if(stroke)CGContextSetRGBStrokeColor(context,r,g,b,1);else CGContextSetRGBFillColor(context,r,g,b,1);
}
}
void initialize_native_fonts(){
    std::lock_guard lock(font_mutex);
    for(unsigned index=0;index<fonts.size();++index){
        if(fonts[index])continue;
        const bool mincho=(index>=13&&index<=17)||index>=20;
        const auto requested=th20::ios::language::effective()==2?
            (mincho?CFSTR("SongtiSC-Bold"):CFSTR("PingFangSC-Regular")):
            (mincho?CFSTR("HiraMinProN-W6"):CFSTR("HiraginoSans-W3"));
        auto font=CTFontCreateWithName(requested,heights[index],nullptr);
        if(!font)throw std::runtime_error("Unable to load native Japanese typeface");
        if(index==11){if(auto bold=CTFontCreateCopyWithSymbolicTraits(font,0,nullptr,kCTFontBoldTrait,kCTFontBoldTrait)){CFRelease(font);font=bold;}}
        CFHandle<CFStringRef> actual(CTFontCopyPostScriptName(font));char name[256]{};
        if(actual)CFStringGetCString(actual,name,sizeof name,kCFStringEncodingUTF8);
        th20_ios_log("text: native font slot=%u size=%d actual=%s",index,heights[index],name);
        fonts[index]=font;
    }
}
void release_native_fonts(){
    std::lock_guard lock(font_mutex);
    th20_ios_log("text: cache shape_hits=%llu shape_misses=%llu raster_hits=%llu raster_misses=%llu bytes=%zu",
        static_cast<unsigned long long>(shape_hits),static_cast<unsigned long long>(shape_misses),
        static_cast<unsigned long long>(bitmap_hits),static_cast<unsigned long long>(bitmap_misses),bitmap_bytes);
    bitmaps.clear();shaped_text.clear();bitmap_bytes=0;use_counter=shape_hits=shape_misses=bitmap_hits=bitmap_misses=0;
    for(auto& font:fonts){if(font)CFRelease(font);font=nullptr;}
}
NativeTextExtent measure_native_text(const char* cp932,int index){
    std::lock_guard lock(font_mutex);const auto font=font_at(index);auto measured=shape(cp932,index,font)->measured;
    if(th20::ios::language::effective()==2)measured.width=std::min(measured.width,1000);
    return measured;
}
NativeTextExtent raster_native_text(std::uint8_t* output,int pitch,int width,int height,const char* cp932,int index,int spacing,
    std::uint32_t foreground,std::uint32_t background,bool outline,float radius,int x,int top){
    std::lock_guard lock(font_mutex);
    if(!output||width<=0||height<=0||pitch<std::int64_t(width)*4)throw std::invalid_argument("Invalid native text bitmap");
    const auto font=font_at(index);if(!cp932)throw std::invalid_argument("Null native text string");
    std::uint32_t radius_bits;std::memcpy(&radius_bits,&radius,sizeof radius_bits);
    BitmapKey key{index,width,height,spacing,foreground,background,outline,radius_bits,x,top,cp932};
    const auto cached=bitmaps.find(key);
    if(cached!=bitmaps.end()){
        ++bitmap_hits;cached->second.used=++use_counter;copy_bitmap(output,pitch,width,height,cached->second.pixels);return cached->second.measured;
    }
    ++bitmap_misses;const auto started=std::chrono::steady_clock::now();const auto shaped=shape(cp932,index,font);
    std::vector<std::uint8_t> rgba(std::size_t(width)*height*4);
    CFHandle<CGColorSpaceRef> colors(CGColorSpaceCreateDeviceRGB());
    CFHandle<CGContextRef> context(CGBitmapContextCreate(rgba.data(),width,height,8,width*4,colors,CGBitmapInfo(kCGImageAlphaPremultipliedLast)|kCGBitmapByteOrder32Big));
    if(!context)throw std::runtime_error("CoreGraphics text bitmap allocation failed");
    CGContextSetTextMatrix(context,CGAffineTransformIdentity);CGContextSetShouldAntialias(context,true);CGContextSetAllowsFontSmoothing(context,false);
    CGContextSetLineJoin(context,kCGLineJoinRound);CGContextSetLineWidth(context,std::max(0.f,radius)*2);
    color(context,foreground,false);color(context,background,true);
    const CGFloat baseline=height-top-CTFontGetAscent(font);
    const bool translated=th20::ios::language::effective()==2;
    const CGFloat available=std::max(1,std::min(width-8,1000)-std::max(0,x));
    const CGFloat horizontalScale=translated&&!spacing&&shaped->measured.width>available?available/shaped->measured.width:1;
    if(horizontalScale<1)CGContextScaleCTM(context,horizontalScale,1);
    const auto draw=[&](CTLineRef item,CGFloat offset){
        offset/=horizontalScale;
        if(outline){CGContextSetTextDrawingMode(context,kCGTextStroke);CGContextSetTextPosition(context,offset,baseline);CTLineDraw(item,context);}
        CGContextSetTextDrawingMode(context,kCGTextFill);CGContextSetTextPosition(context,offset,baseline);CTLineDraw(item,context);
    };
    auto measured=shaped->measured;
    if(spacing){
        shaped->prepare_characters(font);CGFloat at=x+2;
        for(auto glyph:shaped->characters){draw(glyph,at);at+=spacing;}
        measured.width=std::max(0,int(shaped->characters.size())*spacing);
    }else{draw(shaped->text,x+2);measured.width=int(std::ceil(measured.width*horizontalScale));}
    CFRelease(context.value);context.value=nullptr;
    // Quartz uses an upward drawing coordinate system, but CGBitmapContext's
    // backing rows already run from the image's top to bottom. The baseline
    // above accounts for that coordinate system. Reversing the memory rows a
    // second time flips only generated text (ANM textures remain upright).
    // Convert RGBA premultiplied alpha to the atlas's BGRA straight alpha.
    for(int row=0;row<height;++row)for(int column=0;column<width;++column){
        auto* pixel=rgba.data()+(std::size_t(row)*width+column)*4;const unsigned a=pixel[3],red=pixel[0],green=pixel[1],blue=pixel[2];
        pixel[0]=a?std::min(255u,(blue*255+a/2)/a):0;pixel[1]=a?std::min(255u,(green*255+a/2)/a):0;
        pixel[2]=a?std::min(255u,(red*255+a/2)/a):0;pixel[3]=a;
    }
    copy_bitmap(output,pitch,width,height,rgba);cache_bitmap(std::move(key),std::move(rgba),measured);
    const auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    if(milliseconds>8)th20_ios_log("text: slow raster font=%d size=%dx%d bytes=%zu cpu_ms=%.2f",index,width,height,std::strlen(cp932),milliseconds);
    return measured;
}
}
