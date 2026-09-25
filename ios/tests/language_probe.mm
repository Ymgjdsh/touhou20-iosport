// Exercises only the isolated smoke bundle, never production preferences/saves.
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#include "ios_language.h"
#include "ios_game_input.h"
#include "ios_host.h"
#undef TRUE
#undef FALSE
#define BOOL TH20_WINBOOL
#include "program_entry/program_entry.hpp"
#include "title_system/title.hpp"
#include "hud_system/dialogue.hpp"
#include "gameplay/gameplay.hpp"
#include "player_entity/owner.hpp"
#include "game_session/session.hpp"
#include "text_renderer/native_font.hpp"
#include "archive/resource_manager.hpp"
#undef BOOL
#include <fstream>
#include <cstring>
#include <string>
namespace {
namespace lang=th20::ios::language;
namespace pe=th20::source::program_entry;
namespace title=th20::source::title;
namespace input=th20::ios::input;
int checks=0,failures=0,phase=0;double deadline=0;
void check(bool ok,const char* label){++checks;if(!ok)++failures;th20_ios_log("language-probe %s %s",ok?"PASS":"FAIL",label);}
void invoke(id target,NSString* method){[target performSelector:NSSelectorFromString(method)];}
void milestone(const char* label){
    NSString* docs=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    std::ofstream(std::string(docs.fileSystemRepresentation)+"/smoke-milestone.txt")<<label;
}
UITableViewController* settings(UIViewController* owner){return (UITableViewController*)((UINavigationController*)owner.presentedViewController).topViewController;}
UISegmentedControl* segment(UIView* view){
    if([view isKindOfClass:UISegmentedControl.class])return (UISegmentedControl*)view;
    for(UIView* child in view.subviews)if(auto* found=segment(child))return found;
    return nil;
}
bool select(UIViewController* owner,int preference){
    auto* s=settings(owner);if(!s)return false;
    auto* cell=[s.tableView.dataSource tableView:s.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:5]];
    auto* control=segment(cell);check(control&&control.enabled,"language picker is enabled in actual settings");
    if(!control)return false;control.selectedSegmentIndex=preference;[control sendActionsForControlEvents:UIControlEventValueChanged];return true;
}
bool titleReady(){auto* t=title::controller();return pe::graphics_state.field_0b08==4&&t&&t->phase==2&&t->state==1&&t->age.current>=45;}
void catalog(){
    using namespace th20::source;
    check(lang::resolve(0,"zh-Hans-CN")==2&&lang::resolve(0,"zh-Hant-TW")==2,"Chinese system language selects Simplified Chinese");
    check(lang::resolve(0,"ja-JP")==1&&lang::resolve(0,"en-US")==1,"Japanese and other system languages select Japanese");
    check(lang::resolve(1,"zh-Hans")==1&&lang::resolve(2,"ja")==2,"explicit language overrides system");
    check(lang::preference()==0&&lang::effective()==2,"fresh launch detects real Chinese preferred language");
    NSString* path=[NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"Translations/zh-Hans/translations.json"];
    NSDictionary* manifest=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:path] options:0 error:nil];
    unsigned strings=0;
    for(NSString* token in manifest[@"tokens"]){
        CFStringRef value=lang::copy_text(token.UTF8String);
        if(!value||![(__bridge NSString*)value isEqualToString:manifest[@"tokens"][token]])++failures;
        if(value)CFRelease(value);
        text::measure_native_text(token.UTF8String,4);++strings;
    }
    check(strings>3000,"all translated strings decode and shape with real native glyphs");
    unsigned staticFailures=0;
    for(NSString* original in manifest[@"direct"]){
        NSData* bytes=[original dataUsingEncoding:CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingDOSJapanese)];
        std::string source(static_cast<const char*>(bytes.bytes),bytes.length);
        CFStringRef value=lang::copy_text(source.c_str());
        if(!value||![(__bridge NSString*)value isEqualToString:manifest[@"direct"][original]])++staticFailures;
        if(value)CFRelease(value);
        text::measure_native_text(source.c_str(),4);
    }
    check(staticFailures==0,"all static Japanese labels resolve to supplied Chinese translations");
    NSRegularExpression* spec=[NSRegularExpression regularExpressionWithPattern:@"%[-+ #0]*[0-9]*(?:\\.[0-9]+)?(?:ll|l|h|z)?[diuxXfs]" options:0 error:nil];
    for(NSDictionary* item in manifest[@"formats"]){
        NSString* original=item[@"source"], *expected=item[@"target"];
        original=[spec stringByReplacingMatchesInString:original options:0 range:NSMakeRange(0,original.length) withTemplate:@"42"];
        expected=[spec stringByReplacingMatchesInString:expected options:0 range:NSMakeRange(0,expected.length) withTemplate:@"42"];
        NSData* bytes=[original dataUsingEncoding:CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingDOSJapanese)];
        std::string source(static_cast<const char*>(bytes.bytes),bytes.length);
        CFStringRef value=lang::copy_text(source.c_str());
        if(value)th20_ios_log("language-probe format actual=%s expected=%s",[(__bridge NSString*)value UTF8String],expected.UTF8String);
        check(value&&[(__bridge NSString*)value isEqualToString:expected],"formatted translation preserves live values");
        if(value)CFRelease(value);
    }
    auto data=resources::read("st01r0.msg",false);check(data&&data->size()>20,"translated dialogue is loaded through real resource manager");
    check(bool(lang::resource("help_01.png"))&&bool(lang::resource("title.anm")),"translated help and title textures are available");
    const auto spell=lang::spell(0,"original");CFStringRef name=lang::copy_text(spell.c_str());
    check(name&&CFStringFind(name,CFSTR("尘符"),0).location!=kCFNotFound,"spell name resolves to Chinese without changing save record");if(name)CFRelease(name);
    th20_ios_log("language-probe shaped %u translated strings",strings);
}
}
extern "C" int th20_ios_language_probe(){
    using namespace th20::source;
    UIViewController* owner=UIApplication.sharedApplication.keyWindow.rootViewController;
    const double now=CACurrentMediaTime();if(now<deadline)return 0;
    if(phase==0){if(!titleReady())return 0;catalog();milestone("language-title-zh");deadline=now+5;phase=1;return 0;}
    if(phase==1){invoke(owner,@"openSettings");deadline=now+1;phase=2;return 0;}
    if(phase==2){auto* s=settings(owner);[s.tableView scrollToRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:5] atScrollPosition:UITableViewScrollPositionBottom animated:NO];milestone("language-settings");deadline=now+4;phase=3;return 0;}
    if(phase==3){select(owner,1);phase=4;deadline=now+4;return 0;}
    if(phase==4){if(!titleReady())return 0;check(lang::effective()==1&&lang::preference()==1,"actual settings switch reloads Japanese and persists preference");check(!lang::resource("title.anm")&&!lang::copy_text("~ZH000000~"),"Japanese mode bypasses all translated resources and strings");milestone("language-title-ja");phase=5;deadline=now+5;return 0;}
    if(phase==5){invoke(owner,@"openSettings");phase=6;deadline=now+1;return 0;}
    if(phase==6){select(owner,2);phase=7;deadline=now+4;return 0;}
    if(phase==7){if(!titleReady())return 0;check(lang::effective()==2&&lang::preference()==2,"Japanese to Chinese switch reloads title successfully");input::touch(TH20_IOS_TOUCH_MENU_TAP,1,186.f*pe::window_state.scale,378.f*pe::window_state.scale,0,0);phase=8;deadline=now+4;return 0;}
    if(phase==8){auto* t=title::controller();if(!t||t->state!=14||t->phase!=2||t->age.current<90)return 0;check(true,"Chinese music room opens through normal input");milestone("language-music-zh");phase=9;deadline=now+5;return 0;}
    if(phase==9){input::key(0x58,true);phase=10;deadline=now+.2;return 0;}
    if(phase==10){input::key(0x58,false);if(!titleReady())return 0;input::touch(TH20_IOS_TOUCH_MENU_TAP,1,186.f*pe::window_state.scale,240.f*pe::window_state.scale,0,0);phase=11;deadline=now+2;return 0;}
    if(phase==11){
        if(pe::graphics_state.field_0b08==4){auto* t=title::controller();if(t&&t->phase==2&&t->age.current>20&&(t->state==5||t->state==6||t->state==7)){input::key(0x5a,true);deadline=now+.2;phase=12;}return 0;}
        if(pe::graphics_state.field_0b08!=7||!gameplay::controller||!hud::controller||!hud::controller->message_data)return 0;
        if(gameplay::controller->frame_timer.current<180)return 0;
        auto* player=static_cast<player_entity::Player*>(game_session::context(0).objects_04[0]);
        if(!player||player->state!=1)return 0;
        if(!hud::controller->collecting)hud::start_dialogue(*hud::controller,0);phase=13;deadline=now+10;return 0;
    }
    if(phase==12){input::key(0x5a,false);phase=11;deadline=now+2;return 0;}
    if(phase==13){auto* d=hud::controller?hud::controller->collecting:nullptr;if(!d||d->field_134<=0||d->timers[0].current<100)return 0;check(true,"Chinese dialogue has visible text in a loaded live stage");milestone("language-dialogue-zh");phase=14;deadline=now+5;return 0;}
    if(phase==14){invoke(owner,@"openSettings");phase=15;deadline=now+1;return 0;}
    if(phase==15){select(owner,1);phase=16;deadline=now+1;return 0;}
    if(phase==16){
        auto* s=settings(owner);UIAlertController* alert=(UIAlertController*)s.presentedViewController;
        check([alert isKindOfClass:UIAlertController.class]&&lang::effective()==2,"battle language change asks before ending current run");
        // Dismissing the confirmation cancels; no hidden auto-accept in test.
        [alert dismissViewControllerAnimated:NO completion:nil];invoke(s,@"done");
        phase=17;deadline=now+2;return 0;
    }
    check(lang::effective()==2&&pe::graphics_state.field_0b08==7,"cancelled language switch retains battle and Chinese setting");
    NSString* docs=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    std::ofstream(std::string(docs.fileSystemRepresentation)+"/language-probe.json")<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
    th20_ios_log("language-probe RESULT %d/%d",checks-failures,checks);return failures?-1:1;
}
