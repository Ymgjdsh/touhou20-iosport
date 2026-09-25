#import <Foundation/Foundation.h>
#include "ios_language.h"
#include "ios_host.h"
#include <atomic>
#include <cstring>
#include <stdexcept>

namespace th20::ios::language {
namespace {
NSDictionary *pack;
NSString *root;
NSArray *formats;
std::atomic<int> active{1};
NSString* japanese(const char* bytes,std::size_t count){
    return [[NSString alloc] initWithBytes:bytes length:count encoding:CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingDOSJapanese)];
}
}
int resolve(int preference,std::string_view preferred){
    if(preference==1||preference==2)return preference;
    return preferred=="zh"||preferred.substr(0,3)=="zh-"||preferred.substr(0,3)=="zh_"?2:1;
}
int preference(){return int([NSUserDefaults.standardUserDefaults integerForKey:@"gameLanguage"]);}
int effective(){return active.load();}
bool available(){return pack!=nil;}
void select(int requested){
    if(requested<0||requested>2)throw std::invalid_argument("Unknown game language");
    NSString* preferred=NSLocale.preferredLanguages.firstObject?:@"ja";
    const int selected=resolve(requested,preferred.UTF8String);
    if(selected==2&&!available())throw std::runtime_error("Chinese translation pack is missing");
    active.store(selected);
    [NSUserDefaults.standardUserDefaults setInteger:requested forKey:@"gameLanguage"];
    th20_ios_log("language preference=%d effective=%s",requested,selected==2?"zh-Hans":"ja");
}
void initialize(const char* resources){@autoreleasepool {
    root=[[NSString stringWithUTF8String:resources] stringByAppendingPathComponent:@"Translations/zh-Hans"];
    NSData* bytes=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"translations.json"]];
    if(bytes){
        NSError* error=nil;id parsed=[NSJSONSerialization JSONObjectWithData:bytes options:0 error:&error];
        if(![parsed isKindOfClass:NSDictionary.class]||[parsed[@"version"] intValue]!=1||
           ![parsed[@"tokens"] isKindOfClass:NSDictionary.class]||![parsed[@"files"] isKindOfClass:NSDictionary.class])
            throw std::runtime_error("Invalid native translation manifest");
        pack=parsed;
        NSMutableArray* compiled=[NSMutableArray new];
        for(NSDictionary* item in pack[@"formats"]){
            NSRegularExpression* regex=[NSRegularExpression regularExpressionWithPattern:item[@"pattern"] options:0 error:&error];
            if(!regex||![item[@"pieces"] isKindOfClass:NSArray.class])throw std::runtime_error("Invalid translated format");
            [compiled addObject:@{@"regex":regex,@"pieces":item[@"pieces"]}];
        }
        formats=compiled;
        for(NSString* name in pack[@"files"]){
            if(![name isEqualToString:name.lastPathComponent]||![NSFileManager.defaultManager fileExistsAtPath:[root stringByAppendingPathComponent:name]])
                throw std::runtime_error("Incomplete native translation pack");
        }
    }
    if(available())select(preference());
    else{active.store(1);th20_ios_log("language: no optional translation pack; using Japanese");}
}}
std::optional<std::vector<std::uint8_t>> resource(std::string_view name){@autoreleasepool {
    if(effective()!=2||!pack)return std::nullopt;
    auto at=name.find_last_of("/\\");if(at!=std::string_view::npos)name.remove_prefix(at+1);
    NSString* key=[[NSString alloc] initWithBytes:name.data() length:name.size() encoding:NSUTF8StringEncoding];
    if(!key||!pack[@"files"][key])return std::nullopt;
    NSData* data=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:key]];
    if(!data)throw std::runtime_error("Cannot read translated game resource");
    auto* begin=static_cast<const std::uint8_t*>(data.bytes);
    return std::vector<std::uint8_t>(begin,begin+data.length);
}}
CFStringRef copy_text(const char* cp932){@autoreleasepool {
    if(effective()!=2||!pack||!cp932)return nullptr;
    // Short ASCII references fit all original fixed byte buffers; the Chinese
    // Unicode content is resolved only at the native text-rendering boundary.
    std::string_view input(cp932);NSMutableString* text=[NSMutableString new];bool changed=false;
    while(!input.empty()){
        const auto pos=input.find("~ZH");
        if(pos==std::string_view::npos){NSString* tail=japanese(input.data(),input.size());if(!tail)return nullptr;[text appendString:tail];break;}
        NSString* prefix=japanese(input.data(),pos);if(!prefix)return nullptr;[text appendString:prefix];input.remove_prefix(pos);
        if(input.size()<10||input[9]!='~')return nullptr;
        NSString* key=[[NSString alloc] initWithBytes:input.data() length:10 encoding:NSASCIIStringEncoding];
        NSString* value=pack[@"tokens"][key];if(!value)throw std::runtime_error("Unresolved translated string reference");
        [text appendString:value];input.remove_prefix(10);changed=true;
    }
    if(NSString* replacement=pack[@"direct"][text]){text=[replacement mutableCopy];changed=true;}
    else for(NSDictionary* item in formats){
        NSRegularExpression* regex=item[@"regex"];
        NSTextCheckingResult* match=[regex firstMatchInString:text options:0 range:NSMakeRange(0,text.length)];
        if(!match)continue;
        NSMutableString* result=[NSMutableString new];
        for(id piece in item[@"pieces"]){
            if([piece isKindOfClass:NSString.class])[result appendString:piece];
            else{NSUInteger group=[piece unsignedIntegerValue]+1;if(group>=match.numberOfRanges)throw std::runtime_error("Translated format capture out of bounds");[result appendString:[text substringWithRange:[match rangeAtIndex:group]]];}
        }
        text=result;changed=true;break;
    }
    if(!changed)return nullptr;
    return (__bridge_retained CFStringRef)text;
}}
std::string spell(int id,const char* original){@autoreleasepool {
    if(effective()==2&&pack){NSString* token=pack[@"spells"][[NSString stringWithFormat:@"%d",id]];if(token)return token.UTF8String;}
    return original?original:"";
}}
}
