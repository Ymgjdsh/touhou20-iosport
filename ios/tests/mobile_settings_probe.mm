// Test-only driver: exercises the real settings actions in the isolated smoke
// bundle, including a saved layout and the user's content-unlock code.
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#undef TRUE
#undef FALSE
#define BOOL TH20_WINBOOL
#include "progress_state/manager.hpp"
#include "title_system/stage_select.hpp"
#undef BOOL
#include "ios_host.h"
#include "ios_unlock.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

@interface TH20TestPan : UIPanGestureRecognizer
@property(nonatomic) CGPoint testDelta;
@end
@implementation TH20TestPan
- (CGPoint)translationInView:(UIView *)view { return self.testDelta; }
- (void)setTranslation:(CGPoint)translation inView:(UIView *)view { self.testDelta = translation; }
@end
namespace {
int phase=0,checks=0,failures=0;
double deadline=0;
void check(bool okay,const char* name){++checks;if(!okay)++failures;th20_ios_log("mobile-probe %s %s",okay?"PASS":"FAIL",name);}
void milestone(const char* name){
    NSString* docs=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    std::ofstream(std::string(docs.fileSystemRepresentation)+"/smoke-milestone.txt")<<name;
}
UIView* find(UIView* root,Class kind,NSInteger tag){
    if([root isKindOfClass:kind]&&root.tag==tag)return root;
    for(UIView* child in root.subviews)if(auto* found=find(child,kind,tag))return found;
    return nil;
}
void invoke(id target,NSString* name){[target performSelector:NSSelectorFromString(name)];}
void validate_unlock(){
    namespace p=th20::source::progress;
    auto* manager=p::manager;check(manager!=nullptr,"progress manager available");if(!manager)return;
    std::vector<unsigned char> before(sizeof(manager->current));std::memcpy(before.data(),&manager->current,before.size());
    check(th20::ios::apply_unlock_code("incorrect-code")==0&&std::memcmp(before.data(),&manager->current,before.size())==0,"incorrect code leaves progress unchanged");
    bool okay=true;
    for(int character=0;character<2;++character)for(int stone=0;stone<9;++stone){
        okay&=manager->extra_unlocked(character,stone);
        auto& profile=*p::find_profile(manager->current,character,stone);
        for(int difficulty=0;difficulty<4;++difficulty)for(int stage=0;stage<6;++stage)
            okay&=th20::source::title::stage_available(profile,difficulty,stage);
    }
    check(okay,"all characters, Extra loadouts and stage practice available");
    okay=true;for(unsigned card=0;card<113;++card){const auto* r=manager->current.profiles[18].bytes+0xb08+card*0xe0;okay&=p::read<unsigned>(r,0xc8)!=0||p::read<unsigned>(r,0xcc)!=0;}
    check(okay,"113 spell-practice records available");
    okay=true;for(int track=0;track<32;++track)okay&=manager->current.metadata.bytes[0x36+track]!=0;
    check(okay,"all Music Room flags available");
    okay=true;for(unsigned stone=0;stone<9;++stone)okay&=manager->stone_count(stone)==9;
    check(okay,"all stone inventory available");
    okay=true;for(unsigned record=0;record<41;++record)okay&=manager->current.metadata.bytes[0x68+record]!=0;
    check(okay,"all 41 stone-gallery titles and descriptions available");
    okay=true;for(unsigned ending=0;ending<22;++ending)okay&=manager->current.metadata.bytes[0x16+ending]!=0;
    check(okay,"all ending content flags available");
    check(manager->current.metadata.bytes[0x1d1]==p::metadata_checksum(manager->current.metadata),"saved progress checksum valid");
}
}
extern "C" int th20_ios_mobile_settings_probe(){
    if(phase==9)return failures?-1:1;
    const double now=CACurrentMediaTime();if(now<deadline)return 0;
    UIViewController* owner=UIApplication.sharedApplication.keyWindow.rootViewController;
    auto* navigation=(UINavigationController*)owner.presentedViewController;
    UITableViewController* settings=[navigation isKindOfClass:UINavigationController.class]?(UITableViewController*)navigation.topViewController:nil;
    if(phase==0){
        bool okay=true;for(int tag=0;tag<3;++tag){auto* b=(UIButton*)find(owner.view,UIButton.class,tag);okay&=b&&b.layer.cornerRadius>=b.bounds.size.width*.49;}
        check(okay,"ZSX controls are circular overlays");
        auto* z=(UIButton*)find(owner.view,UIButton.class,0);auto* s=(UIButton*)find(owner.view,UIButton.class,1);auto* x=(UIButton*)find(owner.view,UIButton.class,2);
        check([z.currentTitle isEqualToString:@"Z"]&&[s.currentTitle isEqualToString:@"S"]&&[x.currentTitle isEqualToString:@"X"],"ZSX labels match reference");
        th20_ios_open_settings();deadline=now+3;phase=1;return 0;
    }
    if(phase==1){
        check([settings isKindOfClass:UITableViewController.class],"settings uses grouped scrolling table");if(!settings){phase=9;return -1;}
        check([settings.tableView numberOfSections]==6,"control, layout, graphics, gesture, cheat and language sections present");
        check([settings.tableView numberOfRowsInSection:0]==9 &&
              [settings.tableView numberOfRowsInSection:2]==6,
              "drag shooting, hitbox and battle zoom settings present");
        auto* modes=(UISegmentedControl*)find(settings.view,UISegmentedControl.class,0);
        check(modes.numberOfSegments==3&&[[modes titleForSegmentAtIndex:0] isEqualToString:@"Hybrid"],"Hybrid Drag Joystick selector present");
        for(NSInteger selected=0;selected<3;++selected){modes.selectedSegmentIndex=selected;[modes sendActionsForControlEvents:UIControlEventValueChanged];check([[owner valueForKey:@"controlMode"] integerValue]==2-selected,"mode action updates host");}
        modes.selectedSegmentIndex=0;[modes sendActionsForControlEvents:UIControlEventValueChanged];
        auto* hide=(UISwitch*)find(settings.view,UISwitch.class,0);hide.on=YES;[hide sendActionsForControlEvents:UIControlEventValueChanged];
        [owner.view layoutIfNeeded];check(find(owner.view,UIButton.class,0).hidden,"No Button hides ZSX");
        hide.on=NO;[hide sendActionsForControlEvents:UIControlEventValueChanged];
        milestone("settings-controls");deadline=now+3;phase=2;return 0;
    }
    if(phase==2){
        [settings.tableView scrollToRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:2] atScrollPosition:UITableViewScrollPositionTop animated:NO];
        milestone("settings-graphics");deadline=now+3;phase=3;return 0;
    }
    if(phase==3){
        [settings.tableView scrollToRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:4] atScrollPosition:UITableViewScrollPositionBottom animated:NO];
        [settings.tableView layoutIfNeeded];
        auto* field=(UITextField*)[settings valueForKey:@"cheatField"];
        check(field!=nil,"cheat input visible at bottom of settings");
        field.text=@"ymgjdsh";[field sendActionsForControlEvents:UIControlEventEditingChanged];
        auto* result=(UILabel*)[settings valueForKey:@"cheatResult"];
        check([result.text containsString:@"已解锁并保存"],"typed code invokes engine and verifies saved unlock");
        validate_unlock();milestone("settings-unlocked");deadline=now+3;phase=4;return 0;
    }
    if(phase==4){
        [settings.tableView.delegate tableView:settings.tableView didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:2 inSection:0]];
        deadline=now+3;phase=5;return 0;
    }
    if(phase==5){
        check([[owner valueForKey:@"editingLayout"] boolValue],"layout editing is active");
        for(NSString* key in @[@"joystick",@"controls"]){
            id value=[owner valueForKey:key];UIView* control=[value isKindOfClass:NSArray.class]?[value objectAtIndex:0]:(UIView*)value;
            CGPoint before=control.center;
            TH20TestPan* pan=[TH20TestPan new];pan.testDelta=CGPointMake(25,-35);[control addGestureRecognizer:pan];
            [owner performSelector:NSSelectorFromString(@"dragLayoutControl:") withObject:pan];[control removeGestureRecognizer:pan];
            check(std::hypot(control.center.x-before.x,control.center.y-before.y)>1,"layout pan moves control through real handler");
        }
        milestone("layout-editor");deadline=now+3;phase=6;return 0;
    }
    if(phase==6){invoke(owner,@"saveLayout");deadline=now+3;phase=7;return 0;}
    if(phase==7){
        check([NSUserDefaults.standardUserDefaults dictionaryForKey:@"customLayoutsV2"].count>0,"custom layout persisted");
        invoke(settings,@"done");deadline=now+3;phase=8;return 0;
    }
    NSString* docs=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    std::ofstream(std::string(docs.fileSystemRepresentation)+"/mobile-settings-probe.json")<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
    th20_ios_log("mobile-probe RESULT %d/%d %s",checks-failures,checks,failures?"FAIL":"PASS");
    phase=9;return failures?-1:1;
}
