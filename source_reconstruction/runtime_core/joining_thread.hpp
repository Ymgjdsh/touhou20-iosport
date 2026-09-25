#pragma once
#include <thread>
#include <utility>
#include <type_traits>

namespace th20::source::runtime {
#if defined(TH20_IOS)
// Xcode 14 has std::thread but no std::jthread. All recovered worker entry
// points use Worker::close_requested; none accept or use std::stop_token.
// Preserve the joining lifetime and move-assignment contract with a real
// native thread, without placing a substitute class in namespace std.
class JoiningThread {
    std::thread thread_;
public:
    JoiningThread() noexcept=default;
    template<class Function,class... Arguments,
             std::enable_if_t<!std::is_same_v<std::decay_t<Function>,JoiningThread>,int> =0>
    explicit JoiningThread(Function&& function,Arguments&&... arguments)
        :thread_(std::forward<Function>(function),std::forward<Arguments>(arguments)...) {}
    ~JoiningThread() {if(thread_.joinable())thread_.join();}
    JoiningThread(JoiningThread&&)=default;
    JoiningThread& operator=(JoiningThread&& other) noexcept {
        if(this!=&other) {if(thread_.joinable())thread_.join();thread_=std::move(other.thread_);}return *this;
    }
    JoiningThread(const JoiningThread&)=delete;
    JoiningThread& operator=(const JoiningThread&)=delete;
    bool joinable() const noexcept {return thread_.joinable();}
    void join() {thread_.join();}
    void detach() {thread_.detach();}
    std::thread::id get_id() const noexcept {return thread_.get_id();}
};
#else
using JoiningThread=std::jthread;
#endif
}
