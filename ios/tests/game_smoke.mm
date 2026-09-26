// Only linked into th20_ios_game_smoke. Runs the actual game and host controls;
// Uses an isolated save to exercise content unlock; never enters the production app.
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#undef TRUE
#undef FALSE
#define BOOL TH20_WINBOOL
#include "program_entry/program_entry.hpp"
#include "title_system/title.hpp"
#include "gameplay/gameplay.hpp"
#include "player_entity/owner.hpp"
#include "game_session/session.hpp"
#include "pause_system/pause.hpp"
#undef BOOL
#include "ios_game_input.h"
#include "ios_host.h"
#include <fstream>
#include <string>
#include <cstdlib>
extern "C" int th20_ios_mobile_settings_probe();
extern "C" int th20_ios_dev_probe();
extern "C" int th20_ios_language_probe();

namespace {
namespace pe=th20::source::program_entry;
namespace title=th20::source::title;
namespace gameplay=th20::source::gameplay;
namespace input=th20::ios::input;
NSTimer* smokeTimer;
__weak UIButton* pressedButton;
double started=0,lastAction=0;
bool tappedTitle=false,sawDifficulty=false,sawCharacter=false,sawLoadout=false,dragged=false,finished=false;
bool settingsStarted=false,settingsPassed=false;
int musicPhase=0;
int orientationPhase=0;
double orientationDeadline=0;
bool orientationPassed=false;
int cameraStep=0,cameraReadyFrame=0;
double cameraDeadline=0;
bool cameraPending=false;
void requestOrientation(UIInterfaceOrientationMask orientation){
    UIWindowScene* scene=UIApplication.sharedApplication.keyWindow.windowScene;
    if(@available(iOS 16.0,*)){
        UIWindowSceneGeometryPreferencesIOS* preference=[[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:orientation];
        [scene requestGeometryUpdateWithPreferences:preference errorHandler:^(NSError* error){th20_ios_log("SMOKE orientation request: %s",error.description.UTF8String);}];
    }
}
void milestone(const char* name){
    NSString* docs=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    std::ofstream(std::string(docs.fileSystemRepresentation)+"/smoke-milestone.txt")<<name;
}
float beforeX=0;bool moved=false;
UIButton* control(NSInteger tag) {
    for(UIWindow* window in UIApplication.sharedApplication.windows)
        if(window.isKeyWindow)for(UIView* view in window.rootViewController.view.subviews)
            if([view isKindOfClass:UIButton.class]&&view.tag==tag)return (UIButton*)view;
    return nil;
}
void press(NSInteger tag){
    UIButton* button=control(tag);
    if(!button){th20_ios_log("SMOKE missing host button %ld",long(tag));return;}
    [button sendActionsForControlEvents:UIControlEventTouchDown];pressedButton=button;lastAction=CACurrentMediaTime();
}
void finish(bool passed,const char* reason,int frame){
    finished=true;[smokeTimer invalidate];smokeTimer=nil;
    if(pressedButton)[pressedButton sendActionsForControlEvents:UIControlEventTouchUpInside];
    pressedButton=nil;
    NSString* documents=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    std::ofstream out(std::string(documents.fileSystemRepresentation)+"/game-smoke.json");
    out<<"{\"passed\":"<<(passed?"true":"false")<<",\"reason\":\""<<reason<<"\",\"stage_frame\":"<<frame
       <<",\"title_tap\":"<<tappedTitle<<",\"difficulty\":"<<sawDifficulty<<",\"character\":"<<sawCharacter
       <<",\"loadout\":"<<sawLoadout<<",\"movement\":"<<moved<<",\"music_tap_and_scroll\":"<<(musicPhase==4)
       <<",\"pause_and_rotation\":"<<orientationPassed<<"}\n";
    th20_ios_log("SMOKE %s reason=%s stage_frame=%d movement=%d",passed?"PASS":"FAIL",reason,frame,moved);th20_ios_flush_log();
}
void tick(){
    if(finished)return;
    const auto now=CACurrentMediaTime();if(!started)started=now;
    if(pressedButton){[pressedButton sendActionsForControlEvents:UIControlEventTouchUpInside];pressedButton=nil;}
    // Intel simulator startup and software GLES can take several minutes
    // before the first stage; keep this test-only timeout above that cost.
    if(now-started>1800){finish(false,"timeout before playable stage",gameplay::controller?gameplay::controller->frame_timer.current:-1);return;}
    const auto scene=pe::graphics_state.field_0b08;
    if(std::getenv("TH20_LANGUAGE_PROBE")){
        const int result=th20_ios_language_probe();
        if(result)finish(result>0,"Japanese/Chinese language and resource probe",-1);
        return;
    }
    const bool developerProbe=std::getenv("TH20_DEV_PROBE")!=nullptr;
    const bool cameraProbe=std::getenv("TH20_CAMERA_PROBE")!=nullptr;
    if(developerProbe||cameraProbe){settingsStarted=settingsPassed=true;musicPhase=4;}
    if(settingsStarted&&!settingsPassed){
        const int result=th20_ios_mobile_settings_probe();
        if(result<0){finish(false,"mobile settings probe failed",-1);return;}
        settingsPassed=result>0;return;
    }
    if(scene==4){
        auto* t=title::controller();if(!t||t->phase!=2||now-lastAction<1.0)return;
        if(t->state==1&&!tappedTitle){
            if(!settingsStarted){settingsStarted=true;th20_ios_mobile_settings_probe();return;}
            const float scale=pe::window_state.scale;
            if(musicPhase==0){
                input::touch(TH20_IOS_TOUCH_MENU_TAP,1,186.f*scale,378.f*scale,0,0);
                musicPhase=1;lastAction=now;return;
            }
            if(musicPhase!=4)return;
            input::touch(TH20_IOS_TOUCH_MENU_TAP,1,186.f*scale,240.f*scale,0,0);
            tappedTitle=true;lastAction=now;th20_ios_log("SMOKE tapped title Start");
        }else if(t->state==14){
            if(t->age.current<15)return;
            const float scale=pe::window_state.scale;
            if(musicPhase==1){
                input::touch(TH20_IOS_TOUCH_MENU_TAP,1,200.f*scale,166.f*scale,0,0);
                musicPhase=2;lastAction=now;return;
            }
            if(musicPhase==2&&t->cursor.current==3&&t->words47c[2]==3){
                milestone("music-upright");
                for(int i=0;i<3;++i)input::touch(TH20_IOS_TOUCH_MENU_SWIPE,1,0,0,0,24);
                musicPhase=3;lastAction=now;return;
            }
            if(musicPhase==3&&now-lastAction>4&&t->cursor.current==6){
                th20_ios_log("SMOKE music tap and three queued scroll steps PASS");musicPhase=4;press(2);
            }
        }else if(t->state==5){sawDifficulty=true;press(0);}
        else if(t->state==6){sawCharacter=true;press(0);}
        else if(t->state==7){sawLoadout=true;press(0);}
        return;
    }
    if(scene!=7||!gameplay::controller)return;
    auto* player=static_cast<th20::source::player_entity::Player*>(th20::source::game_session::context(0).objects_04[0]);
    const int frame=gameplay::controller->frame_timer.current;
    if(cameraProbe){
        UIViewController* owner=UIApplication.sharedApplication.keyWindow.rootViewController;
        if([[owner valueForKey:@"inputMode"] integerValue]!=TH20_IOS_INPUT_GAMEPLAY){
            if(now-lastAction>1)press(0);
            return;
        }
        if(frame<180||now<cameraDeadline)return;
        const float zooms[]{1,2,.5f,.1f,3};
        const char* names[]{"camera-1x","camera-2x","camera-0_5x","camera-0_1x","camera-3x"};
        if(cameraStep==5){finish(true,"native battle camera screenshots captured at five zoom levels",frame);return;}
        if(!cameraPending){
            [owner setValue:@YES forKey:@"battleZoomEnabled"];
            [owner setValue:@YES forKey:@"autoShot"];
            [owner setValue:@YES forKey:@"alwaysShowHitbox"];
            [owner setValue:@(zooms[cameraStep]) forKey:@"battleZoom"];
            cameraReadyFrame=frame+8;cameraPending=true;return;
        }
        if(frame<cameraReadyFrame)return;
        float actual=0,ax=0,ay=0;
        if(!th20_ios_battle_camera(&actual,&ax,&ay)||actual!=zooms[cameraStep]){
            finish(false,"battle camera setting did not reach renderer",frame);return;
        }
        milestone(names[cameraStep]);th20_ios_log("SMOKE camera=%g stage_frame=%d",actual,frame);
        ++cameraStep;cameraPending=false;cameraDeadline=now+3;return;
    }
    if(developerProbe){
        const int result=th20_ios_dev_probe();
        if(result)finish(result>0,"developer menu and collision autobomb probe",frame);
        return;
    }
    // Actual UIKit rotation and real game pause; geometry-only unit tests
    // cannot establish that the host updates its drawable at the same time.
    const bool canPause=[ [UIApplication.sharedApplication.keyWindow.rootViewController valueForKey:@"inputMode"] integerValue]==TH20_IOS_INPUT_GAMEPLAY;
    if(frame>180&&!orientationPassed&&(orientationPhase>0||canPause)){
        UIViewController* owner=UIApplication.sharedApplication.keyWindow.rootViewController;
        if(orientationPhase==0){press(3);orientationPhase=1;orientationDeadline=now+5;return;}
        if(now<orientationDeadline)return;
        if(orientationPhase==1){
            auto* pause=th20::source::pause::controller();
            if(!pause||pause->state==0)return;
            if(![[owner valueForKey:@"combatScene"] boolValue]){finish(false,"pause lost battle composition",frame);return;}
            milestone("battle-paused-portrait");orientationDeadline=now+5;orientationPhase=2;return;
        }
        if(orientationPhase==2){requestOrientation(UIInterfaceOrientationMaskLandscapeLeft);orientationDeadline=now+6;orientationPhase=3;return;}
        if(orientationPhase==3){
            if(owner.view.bounds.size.width<=owner.view.bounds.size.height){finish(false,"landscape rotation failed",frame);return;}
            milestone("battle-paused-landscape");orientationDeadline=now+5;orientationPhase=4;return;
        }
        if(orientationPhase==4){requestOrientation(UIInterfaceOrientationMaskPortrait);orientationDeadline=now+6;orientationPhase=5;return;}
        if(owner.view.bounds.size.height<=owner.view.bounds.size.width){finish(false,"portrait rotation failed",frame);return;}
        milestone("battle-restored-portrait");orientationPassed=true;press(0);return;
    }
    if(player&&player->state==1&&frame>90&&!dragged){
        beforeX=player->position_614.x;
        input::touch(TH20_IOS_TOUCH_BEGIN,2,320,320,0,0);
        input::touch(TH20_IOS_TOUCH_MOVE,2,360,320,40*pe::window_state.scale,0);
        input::touch(TH20_IOS_TOUCH_END,2,360,320,0,0);
        dragged=true;milestone("battle-portrait");th20_ios_log("SMOKE applied relative drag at stage frame %d",frame);
    }else if(dragged&&player&&player->position_614.x!=beforeX)moved=true;
    if(now-lastAction>=1.0)press(0);
    if(frame>=240&&orientationPassed){milestone("battle-final");finish(settingsPassed&&musicPhase==4&&tappedTitle&&sawDifficulty&&sawCharacter&&sawLoadout&&moved,"stage movement, pause and rotation verified after 240 frames",frame);}
}
}
@interface TH20NativeSmokeDriver:NSObject
@end
@implementation TH20NativeSmokeDriver
+ (void)load {
    if(std::getenv("TH20_LANGUAGE_PROBE")||std::getenv("TH20_CAMERA_PROBE")){
        [NSUserDefaults.standardUserDefaults removeObjectForKey:@"gameLanguage"];
        // Software GLES on Intel needs a smaller output surface; game logic
        // and resource/font loading still execute the production paths.
        [NSUserDefaults.standardUserDefaults setDouble:0.5 forKey:@"renderScale"];
    }
    [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*){
        if(!smokeTimer&&!finished){
            th20_ios_log("SMOKE test-only driver started; production app does not include this driver");
            smokeTimer=[NSTimer scheduledTimerWithTimeInterval:0.1 repeats:YES block:^(NSTimer*){tick();}];
        }
    }];
}
@end
