// Linked only into the isolated smoke app; exercises production collision,
// bomb/resource functions and actual UIKit developer-menu actions.
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#undef TRUE
#undef FALSE
#define BOOL TH20_WINBOOL
#include "player_entity/events.hpp"
#include "player_entity/owner.hpp"
#include "bomb_system/bomb.hpp"
#include "gameplay/player_state.hpp"
#include "overlay_system/overlay.hpp"
#include "hud_system/hud.hpp"
#include "replay_system/replay.hpp"
#include "bullet_system/shoot.hpp"
#include "laser_system/type0.hpp"
#undef BOOL
#include "ios_cheats.h"
#include "ios_host.h"
#include <fstream>

namespace {
using namespace th20::source;
namespace cheat=th20::ios::cheats;
namespace ps=gameplay::player_state;
int checks=0,failures=0,phase=0;
double deadline=0;
void check(bool ok,const char* message){++checks;if(!ok)++failures;th20_ios_log("dev-probe %s %s",ok?"PASS":"FAIL",message);}
void invoke(id value,NSString* selector){[value performSelector:NSSelectorFromString(selector)];}
void milestone(const char* value){
    NSString* docs=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    std::ofstream(std::string(docs.fileSystemRepresentation)+"/smoke-milestone.txt")<<value;
}
// Observe the downstream death graph without playing its sound. The real
// collision and hit functions still run, including their state transition.
struct Events:player_entity::EventServices {
    int sounds=0,effects=0,notifications=0;
    game_session::Session& session()override{return game_session::session;}
    void mark_enemies()override{++notifications;}
    void notify_secondary(void*)override{++notifications;}
    void sound(int)override{++sounds;}
    void sound_at(int,float)override{++sounds;}
    void spawn_hit_effect(game_session::Context&,const sprite::Vec3&)override{++effects;}
    void reset_player_animation(void*)override{}
    unsigned random_next()override{return 0;}
    void enqueue_graze(game_session::Context&,const sprite::Vec3&,unsigned,int)override{}
    bool special_active()override{return false;}
    bool selected_enemy_present()override{return false;}
    void accumulate_reward(void*,const sprite::Vec3&,int,int)override{}
    void add_special_items(game_session::Player&,int)override{}
};
struct Collision:player_entity::CollisionServices {
    Events events;
    const void* boss_hud()override{return hud::controller;}
    void hit(void* value)override{player_entity::hit(value,events);}
};
void collide(player_entity::Player& p,Collision& c){player_entity::collide_circle(&p,p.position_614,3,0,c);}
void verify(player_entity::Player& p){
    auto& record=*p.context->current_player;
    auto* b=bomb::controller();
    const auto originalRecord=record;const auto originalState=p.state;
    const auto age=p.timers_644[0];const auto field=p.field_2204;
    auto* overlay=static_cast<overlay::WeaponStoneInf*>(p.context->overlay_owner);
    const int overlayPhase=overlay->phase;overlay->phase=0;
    auto reset=[&]{p.state=1;th20::recovered::timer_set(p.timers_2050[0],0);};
    cheat::configure(false,false);
    check(cheat::perform(cheat::max_all)==0,"developer actions gated while disabled");
    cheat::configure(true,false);
    check(cheat::perform(cheat::max_power)==1&&ps::read<int>(record,0x30)==400,"max power refreshes real player options");
    check(cheat::perform(cheat::full_stock)==1&&ps::read<int>(record,0xb8)==7&&ps::read<int>(record,0xcc)==7,"full stock sets seven lives and bombs");
    check(cheat::perform(cheat::max_score)==1&&ps::score(record)==999999999,"score uses game maximum");
    check(cheat::perform(cheat::max_items)==1&&ps::read<int>(record,0x44)==1000000&&ps::read<int>(record,0x5c)==10000,"TH20 item totals and stone gauge filled");
    check(cheat::perform(cheat::max_all)==1,"max all completes without invalidating player");
    auto* bullets=bullet::controller();bullet::ShotParameters shot;
    shot.position={0,100,0};shot.count=shot.rows=1;shot.speed=1;
    bullet::shoot(*bullets,shot,std::make_shared<bullet::ShotMetadata>());
    auto* sampleBullet=reinterpret_cast<bullet::Bullet*>(bullets->active.sentinel.next->value);
    auto* lasers=laser::controller();laser::Type0Parameters beam;
    beam.position={80,80,0};beam.length=80;beam.length_limit=100;beam.width=12;beam.speed=1;
    laser::spawn_type0(*lasers,beam);
    auto* sampleLaser=reinterpret_cast<laser::Laser*>(lasers->active.sentinel.next->value);
    check(sampleBullet->state==1&&sampleLaser->state!=1,"real bullet and laser spawned for clear test");
    check(cheat::perform(cheat::clear_bullets)==1&&sampleBullet->state==4&&sampleLaser->state==1,"clear transitions real bullet and laser to retired states");
    reset();cheat::perform(cheat::invincible);Collision safe;
    collide(p,safe);
    check(p.state==1&&safe.events.sounds==0&&safe.events.effects==0&&safe.events.notifications==0,"invincible contact skips entire death graph");
    cheat::configure(false,false);reset();Collision normal;collide(p,normal);
    check(!cheat::invincible_enabled()&&p.state==4&&normal.events.sounds==1,"disabling developer mode restores normal death sound/state");
    reset();cheat::configure(false,true);ps::write(record,0xcc,0);Collision empty;collide(p,empty);
    check(p.state==4&&empty.events.sounds==1&&!b->active(),"autobomb with zero stock retains normal hit");
    reset();ps::write(record,0xcc,3);const auto life=ps::read<int>(record,0xb8);Collision automatic;
    const auto savedHitTimer=p.timers_644[0];collide(p,automatic);
    check(b->active()&&b->active_bomb&&ps::read<int>(record,0xcc)==2,"collision starts real character bomb and spends exactly one");
    check(p.state==1&&ps::read<int>(record,0xb8)==life&&p.timers_644[0].current==savedHitTimer.current,"autobomb keeps player alive without life loss or hit timer");
    check(automatic.events.sounds==0&&automatic.events.effects==0&&automatic.events.notifications==0,"autobomb runs before sound, hit effect and death notifications");
    collide(p,automatic);collide(p,automatic);
    check(p.state==1&&ps::read<int>(record,0xcc)==2&&p.timers_2050[0].current>=40,"same-frame additional hits cannot spend again or kill");
    auto* replay=replay::controller();if(replay){int mode=replay->mode;replay->mode=1;
        cheat::configure(true,true);check(cheat::perform(cheat::max_score)==0,"replay playback rejects developer actions");replay->mode=mode;}
    // Keep the real bomb and its invulnerability alive until it completes.
    // Restore the hit-test-only mutations and original record after assertions.
    record=originalRecord;p.state=originalState;p.timers_644[0]=age;p.field_2204=field;
    overlay->phase=overlayPhase;cheat::configure(false,false);
}
}
extern "C" int th20_ios_dev_probe(){
    auto* p=static_cast<player_entity::Player*>(game_session::context(0).objects_04[0]);
    if(phase==0&&(!p||p->state!=1||!bomb::controller()||!bomb::controller()->can_trigger()))return 0;
    UIViewController* owner=UIApplication.sharedApplication.keyWindow.rootViewController;
    const double now=CACurrentMediaTime();
    if(phase==0){
        verify(*p);
        [owner setValue:@YES forKey:@"developerMode"];invoke(owner,@"syncCombatOptions");
        [owner.view layoutIfNeeded];
        UIButton* launcher=[owner valueForKey:@"devLauncher"];
        check(!launcher.hidden,"DEV launcher visible in combat after enabling mode");
        [launcher sendActionsForControlEvents:UIControlEventTouchUpInside];[owner.view layoutIfNeeded];
        check([owner valueForKey:@"devOverlay"]!=nil&&[[owner valueForKey:@"enginePaused"] boolValue],"DEV overlay pauses game");
        NSArray<UIButton*>* buttons=[owner valueForKey:@"devButtons"];
        check(buttons.count==8,"all seven cheat actions and close are present");
        [buttons[0] sendActionsForControlEvents:UIControlEventTouchUpInside];
        check(cheat::invincible_enabled()&&[buttons[0].currentTitle hasSuffix:@"ON"],"real UI action toggles invincibility and label");
        milestone("dev-menu-portrait");deadline=now+4;phase=1;return 0;
    }
    if(now<deadline)return 0;
    if(phase==1){
        if(@available(iOS 16.0,*)){
            auto* preference=[[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscapeLeft];
            [owner.view.window.windowScene requestGeometryUpdateWithPreferences:preference errorHandler:nil];
        }
        deadline=now+5;phase=2;return 0;
    }
    if(phase==2){
        [owner.view layoutIfNeeded];UIView* panel=[owner valueForKey:@"devPanel"];UIScrollView* list=[owner valueForKey:@"devList"];
        check(CGRectContainsRect(owner.view.bounds,panel.frame)&&list.bounds.size.height>44,"developer panel remains usable after rotation");
        [list setContentOffset:CGPointMake(0,MAX(0,list.contentSize.height-list.bounds.size.height)) animated:NO];
        milestone("dev-menu-landscape");deadline=now+4;phase=3;return 0;
    }
    if(phase==3){
        invoke(owner,@"closeDeveloper");check(![[owner valueForKey:@"enginePaused"] boolValue],"close resumes battle");
        [owner setValue:@NO forKey:@"developerMode"];invoke(owner,@"syncCombatOptions");[owner.view layoutIfNeeded];
        check(!cheat::invincible_enabled()&&[[owner valueForKey:@"devLauncher"] isHidden],"disable hides DEV and removes invincibility");
        invoke(owner,@"openSettings");deadline=now+2;phase=4;return 0;
    }
    auto* navigation=(UINavigationController*)owner.presentedViewController;
    auto* settings=(UITableViewController*)navigation.topViewController;
    for(NSIndexPath* path in @[[NSIndexPath indexPathForRow:7 inSection:0],[NSIndexPath indexPathForRow:3 inSection:3]]){
        UITableViewCell* cell=[settings.tableView.dataSource tableView:settings.tableView cellForRowAtIndexPath:path];
        UISwitch* toggle=(UISwitch*)cell.accessoryView;
        check([toggle isKindOfClass:UISwitch.class],"new setting renders as an actual switch");
        toggle.on=YES;[toggle sendActionsForControlEvents:UIControlEventValueChanged];
        NSString* key=path.section==0?@"autoBomb":@"developerMode";
        check([[owner valueForKey:key] boolValue]&&[NSUserDefaults.standardUserDefaults boolForKey:key],"settings switch updates and persists combat option");
        toggle.on=NO;[toggle sendActionsForControlEvents:UIControlEventValueChanged];
    }
    invoke(settings,@"done");
    NSString* docs=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    std::ofstream(std::string(docs.fileSystemRepresentation)+"/dev-probe.json")<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
    th20_ios_log("dev-probe RESULT %d/%d",checks-failures,checks);
    return failures?-1:1;
}
