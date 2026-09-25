#include "../core_scheduler/scheduler.hpp"
#include "../game_session/session.hpp"
#include "../../native_recovered/native_core.hpp"
#include "../../native_recovered/scalar_sse.hpp"
#include "../runtime_core/joining_thread.hpp"
#include "../runtime_core/worker.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <future>

namespace s=th20::recovered::scalar_sse;
namespace scheduler=th20::source::scheduler;
namespace session=th20::source::game_session;
namespace {
void require(bool result,const char* message) { if(!result)throw std::runtime_error(message); }
std::uint32_t random_word(std::uint32_t& state) { state^=state<<13;state^=state>>17;state^=state<<5;return state; }
std::uint64_t random_wide(std::uint32_t& state) {const std::uint64_t high=random_word(state),low=random_word(state);return (high<<32)|low;}
std::uint64_t digest=1469598103934665603ull;
void record(std::uint64_t word) { digest^=word;digest*=1099511628211ull; }
void math_results() {
    constexpr std::uint32_t special[]{0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,
        0x3f800000u,0xbf800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,
        0x7fc12345u,0xffc54321u,0x7f812345u,0xff854321u,0x4f000000u,0xcf000001u};
    std::uint32_t rng=0x12345678u;
    for(unsigned i=0;i<100000;++i) {
        float x=s::bits<float>(i<324?special[i/18]:random_word(rng));
        float y=s::bits<float>(i<324?special[i%18]:random_word(rng));
        const auto a=s::_mm_set_ss(x),b=s::_mm_set_ss(y);
        const auto native_a=_mm_set_ss(x),native_b=_mm_set_ss(y);
        const s::Float4 actual[]{s::_mm_add_ss(a,b),s::_mm_sub_ss(a,b),s::_mm_mul_ss(a,b),s::_mm_div_ss(a,b)};
        const float expected[]{_mm_cvtss_f32(_mm_add_ss(native_a,native_b)),_mm_cvtss_f32(_mm_sub_ss(native_a,native_b)),
            _mm_cvtss_f32(_mm_mul_ss(native_a,native_b)),_mm_cvtss_f32(_mm_div_ss(native_a,native_b))};
        for(unsigned op=0;op<4;++op) {
            const auto got=s::bits<std::uint32_t>(actual[op].lane[0]);
            const auto wanted=s::bits<std::uint32_t>(expected[op]);
            if(got!=wanted) {
                std::fprintf(stderr,"SSE mismatch i=%u op=%u x=%08x y=%08x got=%08x expected=%08x\n",i,op,s::bits<std::uint32_t>(x),s::bits<std::uint32_t>(y),got,wanted);
                throw std::runtime_error("binary32 arithmetic differs from native SSE");
            }
            record(got);
        }
        require(s::_mm_cvttss_si32(a)==_mm_cvttss_si32(native_a),"binary32 truncation differs from native SSE");
        const auto integer=s::bits<std::int32_t>(random_word(rng));
        require(s::bits<std::uint32_t>(s::_mm_cvtsi32_ss(a,integer).lane[0])==
                s::bits<std::uint32_t>(_mm_cvtss_f32(_mm_cvtsi32_ss(native_a,integer))),"integer to float rounding differs from native SSE");
        record(static_cast<std::uint32_t>(s::_mm_cvttss_si32(a)));
    }
    const double boundary[]{-2147483649.0,-2147483648.75,-2147483648.0,-1.999,0.,1.999,2147483647.99,2147483648.0,
        std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()};
    for(double value:boundary) require(s::_mm_cvttsd_si32(s::_mm_set_sd(value))==_mm_cvttsd_si32(_mm_set_sd(value)),"binary64 conversion boundary mismatch");
    constexpr std::uint64_t special_double[]{0ull,0x8000000000000000ull,1ull,0x000fffffffffffffull,0x0010000000000000ull,
        0x3ff0000000000000ull,0x7fefffffffffffffull,0x7ff0000000000000ull,0xfff0000000000000ull,
        0x7ff8123456789abcull,0xfff854321abcdef0ull,0x7ff0123456789abcull};
    for(unsigned i=0;i<100000;++i) {
        const auto xr=i<144?special_double[i/12]:random_wide(rng);
        const auto yr=i<144?special_double[i%12]:random_wide(rng);
        const double x=s::bits<double>(xr),y=s::bits<double>(yr);
        const auto a=s::_mm_set_sd(x),b=s::_mm_set_sd(y);
        const auto native_a=_mm_set_sd(x),native_b=_mm_set_sd(y);
        const s::Double2 actual[]{s::_mm_add_sd(a,b),s::_mm_sub_sd(a,b),s::_mm_mul_sd(a,b),s::_mm_div_sd(a,b)};
        const double expected[]{_mm_cvtsd_f64(_mm_add_sd(native_a,native_b)),_mm_cvtsd_f64(_mm_sub_sd(native_a,native_b)),
            _mm_cvtsd_f64(_mm_mul_sd(native_a,native_b)),_mm_cvtsd_f64(_mm_div_sd(native_a,native_b))};
        for(unsigned op=0;op<4;++op) {
            require(s::bits<std::uint64_t>(actual[op].lane[0])==s::bits<std::uint64_t>(expected[op]),"binary64 arithmetic differs from native SSE");
            record(s::bits<std::uint64_t>(actual[op].lane[0]));
        }
        require(s::bits<std::uint32_t>(s::_mm_cvtsd_ss(s::_mm_setzero_ps(),a).lane[0])==
            s::bits<std::uint32_t>(_mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(),native_a))),"binary64 to binary32 mismatch");
        require(s::_mm_cvttsd_si32(a)==_mm_cvttsd_si32(native_a),"binary64 truncation differs from native SSE");
    }
    const s::Float4 high{{1,2,3,4}};
    const auto sum=s::_mm_add_ss(high,s::_mm_set_ss(9));
    require(sum.lane[0]==10&&sum.lane[1]==2&&sum.lane[2]==3&&sum.lane[3]==4,"scalar operation changed high lanes");
    const auto neg=s::_mm_xor_ps(s::_mm_set_ss(0.f),s::_mm_castsi128_ps(s::_mm_set_epi32(0,0,0,std::numeric_limits<int>::min())));
    require(s::bits<std::uint32_t>(neg.lane[0])==0x80000000u,"sign-bit operation lost negative zero");
    // Captured only after comparison with Intel SSE hardware on the build Mac.
    // On ARM this independent oracle remains meaningful even though the native
    // intrinsic names are forwarded to scalar_sse by the platform headers.
    require(digest==0x9924b16d9e2c2027ull,"arithmetic output differs from the Intel SSE reference digest");
}
struct CallbackData {
    scheduler::State* state;
    scheduler::Environment* environment;
    std::vector<int>* order;
    scheduler::Node* remove_next=nullptr;
    int id;
    int result=1;
};
int callback(void* raw) {
    auto& d=*static_cast<CallbackData*>(raw);d.order->push_back(d.id);
    if(d.remove_next) { scheduler::remove(*d.state,*d.environment,d.remove_next);d.remove_next=nullptr; }
    return d.result;
}
void scheduler_results() {
    scheduler::State state{};scheduler::initialize_state(state);scheduler::Environment environment;
    std::vector<int> order;
    CallbackData first{&state,&environment,&order,nullptr,1},second{&state,&environment,&order,nullptr,2},third{&state,&environment,&order,nullptr,3};
    scheduler::register_callback(state,environment,30,callback,&third,false,true);
    first.remove_next=scheduler::register_callback(state,environment,20,callback,&second,false,true);
    scheduler::register_callback(state,environment,10,callback,&first,false,true);
    scheduler::dispatch_update(state,environment);
    require(order==std::vector<int>({1,3}),"callback deletion did not repair the active iterator");
    require(environment.dispatch_depth==0,"scheduler left a tracked dispatch lock entered");
    order.clear();first.result=0;
    scheduler::dispatch_update(state,environment);
    require(order==std::vector<int>({1,3}),"self-removal changed dispatch order");
    order.clear();scheduler::dispatch_update(state,environment);
    require(order==std::vector<int>({3}),"removed callback was invoked again");
    scheduler::shutdown_chains(state,environment);
    require(!state.update.sentinel.next&&!state.draw.sentinel.next,"shutdown left a live callback");
    require(state.update.tail==&state.update.sentinel,"shutdown did not restore the list tail");
}
void session_results() {
    session::Session value;
    require(value.player_table.players[0].fields_30[1]==400,"initial player power changed");
    require(value.player_table.players[1].fields_b4[8]==2,"second player defaults changed");
    value.contexts[1].current_player=&value.player_table.players[1];
    require(value.contexts[1].current_player==&value.player_table.players[1],"native context pointer was truncated");
    value.contexts[0].objects_04[7]=&value.contexts[1];
    require(value.contexts[0].objects_04[7]==&value.contexts[1],"native object pointer was truncated");
    session::set_best_score(value,0xfedcba9876543210ull);
    session::set_playtime_origin(value,12345.125);
    session::set_credits(value,-7);
    require(session::best_score(value)==0xfedcba9876543210ull,"best score lost a high word");
    require(session::playtime_origin(value)==12345.125,"playtime origin was numerically converted");
    require(session::remaining_credits(value)==-7,"credits lost its signed representation");
    require(value.contexts[1].current_player==&value.player_table.players[1]&&value.contexts[0].objects_04[7]==&value.contexts[1],"session scalar access overwrote an expanded Context");
    session::set_flag0(value,3);session::set_flag1(value,1);require(value.flags==3,"session flag semantics changed");
    session::add_continue_count(value.player_table,100);require(value.player_table.continue_count==9,"continue upper clamp changed");
    session::add_continue_count(value.player_table,-100);require(value.player_table.continue_count==0,"continue lower clamp changed");
    session::construct_session(value);require(value.contexts[1].current_player==nullptr&&value.contexts[0].objects_04[7]==nullptr,"session reset retained pointers");
}
void worker_results() {
    using th20::source::runtime::JoiningThread;
    std::atomic<unsigned> completed{0};
    {JoiningThread worker([&]{completed.fetch_add(1);});require(worker.joinable(),"native worker was not launched");}
    require(completed.load()==1,"worker destructor did not join");
    {JoiningThread first([&]{completed.fetch_add(1);});JoiningThread second([&]{completed.fetch_add(1);});first=std::move(second);require(!second.joinable(),"thread move retained ownership");}
    require(completed.load()==3,"move-assignment failed to join both workers");
    std::promise<void> done;auto future=done.get_future();
    {JoiningThread worker([&]{done.set_value();});worker.detach();require(!worker.joinable(),"detach retained ownership");}
    future.get();
}
}
int main() {
    try { math_results();scheduler_results();session_results();worker_results();std::printf("TH20 native foundation PASS pointer_bits=%zu arithmetic_digest=%016llx\n",sizeof(void*)*8,static_cast<unsigned long long>(digest));return 0; }
    catch(const std::exception& error) {std::fprintf(stderr,"FAIL %s\n",error.what());return 1;}
}
