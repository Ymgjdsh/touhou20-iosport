#include "ios_game_input.h"
#include "ios_touch_motion.h"
#include "../../source_reconstruction/program_entry/program_entry.hpp"
#include "../../source_reconstruction/input/input.hpp"
#include "../../source_reconstruction/title_system/title.hpp"
#include "../../source_reconstruction/title_system/data.hpp"
#include "../../source_reconstruction/title_system/name_data.hpp"
#include "../../source_reconstruction/title_system/practice_data.hpp"
#include "../../source_reconstruction/title_system/stones_data.hpp"
#include "../../source_reconstruction/help_system/help.hpp"
#include "../../source_reconstruction/options_system/options.hpp"
#include "../../source_reconstruction/options_system/data.hpp"
#include "../../source_reconstruction/key_config/key_config.hpp"
#include "../../source_reconstruction/key_config/data.hpp"
#include "../../source_reconstruction/pause_system/pause.hpp"
#include "../../source_reconstruction/pause_system/menu_support.hpp"
#include "../../source_reconstruction/pause_system/draw_constants.hpp"
#include "../../source_reconstruction/stone_menu/stone.hpp"
#include "../../source_reconstruction/effect_system/stone_selection.hpp"
#include "../../source_reconstruction/hud_system/hud.hpp"
#include "../../source_reconstruction/player_entity/owner.hpp"
#include "../../source_reconstruction/sprite_renderer/binding.hpp"
#include "../../source_reconstruction/sprite_renderer/pool.hpp"
#include "../../source_reconstruction/sprite_renderer/quad.hpp"
#include "../../source_reconstruction/sprite_renderer/menu_animation.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <cstdio>

namespace th20::ios::input {
namespace {
using namespace source;
namespace pe=program_entry;
namespace nd=source::title::name_data;
constexpr int up=0x26,down=0x28,left=0x25,right=0x27,confirm=0x5a;
struct Scene {
    int current=-100,pending=-100,title=-1,phase=-1,pause=-1,substate=-1,stone=-1,visible=-1,option=-1;
    const void* dialogue=nullptr;
    const void* key_config=nullptr;
    const void* help_page=nullptr;
    int help_state=-1,key_state=-1,key_phase=-1;
    bool alive=false;
    bool operator==(const Scene&)const=default;
};
Scene observed;
TH20IOSInputMode mode=TH20_IOS_INPUT_LOADING;
enum class Menu {none,title,difficulty,character,pause,pause_name,stone_category,stone_item,options,name,help,key_config,stone_record};
Menu queued=Menu::none;
int target=-1,pulse=0,attempts=0,action=confirm,swipe_key=0;
std::deque<int> swipe_steps;
bool physical[256]{};
DragMotion drag;
uint64_t movement_id=0;
bool moving=false;

player_entity::Player* player() noexcept {
    if(pe::graphics_state.field_0b08!=7)return nullptr;
    return static_cast<player_entity::Player*>(game_session::context(0).objects_04[0]);
}
Scene scene() {
    Scene s;s.current=pe::graphics_state.field_0b08;s.pending=pe::graphics_state.field_0b0c;
    if(s.current==4)if(auto* t=title::controller()){
        s.title=t->state;s.phase=t->phase;
        if(t->state==7&&stone_menu::controller){s.stone=stone_menu::controller->state;s.visible=stone_menu::controller->visible;}
    }
    if(s.current==7){
        if(auto* p=pause::controller()){s.pause=p->state;s.substate=p->substate;}
        if(hud::controller)s.dialogue=hud::controller->collecting;
        if(auto* p=player())s.alive=p->state==1&&((p->entity_flags>>6)&3u)==0;
    }
    if(s.current==4||s.current==7){
        if(auto* o=options::controller())s.option=o->state;
        if(auto* k=key_config::controller()){s.key_config=k;s.key_state=k->state;s.key_phase=k->phase;}
        if(auto* h=help::controller()){s.help_page=h;s.help_state=h->substate;}
    }
    return s;
}
TH20IOSInputMode input_mode(const Scene& s) {
    if(s.help_page||s.key_config)return TH20_IOS_INPUT_MENU;
    // Ending CG and staff-roll scripts wait for the ordinary confirm input.
    if(s.current==15)return TH20_IOS_INPUT_DIALOGUE;
    if(s.current==7){
        if(s.pause>0)return TH20_IOS_INPUT_MENU;
        if(s.dialogue)return TH20_IOS_INPUT_DIALOGUE;
        return TH20_IOS_INPUT_GAMEPLAY;
    }
    if(s.current==4&&s.title==3&&s.option>=0)return TH20_IOS_INPUT_MENU;
    if(s.current==4&&s.title==23&&s.phase==1)return TH20_IOS_INPUT_MENU;
    if(s.current==4&&((s.title==12&&s.phase==4)||(s.title==16&&s.phase==3)))return TH20_IOS_INPUT_MENU;
    if(s.current==4&&s.title>=1&&s.phase==2)return TH20_IOS_INPUT_MENU;
    return TH20_IOS_INPUT_LOADING;
}
void sync_scene() {
    auto now=scene();
    if(now==observed)return;
    // Life-state changes are not scene changes. Keep UIKit ownership and
    // held controls intact across respawn, but discard queued displacement.
    if(now.current==observed.current&&now.pending==observed.pending&&now.title==observed.title&&
       now.phase==observed.phase&&now.pause==observed.pause&&now.substate==observed.substate&&
       now.stone==observed.stone&&now.visible==observed.visible&&now.option==observed.option&&
       now.dialogue==observed.dialogue&&now.key_config==observed.key_config&&now.help_page==observed.help_page&&
       now.help_state==observed.help_state&&now.key_state==observed.key_state&&now.key_phase==observed.key_phase){
        observed=now;drag.clear();return;
    }
    observed=now;
    th20_ios_set_combat_scene(now.current==7&&!now.help_page&&!now.key_config&&now.option<0);
    clear();
    th20_ios_clear_input();
    const auto next=input_mode(now);
    if(next!=mode){mode=next;th20_ios_set_input_mode(mode);}
    th20_ios_log("[input] scene=%d next=%d title=%d phase=%d pause=%d/%d stone=%d mode=%d",
        now.current,now.pending,now.title,now.phase,now.pause,now.substate,now.stone,int(mode));
    if(now.current==4&&now.phase==2)th20_ios_set_stage("TH20 title");
    else if(now.current==7)th20_ios_set_stage(now.pause>0?"TH20 paused":now.dialogue?"TH20 dialogue":"TH20 gameplay");
    else if(now.current==15)th20_ios_set_stage("TH20 ending");
}
bool rectangle(float x,float y,float left,float top,float width,float height) {
    return width>0&&height>0&&x>=left&&x<=left+width&&y>=top&&y<=top+height;
}
bool animation_hit(sprite::Animation* animation,float x,float y) {
    if(!animation)return false;
    auto& a=*animation;
    if(!(a.base.flags[1]&1)||!(a.base.flags[0]&0x10000u)||a.retirement||
       !((a.base.field_490|a.base.field_494)>>24)||(a.base.flags[0]&255u)>3||a.base.flags[4]>2||a.base.flags[5]>2)return false;
    sprite::Vec3 corners[4]{};
    const auto type=a.base.flags[0]&255u;
    sprite::calculate_sprite_corners(a,sprite::current_sprite(*pe::sprite_controller,a),
        corners,corners+1,corners+2,corners+3,type==1||type==3);
    float x0=corners[0].x,x1=x0,y0=corners[0].y,y1=y0;
    for(const auto& corner:corners){
        if(!std::isfinite(corner.x)||!std::isfinite(corner.y))return false;
        x0=std::min(x0,corner.x);x1=std::max(x1,corner.x);
        y0=std::min(y0,corner.y);y1=std::max(y1,corner.y);
    }
    return rectangle(x,y,x0,y0,x1-x0,y1-y0);
}
bool handle_hit(uint32_t handle,float x,float y) {
    return pe::sprite_controller&&animation_hit(sprite::find_animation(*pe::sprite_controller,handle),x,y);
}
bool child_hit(uint32_t handle,int script,float x,float y) {
    return pe::sprite_controller&&animation_hit(sprite::find_animation_child(*pe::sprite_controller,handle,script,0),x,y);
}
menu::Cursor* cursor(Menu menu) {
    if(menu==Menu::title||menu==Menu::difficulty||menu==Menu::character||menu==Menu::stone_record){auto* t=title::controller();return t?&t->cursor:nullptr;}
    if(menu==Menu::pause){auto* p=pause::controller();return p?&p->cursor:nullptr;}
    if(menu==Menu::pause_name){auto* p=pause::controller();return p?&p->name_cursor:nullptr;}
    if(menu==Menu::stone_category)return stone_menu::controller?&stone_menu::controller->category:nullptr;
    if(menu==Menu::stone_item)return stone_menu::controller?&stone_menu::controller->selection:nullptr;
    if(menu==Menu::options){auto* o=options::controller();return o?&o->cursor:nullptr;}
    if(menu==Menu::name){auto* t=title::controller();return t?&t->cursor56e8:nullptr;}
    if(menu==Menu::help){auto* h=help::controller();return h?&h->cursor:nullptr;}
    if(menu==Menu::key_config){auto* k=key_config::controller();return k?&k->cursor:nullptr;}
    return nullptr;
}
void enqueue(Menu menu,int index,int final_key=confirm) {
    auto* c=cursor(menu);
    if(!c||index<c->minimum||index>=c->count||c->is_excluded(index))return;
    swipe_steps.clear();swipe_key=0;queued=menu;target=index;attempts=0;action=final_key;
}
float main_text_width(const char* label) {
    if(!text::renderer||!pe::sprite_controller)return 0;
    const float divisor=3.f-pe::window_state.scale;
    if(!(divisor>0))return 0;
    const auto& file=sprite::sprite_file(*pe::sprite_controller,text::renderer->animations[0]);
    float width=0;
    for(auto* c=reinterpret_cast<const unsigned char*>(label);*c;++c){
        const unsigned index=unsigned(*c)+0x1e4u;
        if(index>=file.sprite_count)return 0;
        width+=file.sprites[index].extent_4c/divisor;
    }
    return width;
}
void options_tap(float x,float y) {
    const float scale=pe::window_state.scale;
        auto* o=options::controller();
        if(!o||o->state!=2||key_config::controller())return;
        for(int i=0;i<6;++i){
            // The heading is centered at position.x + 168, but draw() then
            // subtracts 168 again before drawing the left-aligned labels.
            const float px=o->position.x*scale,
                        py=(o->position.y+options::data::f_0056fd48+options::data::f_0056f7a8+
                            options::data::f_0056fd48*i)*scale;
            if(rectangle(x,y,px,py,main_text_width(options::data::labels[i]),23.5f*scale)){
                enqueue(Menu::options,i,i<3?0:confirm);return;
            }
            if(i<3){
                const auto& config=pe::graphics_state.configuration;
                char value[32];
                if(i==0){const int display=pe::window_state.display_mode;std::snprintf(value,sizeof(value),"%s",display<3?"FullScreen":display<8?"Window":display==8?"BorderlessDBD":"Borderless");}
                else std::snprintf(value,sizeof(value),"%d%%",int(i==1?config.value_7e:config.value_7f));
                const float width=main_text_width(value),center=px+240.f*scale;
                if(rectangle(x,y,center-width*.5f,py,width,23.5f*scale)){
                    enqueue(Menu::options,i,x<center?left:right);return;
                }
            }
        }
}
void title_tap(title::TitleInf& t,float x,float y) {
    const float scale=pe::window_state.scale;
    if(t.phase!=2&&!(t.state==23&&t.phase==1)&&!(t.state==12&&t.phase==4)&&!(t.state==16&&t.phase==3))return;
    switch(t.state){
    case 1:
        for(int i=0;i<10;++i){
            const float width=main_text_width(title::data::main_labels[i]);
            // Use the actual centered draw origin (hex-float 0x1.74p+7 = 186).
            if(rectangle(x,y,title::data::f_00575670*scale-width/2-3.f*scale,(230.f+23.f*i)*scale,width+6.f*scale,23.f*scale)){
                enqueue(Menu::title,i);return;
            }
        }break;
    case 5:{
        const bool extra=game_session::session.player_table.field_1e0>=4;
        const auto root=t.handles[extra?59:58];
        for(int i=0;i<(extra?1:4);++i)
            if(child_hit(root,extra?52:48+i,x,y)||(!extra&&child_hit(root,53+i,x,y))){enqueue(Menu::difficulty,i);return;}
        break;}
    case 6:{
        const bool extra=game_session::session.player_table.field_1e0>=4;
        const auto root=t.handles[extra?14:12];
        // Labels have priority where the animated portraits overlap.
        // Both normal and Extra character panels use child scripts 8/9 for
        // the labels and 6/7 for their portrait hit regions.  Extra mode
        // changes the parent ANM (14), not these child script numbers.
        for(int i=0;i<2;++i)if(child_hit(root,8+i,x,y)){enqueue(Menu::character,i);return;}
        for(int i=0;i<2;++i)if(child_hit(root,6+i,x,y)){enqueue(Menu::character,i);return;}
        break;}
    case 8:
        // stage_select_draw.cpp: labels and scores share these six rows.
        if(t.age.current>=10)for(int i=0;i<6;++i)
            if(rectangle(x,y,330.f*scale,(170.f+34.f*i)*scale,260.f*scale,30.f*scale)){
                enqueue(Menu::title,i);return;
            }
        break;
    case 14:
        // music_draw.cpp displays ten rows starting at word56d0.
        for(int row=0;row<std::min(t.age.current,10);++row){
            const auto index=t.word56d0+unsigned(row);
            if(index>=t.words47c[0]||index>=32)break;
            if(rectangle(x,y,64.f*scale,(96.f+20.f*row)*scale,540.f*scale,20.f*scale)){
                enqueue(Menu::title,int(index));return;
            }
        }
        break;
    case 12:
        if(t.phase==2){
            for(int row=0;row<25;++row)
                if(rectangle(x,y,32.f*scale,(80.f+15.f*row)*scale,576.f*scale,15.f*scale)){
                    enqueue(Menu::title,row);return;
                }
        }else if(t.age.current>=10){
            for(int row=0;row<7;++row)
                if(rectangle(x,y,220.f*scale,(128.f+18.f*row)*scale,320.f*scale,18.f*scale)){
                    enqueue(Menu::title,row);return;
                }
        }
        break;
    case 23:
        if(t.phase==1)for(int i=0;i<41;++i){
            namespace d=title::stones_data;
            const float px=(d::f_00575664+(i%10)*d::f_00575658)*scale;
            const float py=(d::f_00570ad0+(i/10)*d::f_00575654)*scale;
            if(rectangle(x,y,px-4*scale,py,32*scale,30*scale)){
                // First tap shows the record; tapping it again plays the
                // unlocked ending/replay through the original confirm path.
                enqueue(Menu::stone_record,i,t.cursor.current==i?confirm:0);return;
            }
        }
        break;
    case 11:{
        // Player-data detail owns four real arrow animations: horizontal
        // changes character/stone and vertical changes difficulty.
        constexpr int keys[]{left,right,up,down};
        for(int i=0;i<4;++i)if(handle_hit(t.handles[73+i],x,y)){swipe_key=keys[i];return;}
        if(t.words58d8[8]==1&&rectangle(x,y,32*scale,140*scale,576*scale,200*scale))swipe_key=confirm;
        break;}
    case 10:
        // player_data.cpp spawns ANM 78 and signals child scripts 7..9 for
        // its three rows. Use the rendered child bounds so locked/hidden
        // entries cannot be selected by an arbitrary rectangle.
        if(t.age.current>=6)
            for(int i=0;i<3;++i)
                if(child_hit(t.handles[78],7+i,x,y)){enqueue(Menu::title,i);return;}
        break;
    case 15:
        // The score rank is fixed. Only its name keyboard is selectable;
        // sending Up/Down to "select" a score row actually changes a letter.
        if(t.word56e4!=0)swipe_key=confirm;
        break;
    case 16:
        if(t.phase==2){
            constexpr float left=nd::f_0056ed10,top=nd::f_0056fa30,row=15.f;
            for(int i=0;i<25;++i)if(rectangle(x,y,left*scale,(top+row*i)*scale,584.f*scale,row*scale)){
                enqueue(Menu::title,i);return;
            }
        }else if(t.phase==3&&t.age.current>=10){
            if(rectangle(x,y,nd::f_0056f090*scale,nd::f_0056f108*scale,280.f*scale,18.f*scale)){
                enqueue(Menu::name,t.cursor56e8.current);return;
            }
        }
        break;
    case 18:
        // Stage page itself is represented by ANM 85; child scripts 7..13
        // are the seven rendered stage labels.
        if(t.age.current>=10)for(int i=0;i<7;++i)
            if(child_hit(t.handles[85],7+i,x,y)){enqueue(Menu::title,i);return;}
        break;
    case 19:{
        const int stage=std::clamp(static_cast<int>(t.words58d8[0]),0,6);
        const int count=title::practice_data::boss_counts[stage];
        if(t.age.current>=10)for(int i=0;i<count;++i)
            if(child_hit(t.handles[86],7+i,x,y)){enqueue(Menu::title,i);return;}
        break;}
    case 20:
        if(t.age.current>=10)for(int i=0;i<5;++i)
            // refresh_practice_cards() draws dynamic text at x=220, y=168
            // with a 46-pixel stride; handles394 may not own that text.
            if(handle_hit(t.handles394[i],x,y)||rectangle(x,y,
                title::practice_data::f_00575678*scale,
                (title::practice_data::f_00571058+i*title::practice_data::f_0057565c)*scale,
                388*scale,44*scale)){enqueue(Menu::title,i);return;}
        break;
    }
}

void name_keyboard_tap(title::TitleInf& t,float x,float y) {
    if(t.age.current<10||!((t.state==15&&t.phase==2&&t.word56e4==0)||(t.state==16&&t.phase==3)))return;
    const int count=static_cast<int>(std::strlen(pause::name_characters()));
    const float scale=pe::window_state.scale;
    for(int i=0;i<count;++i){
        const int row=i/13,col=i%13;
        const float left=nd::f_00575674+18.f*col;
        const float top=360.f+16.f*row;
        if(rectangle(x,y,left*scale,top*scale,18.f*scale,16.f*scale)){
            enqueue(Menu::name,i);return;
        }
    }
}
void pause_tap(pause::PauseInf& p,float x,float y) {
    const float scale=pe::window_state.scale;
    if(p.substate==6){
        constexpr int slots[]{0,1,2,3,4,5,0,1,2,3,4,5,0,1,4,5,1,5,1,2,4,5};
        for(int i=0;i<22;++i)if(child_hit(p.panel_handle,119+i,x,y)){enqueue(Menu::pause,slots[i]);return;}
    }else if((p.substate==7||p.substate==9)&&p.age.current>=30){
        for(int i=0;i<2;++i)if(child_hit(p.panel_handle,142+i,x,y)){enqueue(Menu::pause,i);return;}
    }else if(p.substate==11&&p.age.current>=10){
        namespace d=pause::draw_data;
        for(int i=0;i<25;++i)
            if(rectangle(x,y,d::f_0056fb7c*scale,(d::f_0056fa28+i*d::f_0056f2fc)*scale,360*scale,15*scale)){
                enqueue(Menu::pause,i);return;
            }
    }else if((p.substate==12||p.substate==15)&&p.age.current>=10){
        namespace d=pause::draw_data;
        for(int i=0;i<int(std::strlen(pause::name_characters()));++i)
            if(rectangle(x,y,(d::f_00572658+(i%13)*d::f_00572644)*scale,
                (d::f_0056f10c+(i/13)*d::f_0056cd90)*scale,18*scale,16*scale)){
                enqueue(Menu::pause_name,i);return;
            }
    }
}
void stone_tap(stone_menu::StoneMenuInf& s,float x,float y) {
    if(s.visible!=1)return;
    if(s.state==3&&pe::sprite_controller){
        auto* animation=sprite::find_animation(*pe::sprite_controller,s.animation_handles[0]);
        if(!animation)return;
        // The selection effect owns independent ANM handles, not ANM children.
        auto* effect=dynamic_cast<effects::StoneSelection*>(reinterpret_cast<sprite::AnimationCallback*>(animation->callback));
        if(!effect)return;
        if(handle_hit(effect->center_handle,x,y)){enqueue(Menu::stone_category,0);return;}
        for(int i=0;i<4;++i)if(handle_hit(effect->handles[i],x,y)){enqueue(Menu::stone_category,i+1);return;}
    }else if(s.state==4&&s.age.current>20){
        const float scale=pe::window_state.scale;
        const float px=(s.selection_position.x-240.f)*.5f*scale;
        const float py=(s.selection_position.y-(s.category.current==1?190.f:210.f))*.5f*scale;
        const int count=s.category.current==1?8:9;
        // These rows are the exact text/count columns emitted by stone draw().
        for(int i=0;i<count;++i)if(rectangle(x,y,px,py+24.f*i*scale,248.f*scale,24.f*scale)){
            enqueue(Menu::stone_item,i);return;
        }
    }
}
void issue(int key) noexcept {pulse=key;th20_ios_key_event(unsigned(key),1);}
void cancel_navigation() noexcept {
    queued=Menu::none;swipe_steps.clear();swipe_key=0;
    if(pulse){const int released=pulse;pulse=0;th20_ios_key_event(unsigned(released),physical[released]);}
}
void select_binding(key_config::KeyConfigInf& k,int index) {
    // The keyboard-binding page treats arrows as assignable raw keys. Do
    // not synthesize arrow presses to reach a tapped row: that rebinds the
    // previously selected action as a side effect of touch navigation.
    const int previous=k.cursor.current;k.cursor.select(index);
    if(k.cursor.current!=previous){
        key_config::environment().play_effect(10);
        recovered::timer_set(k.selection_age,8);
    }
}
void activate_binding(key_config::KeyConfigInf& k) {
    auto& e=key_config::environment();
    const int kind=e.input.devices[e.input.selected[k.selected_slot]].kind;
    const int group=kind==1?0:kind==2?1:2;
    auto* selected=k.bindings[group];
    if(k.cursor.current==k.cursor.count-1){
        auto& mapping=e.input.mappings[k.selected_slot];
        auto* destination=group==0?mapping.pad:group==1?mapping.alternate_pad:mapping.keyboard;
        std::memcpy(destination,selected,16);
        e.configuration.bindings[k.selected_slot]=mapping;
        e.play_effect(9);key_config::set_phase(k,3);
    }else if(kind!=0&&k.cursor.current==4){
        const auto* defaults=group==0?e.defaults.pad:e.defaults.alternate_pad;
        std::memcpy(selected,defaults,16);e.play_effect(7);recovered::timer_set(k.age,0);
    }
}
void navigate() {
    if(pulse){const int released=pulse;pulse=0;th20_ios_key_event(unsigned(released),physical[released]);return;}
    if(swipe_key){const int key=swipe_key;swipe_key=0;issue(key);return;}
    if(!swipe_steps.empty()){
        const int key=swipe_steps.front();swipe_steps.pop_front();
        if(auto* k=key_config::controller();k&&k->state==2&&k->phase==2){
            const int change=(key==up||key==left)?-1:1;
            int index=k->cursor.current+change;
            if(index<0)index=k->cursor.count-1;else if(index>=k->cursor.count)index=0;
            select_binding(*k,index);
        }else issue(key);
        return;
    }
    if(queued==Menu::none)return;
    auto* c=cursor(queued);
    if(!c||target<0||target>=c->count||c->is_excluded(target)||++attempts>64){queued=Menu::none;return;}
    if(c->current==target){if(action)issue(action);queued=Menu::none;return;}
    if(queued==Menu::stone_category){
        constexpr int toward[]{0,up,left,right,down};
        constexpr int opposite[]{0,down,right,left,up};
        issue(target? toward[target]:opposite[std::clamp(c->current,0,4)]);
    }else{
        const bool horizontal=queued==Menu::character;
        if(queued==Menu::name||queued==Menu::pause_name||queued==Menu::stone_record){
            const int columns=queued==Menu::stone_record?10:13;
            const int current=std::max(0,c->current),from_row=current/columns,to_row=target/columns;
            // Align the column first, so a partial final row (record 41 or
            // the final name keys) cannot wrap past the requested target.
            if(current%columns!=target%columns)issue(current%columns<target%columns?right:left);
            else if(from_row!=to_row)issue(from_row<to_row?down:up);
        }else issue(c->current<target?(horizontal?right:down):(horizontal?left:up));
    }
}
}
void clear() noexcept {
    th20_ios_clear_keys();std::memset(physical,0,sizeof(physical));
    queued=Menu::none;target=-1;pulse=0;swipe_key=0;swipe_steps.clear();attempts=0;drag.clear();moving=false;
    auto& legacy=source::input::shared_state();
    for(auto& slot:legacy.slots)source::input::initialize(slot);
    for(auto& slot:legacy.previous_slots)source::input::initialize(slot);
    if(auto* controller=source::input::controller)for(auto& device:controller->devices){
        source::input::initialize(device.buttons);std::memset(device.raw,0,sizeof(device.raw));
    }
    pe::window_state.input_latch=0;
    for(auto& repeat:pe::window_state.repeat)repeat={};
}
void key(int virtual_key,bool down) noexcept {
    if(virtual_key<0||virtual_key>=256)return;
    if(virtual_key==0x1b&&down)if(auto* k=key_config::controller();k&&k->phase==2){
        cancel_navigation();
        // The touch Back button must cancel binding capture, rather than
        // assigning Escape to the selected action.
        if(k->state==2)key_config::set_phase(*k,4);
        else if(k->state==1)enqueue(Menu::key_config,2);
        return;
    }
    physical[virtual_key]=down;
    th20_ios_key_event(unsigned(virtual_key),down||pulse==virtual_key);
}
void touch(TH20IOSTouchPhase phase,uint64_t id,float x,float y,float dx,float dy) {
    sync_scene();
    if(phase==TH20_IOS_TOUCH_MENU_SWIPE){
        if(mode==TH20_IOS_INPUT_MENU&&std::isfinite(dx)&&std::isfinite(dy)){
            queued=Menu::none;
            swipe_key=std::fabs(dx)>std::fabs(dy)?(dx>0?right:left):(dy>0?down:up);
            // The manual's page changes use Up/Down even for a horizontal
            // page-turn gesture. Its contents list still uses vertical rows.
            if(auto* h=help::controller();h&&h->substate==4)
                swipe_key=std::fabs(dx)>std::fabs(dy)?(dx<0?down:up):(dy>0?down:up);
            if(!swipe_steps.empty()&&swipe_steps.back()!=swipe_key)swipe_steps.clear();
            if(swipe_steps.size()<12)swipe_steps.push_back(swipe_key);
            swipe_key=0;
        }
        return;
    }
    if(phase==TH20_IOS_TOUCH_MENU_TAP){
        if(mode==TH20_IOS_INPUT_DIALOGUE){swipe_key=confirm;return;}
        if(mode!=TH20_IOS_INPUT_MENU)return;
        if(auto* h=help::controller()){
            if(h->state==1&&h->substate==1&&h->age.current>=20)
                for(int i=0;i<9;++i)if(handle_hit(h->handles[i],x,y)){enqueue(Menu::help,i);return;}
            if(h->substate==4&&h->age.current>=20)swipe_key=confirm;
            return;
        }
        if(auto* k=key_config::controller()){
            if(k->phase!=2||(k->state!=1&&k->state!=2))return;
            namespace d=key_config::data;const float scale=pe::window_state.scale;
            for(int i=0;i<k->cursor.count;++i){
                const float py=k->state==1?k->position.y+d::f_0056fc04+(i?i+1:0)*d::f_0056fd48:
                    k->position.y+2*d::f_0056fd48+d::f_00571050+i*d::f_0056fd48;
                // Device rows are centered at x+168. Binding labels start
                // at x+28, with their centered value column at x+288.
                const float px=k->position.x+(k->state==1?0:d::f_00571058-d::f_00571054);
                if(rectangle(x,y,px*scale,py*scale,(k->state==1?336:308)*scale,24*scale)){
                    if(k->state==1)enqueue(Menu::key_config,i);
                    else {cancel_navigation();select_binding(*k,i);activate_binding(*k);}
                    return;
                }
            }
            return;
        }
        if(observed.option>=0){options_tap(x,y);return;}
        if(observed.current==7){if(auto* p=pause::controller())pause_tap(*p,x,y);}
        else if(observed.current==4){
            if(observed.title==7&&stone_menu::controller)stone_tap(*stone_menu::controller,x,y);
            else if(auto* t=title::controller()){title_tap(*t,x,y);if(t->state==15||t->state==16)name_keyboard_tap(*t,x,y);}
        }
        return;
    }
    if(mode!=TH20_IOS_INPUT_GAMEPLAY){moving=false;drag.clear();return;}
    if(phase==TH20_IOS_TOUCH_BEGIN){movement_id=id;moving=true;drag.clear();}
    else if(moving&&movement_id==id){
        if(phase==TH20_IOS_TOUCH_MOVE){if(observed.alive)drag.add(dx,dy);else drag.clear();}
        else if(phase==TH20_IOS_TOUCH_END)moving=false;
        else if(phase==TH20_IOS_TOUCH_CANCEL){moving=false;drag.clear();}
    }
}
void before_frame(){sync_scene();navigate();}
void after_frame(){sync_scene();}
bool player_screen_anchor(float& x,float& y,bool& focused) noexcept {
    auto* p=player();
    if(!p||p->state!=1)return false;
    x=224.f+p->position_614.x;y=16.f+p->position_614.y;
    focused=p->focused_204c!=0;
    return std::isfinite(x)&&std::isfinite(y);
}
void apply_drag(source::player_entity::Player& p,int& x,int& y,float rate) noexcept {
    if(p.view_index!=0||p.state!=1||mode!=TH20_IOS_INPUT_GAMEPLAY){drag.clear();return;}
    const float scale=pe::window_state.scale;
    if(!std::isfinite(rate)||rate<=0||!std::isfinite(scale)||scale<=0){drag.clear();return;}
    const auto delta=drag.step();
    if(delta.x==0&&delta.y==0)return;
    // The existing fixed-position integrator multiplies velocity by clock rate.
    // Cancel that multiplier for direct finger displacement, preserving its
    // clamping and float/fixed synchronization and all downstream animation.
    auto add=[&](int value,double delta){
        const double result=double(value)+double(delta)*128.0/(double(scale)*double(rate));
        return int(std::clamp(result,-2147483648.0,2147483647.0));
    };
    x=add(x,delta.x);y=add(y,delta.y);
    p.fields_674[2]=x<0?(y<0?5u:y>0?7u:3u):x>0?(y<0?6u:y>0?8u:4u):y<0?1u:y>0?2u:0u;
}
}
