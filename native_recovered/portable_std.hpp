#pragma once
#include <atomic>
#include <bit>
#include <cstddef>
#include <type_traits>

namespace th20::portable {
#if defined(TH20_IOS)
// Xcode 14 Clang supports these operations; its libc++ lacks the C++20 facades.
// Keep the implementation in our namespace and require the original type rules.
template<class To, class From>
constexpr To bit_cast(const From& value) noexcept {
    static_assert(sizeof(To)==sizeof(From));
    static_assert(std::is_trivially_copyable_v<To> && std::is_trivially_copyable_v<From>);
    return __builtin_bit_cast(To,value);
}
inline constexpr int atomic_order(std::memory_order order) noexcept {
    switch(order) {
    case std::memory_order_relaxed:return __ATOMIC_RELAXED;
    case std::memory_order_consume:return __ATOMIC_CONSUME;
    case std::memory_order_acquire:return __ATOMIC_ACQUIRE;
    case std::memory_order_release:return __ATOMIC_RELEASE;
    case std::memory_order_acq_rel:return __ATOMIC_ACQ_REL;
    default:return __ATOMIC_SEQ_CST;
    }
}
template<class T> class atomic_ref {
    static_assert(std::is_integral_v<T> && (sizeof(T)==4 || sizeof(T)==8));
    T* address_;
public:
    explicit atomic_ref(T& value) noexcept:address_(&value) {
        // Native runtime members must have their natural alignment. Packed file
        // fields are never valid cross-thread flags.
        if(reinterpret_cast<std::size_t>(address_)%alignof(T)) __builtin_trap();
    }
    T load(std::memory_order order=std::memory_order_seq_cst) const noexcept {
        return __atomic_load_n(address_,atomic_order(order));
    }
    void store(T value,std::memory_order order=std::memory_order_seq_cst) const noexcept {
        __atomic_store_n(address_,value,atomic_order(order));
    }
    T fetch_or(T value,std::memory_order order=std::memory_order_seq_cst) const noexcept {
        return __atomic_fetch_or(address_,value,atomic_order(order));
    }
    T fetch_add(T value,std::memory_order order=std::memory_order_seq_cst) const noexcept {
        return __atomic_fetch_add(address_,value,atomic_order(order));
    }
    T fetch_and(T value,std::memory_order order=std::memory_order_seq_cst) const noexcept {
        return __atomic_fetch_and(address_,value,atomic_order(order));
    }
};
template<class T> atomic_ref(T&)->atomic_ref<T>;
#else
using std::bit_cast;
using std::atomic_ref;
#endif
}
