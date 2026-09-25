cmake_minimum_required(VERSION 3.20)
project(th20_native_foundation LANGUAGES CXX)
enable_testing()
add_executable(th20_native_foundation_tests foundation_tests.cpp ../core_scheduler/scheduler.cpp ../game_session/session.cpp)
target_compile_features(th20_native_foundation_tests PRIVATE cxx_std_17)
target_compile_definitions(th20_native_foundation_tests PRIVATE TH20_IOS=1)
target_include_directories(th20_native_foundation_tests PRIVATE ../../ios/compat)
if(MSVC)
  target_compile_options(th20_native_foundation_tests PRIVATE /W4 /fp:strict)
else()
  target_compile_options(th20_native_foundation_tests PRIVATE -Wall -Wextra -fno-fast-math -ffp-contract=off)
endif()
add_test(NAME th20_native_foundation_tests COMMAND th20_native_foundation_tests)
if(APPLE)
  add_executable(th20_native_pmr_probe pmr_probe.cpp)
  target_compile_features(th20_native_pmr_probe PRIVATE cxx_std_17)
  target_compile_definitions(th20_native_pmr_probe PRIVATE TH20_IOS=1)
  target_include_directories(th20_native_pmr_probe PRIVATE ../../ios/compat ../core_scheduler)
  add_test(NAME th20_native_pmr_probe COMMAND th20_native_pmr_probe)
  add_executable(th20_native_sprite_tests
    sprite_native_tests.cpp
    ../sprite_renderer/animation.cpp
    ../sprite_renderer/binding.cpp
    ../sprite_renderer/pool.cpp
    ../sprite_renderer/buffers.cpp
    ../sprite_renderer/render_state.cpp
    ../sprite_renderer/loading_interrupt.cpp
    ../sprite_renderer/dispatch.cpp
    ../core_scheduler/scheduler.cpp
    ../runtime_core/runtime_core.cpp)
  target_compile_features(th20_native_sprite_tests PRIVATE cxx_std_20)
  target_compile_definitions(th20_native_sprite_tests PRIVATE TH20_IOS=1)
  target_include_directories(th20_native_sprite_tests PRIVATE ../../ios/compat ../../ios/src ../core_scheduler)
  target_compile_options(th20_native_sprite_tests PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/../../ios/compat/port_prefix.hpp
    -fno-fast-math -ffp-contract=off -fno-strict-aliasing)
  target_link_options(th20_native_sprite_tests PRIVATE -Wl,-dead_strip)
  add_test(NAME th20_native_sprite_tests COMMAND th20_native_sprite_tests)
endif()
