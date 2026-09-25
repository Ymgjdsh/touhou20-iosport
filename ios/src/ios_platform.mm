#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
#undef TRUE
#undef FALSE
#define BOOL TH20_WINBOOL
#include <Windows.h>
#include <mmsystem.h>
#undef BOOL
#include "ios_platform.h"
#include "ios_host.h"
#include <chrono>
#include <thread>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

namespace {
std::string resources,documents;
NSStringEncoding encoding(UINT codepage) {
    if(codepage==932)return CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingDOSJapanese);
    if(codepage==65001)return NSUTF8StringEncoding;
    return 0;
}
NSString* decode(const char* value,UINT codepage,int length) {
    const auto codec=encoding(codepage);
    if(!value||!codec||length==0||length < -1)return nil;
    return [[NSString alloc] initWithBytes:value length:length<0?std::strlen(value):std::size_t(length) encoding:codec];
}
}
extern "C" void th20_ios_configure_paths(const char* resource_path,const char* document_path) {
    if(!resource_path||!document_path||!*resource_path||!*document_path)
        throw std::invalid_argument("Native sandbox paths are missing");
    resources=resource_path;documents=document_path;
}
extern "C" const char* th20_ios_resource_directory(){return resources.c_str();}
extern "C" const char* th20_ios_documents_directory(){return documents.c_str();}
extern "C" uint64_t th20_ios_monotonic_microseconds(){
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
extern "C" void th20_ios_log_cp932(const char* text){@autoreleasepool {
    NSString* decoded=decode(text,932,-1);
    if(decoded)th20_ios_log("engine: %s",decoded.UTF8String);
    else if(text){
        // Preserve undecodable bytes for diagnosis instead of dropping the log.
        NSMutableString* hex=[NSMutableString stringWithString:@"engine invalid CP932 bytes:"];
        for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text);*p;++p)[hex appendFormat:@" %02x",*p];
        th20_ios_log("%s",hex.UTF8String);
    }
}}
extern "C" void th20_ios_report_cp932_error(const char* text){@autoreleasepool {
    NSString* decoded=decode(text,932,-1);
    th20_ios_set_error(decoded?decoded.UTF8String:"Engine error text has invalid CP932 encoding; see diagnostic log.");
}}
int MultiByteToWideChar(UINT codepage,DWORD,const char* value,int length,char16_t* output,int capacity){@autoreleasepool {
    NSString* text=decode(value,codepage,length);if(!text||capacity<0)return 0;
    const auto needed=text.length+(length<0?1:0);if(needed>INT_MAX)return 0;
    if(!output||!capacity)return int(needed);if(std::size_t(capacity)<needed)return 0;
    [text getCharacters:reinterpret_cast<unichar*>(output) range:NSMakeRange(0,text.length)];
    if(length<0)output[text.length]=0;return int(needed);
}}
int MultiByteToWideChar(UINT codepage,DWORD,const char* value,int length,wchar_t* output,int capacity){@autoreleasepool {
    NSString* text=decode(value,codepage,length);if(!text||capacity<0)return 0;
    NSData* data=[text dataUsingEncoding:NSUTF32LittleEndianStringEncoding];
    const auto count=data.length/sizeof(wchar_t),needed=count+(length<0?1:0);if(needed>INT_MAX)return 0;
    if(!output||!capacity)return int(needed);if(std::size_t(capacity)<needed)return 0;
    std::memcpy(output,data.bytes,data.length);if(length<0)output[count]=0;return int(needed);
}}
int WideCharToMultiByte(UINT codepage,DWORD,const wchar_t* value,int length,char* output,int capacity,const char*,TH20_WINBOOL* fallback){@autoreleasepool {
    if(fallback)*fallback=0;if(!value||length==0||length < -1||capacity<0)return 0;
    const auto codec=encoding(codepage);if(!codec)return 0;
    const auto count=length<0?std::wcslen(value):std::size_t(length);
    NSString* text=[[NSString alloc] initWithBytes:value length:count*sizeof(wchar_t) encoding:NSUTF32LittleEndianStringEncoding];
    NSData* bytes=[text dataUsingEncoding:codec allowLossyConversion:NO];if(!bytes)return 0;
    const auto needed=bytes.length+(length<0?1:0);if(needed>INT_MAX)return 0;
    if(!output||!capacity)return int(needed);if(std::size_t(capacity)<needed)return 0;
    std::memcpy(output,bytes.bytes,bytes.length);if(length<0)output[bytes.length]=0;return int(needed);
}}
void Sleep(DWORD milliseconds){std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));}
extern "C" DWORD WINAPI timeGetTime(){return DWORD(th20_ios_monotonic_microseconds()/1000);}
// iOS clock precision is a platform property; these do not change a global timer.
extern "C" MMRESULT WINAPI timeBeginPeriod(UINT){return 0;}
extern "C" MMRESULT WINAPI timeEndPeriod(UINT){return 0;}

extern "C" bool th20_ios_is_japanese_locale(void){return [[NSLocale preferredLanguages].firstObject hasPrefix:@"ja"];}
extern "C" bool th20_ios_save_bgra_png(const char* path,const void* pixels,int width,int height,int pitch){@autoreleasepool {
    if(!path||!pixels||width<=0||height<=0||pitch<width*4)return false;
    CGColorSpaceRef color=CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider=CGDataProviderCreateWithData(nullptr,pixels,std::size_t(pitch)*height,nullptr);
    CGImageRef image=CGImageCreate(width,height,8,32,pitch,color,kCGBitmapByteOrder32Little|kCGImageAlphaNoneSkipFirst,provider,nullptr,false,kCGRenderingIntentDefault);
    CGColorSpaceRelease(color);CGDataProviderRelease(provider);if(!image)return false;
    NSURL* url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
    CGImageDestinationRef destination=CGImageDestinationCreateWithURL((__bridge CFURLRef)url,CFSTR("public.png"),1,nullptr);
    bool saved=false;
    if(destination){CGImageDestinationAddImage(destination,image,nullptr);saved=CGImageDestinationFinalize(destination);CFRelease(destination);}
    CGImageRelease(image);return saved;
}}
