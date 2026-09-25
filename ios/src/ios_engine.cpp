#include "ios_host.h"
#include "ios_platform.h"
#include "ios_game_input.h"
#include "ios_unlock.h"
#include "../../source_reconstruction/program_entry/program_entry.hpp"
#include "../../source_reconstruction/program_entry/unrecovered_dependencies.hpp"
#include "../../source_reconstruction/program_entry/text_constants.hpp"
#include <cmath>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <mmsystem.h>

namespace {
namespace pe=th20::source::program_entry;
namespace u=pe::unrecovered;
namespace input=th20::ios::input;
struct Engine {
    u::RuntimeListenerStorage listener;
    bool runtime=false, session=false, running=false, paused=false;
    bool listener_installed=false, configuration_loaded=false;
    bool report_engine_errors() {
        std::lock_guard guard(th20::source::runtime::shared_locks().slot(3));
        if(!pe::log_buffer.error)return false;
        running=false;
        th20_ios_report_cp932_error(pe::log_buffer.text.c_str());
        return true;
    }
    void reset_clock() {
        auto& w=pe::window_state;
        w.clock_offset=0;
        w.next_update_time=w.current_time=w.previous_time=u::read_clock(w);
        w.current_draw_time=w.previous_draw_time=w.current_time;
    }
    void start_session() {
        auto& w=pe::window_state;
        th20_ios_set_stage("Creating native graphics");
        if(u::fn_0041c320()!=0) throw std::runtime_error("Cannot create native graphics device");
        if(u::create_window(w,w.instance)!=0) throw std::runtime_error("Cannot configure native game surface");
        if(u::fn_0041c3e0(w)!=0) throw std::runtime_error("Cannot prepare native presentation");
        pe::function_controller=u::make_function_controller(pe::allocations,pe::text_constants::function_allocation_site);
        pe::sprite_controller=u::make_sprite_controller(pe::allocations,pe::text_constants::sprite_allocation_site);
        if(!pe::function_controller||!pe::sprite_controller) throw std::bad_alloc();
        reset_clock();
        th20_ios_set_stage("Loading TH20 game resources");
        // The registration's before-insert callback performs real archive,
        // input, texture and surface VM initialization, and can throw midway.
        session=true;
        const int status=u::fn_004de1f0();
        if(status!=0) throw std::runtime_error("TH20 graphics initialization failed");
        if(report_engine_errors())throw std::runtime_error("TH20 resource initialization failed; see engine diagnostic log");
        pe::set_draw_counter(w,0xfc);
        w.flags|=1u;
        w.quit_requested=0;
        running=true;
        input::clear();
        th20_ios_set_stage("Starting TH20");
    }
    void finish_session() {
        running=false;
        input::clear();
        auto& g=pe::graphics_state;
        if(session) {session=false;u::fn_004dd490(g);}
        if(pe::function_controller) {
            u::free_function_controller(pe::allocations,pe::function_controller);
            pe::function_controller=nullptr;
        }
        u::fn_00426170(pe::thread_registry);
        if(pe::sprite_controller) {
            u::free_sprite_controller(pe::allocations,pe::sprite_controller);
            pe::sprite_controller=nullptr;
        }
        for(auto** resource:{&g.resource_019c,&g.resource_01a0,&g.resource_01a4})
            if(*resource){(*resource)->Release();*resource=nullptr;}
        pe::release_device(g);
        pe::release_direct3d(g);
        pe::window_state.window=nullptr;
    }
    void shutdown() {
        if(!runtime)return;
        finish_session();
        if(configuration_loaded)u::save_configuration(pe::graphics_state.configuration);
        u::log_flush(pe::log_buffer);
        timeEndPeriod(1);
        if(pe::allocations){u::destroy_allocations(pe::allocations,1);pe::allocations=nullptr;}
        u::lock_registry_disable(pe::lock_registry);
        runtime=false;
        // Global PMR owners retain this allocator until process destruction.
        // The Engine and its listener deliberately share process lifetime.
        th20_ios_flush_log();
    }
    bool initialize(const char* resources,const char* saves) {
        th20_ios_configure_paths(resources,saves);
        // Archive and streaming BGM opens retain their recovered relative
        // names. Writable configuration paths are resolved via Documents.
        std::filesystem::current_path(th20_ios_resource_directory());
        auto& w=pe::window_state;
        auto& g=pe::graphics_state;
        w.instance=reinterpret_cast<HINSTANCE>(this);
        w.startup_status=0;
        timeBeginPeriod(1);
        u::set_rounding_mode(0);
        u::lock_registry_enable(pe::lock_registry);
        runtime=true;
        auto* allocation=u::allocate(sizeof(pe::AllocationController));
        if(!allocation)throw std::bad_alloc();
        pe::allocations=u::construct_allocations(allocation);
        if(!listener_installed){u::construct_listener(listener);u::install_listener(&listener);listener_installed=true;}
        u::log_append(pe::log_buffer,pe::text_constants::start);
        if(u::fn_0041c020(w.instance)==-1)throw std::runtime_error("TH20 process initialization failed");
        u::fn_004117a0(g,w.instance);
        if(u::fn_0041b4f0(w)!=0)throw std::runtime_error("TH20 save directory initialization failed");
        if(u::load_configuration(g,"th20.cfg")!=0)throw std::runtime_error("TH20 configuration could not be loaded");
        configuration_loaded=true;
        u::fn_00420f80();u::fn_00421040();
        // UIKit already loaded persisted mobile settings before initialization.
        // Its Settings button replaces the desktop pre-window display chooser.
        if(((w.flags>>3)&3u)!=0)throw std::runtime_error("TH20 input initialization failed");
        w.display_mode=g.configuration.saved_display_mode;
        u::fn_00416d20();
        start_session();
        return true;
    }
    void reset_device() {
        auto& w=pe::window_state;auto& g=pe::graphics_state;
        pe::set_reset_delay(w,10);
        if(pe::needs_device_reset(w))u::fn_0041e050(w,0);
        u::fn_004dd840(g);u::fn_0041dbe0(pe::sprite_controller);
        if(u::fn_0041c730(1)!=0)throw std::runtime_error("Native graphics device reset failed");
        u::fn_0041a2c0();u::fn_0041d9f0(pe::sprite_controller);
        pe::device_reset_countdown=3;pe::graphics_event_flags|=8u;
        g.window_rectangle={0,0,w.client_width,w.client_height};
        u::fn_004dbd70(g);pe::set_device_reset(w,0);
        th20_ios_set_logical_size(w.client_width,w.client_height);
        reset_clock();
    }
    void update() {
        if(!running||paused)return;
        if(report_engine_errors())return;
        auto& w=pe::window_state;auto& g=pe::graphics_state;
        if(w.quit_requested){shutdown();th20_ios_set_ready(false);th20_ios_set_stage("TH20 stopped");return;}
        if(!pe::device(g))throw std::runtime_error("TH20 lost its native graphics device");
        const auto status=pe::device(g)->TestCooperativeLevel();
        if(status==D3DERR_DEVICENOTRESET||pe::needs_device_reset(w)){reset_device();return;}
        if(status!=S_OK)throw std::runtime_error("Native graphics device is unavailable");
        input::before_frame();
        const int result=pe::run_present_paced_frame(w);
        pe::graphics_event_flags&=~8u;
        input::after_frame();
        if(result==2){
            finish_session();u::log_restart(pe::log_buffer);u::log_append(pe::log_buffer,pe::text_constants::restart);
            pe::graphics_event_flags&=0xffffff9fu;start_session();
        }else if(result!=0){shutdown();th20_ios_set_ready(false);th20_ios_set_stage("TH20 stopped");}
    }
    void failure(const char* operation,const char* reason) noexcept {
        running=false;
        input::clear();
        th20_ios_log("[engine] %s failed: %s",operation,reason);
        th20_ios_set_error(reason);
        th20_ios_flush_log();
    }
};
template<class F> bool guarded(Engine& e,const char* operation,F&& function) noexcept {
    try{function();return true;}
    catch(const std::exception& error){e.failure(operation,error.what());}
    catch(...){e.failure(operation,"Unknown exception in native TH20 runtime");}
    return false;
}
}

int main(int argc,char** argv) {
    // UIKit retains callback userdata; PMR containers also retain the listener.
    auto* engine=new Engine;
    TH20IOSCallbacks callbacks{};
    callbacks.struct_size=sizeof(callbacks);callbacks.userdata=engine;
    callbacks.initialize=[](void* p,const char* r,const char* s){auto& e=*static_cast<Engine*>(p);return guarded(e,"initialize",[&]{e.initialize(r,s);});};
    callbacks.update=[](void* p,double){auto& e=*static_cast<Engine*>(p);guarded(e,"frame",[&]{e.update();});};
    callbacks.key=[](void*,int key,bool down){input::key(key,down);};
    callbacks.touch=[](void* p,TH20IOSTouchPhase phase,uint64_t id,float x,float y,float dx,float dy){auto& e=*static_cast<Engine*>(p);guarded(e,"touch",[&]{input::touch(phase,id,x,y,dx,dy);});};
    callbacks.clear_input=[](void*){input::clear();};
    callbacks.cheat_code=[](void* p,const char* code){
        auto& e=*static_cast<Engine*>(p);if(!e.running)return -1;
        return th20::ios::apply_unlock_code(code);
    };
    callbacks.pause=[](void* p,bool paused){auto& e=*static_cast<Engine*>(p);guarded(e,"lifecycle",[&]{
        input::clear();e.paused=paused;pe::window_state.active=paused?0:1;
        if(!paused&&e.running)e.reset_clock();
        if(paused&&e.configuration_loaded)u::save_configuration(pe::graphics_state.configuration);
    });};
    callbacks.shutdown=[](void* p){auto& e=*static_cast<Engine*>(p);guarded(e,"shutdown",[&]{e.shutdown();});};
    return th20_ios_run_app(argc,argv,&callbacks);
}
