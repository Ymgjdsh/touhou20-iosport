cmake_minimum_required(VERSION 3.20)
project(th20_scene_native_checks LANGUAGES CXX)
enable_testing()
if(NOT APPLE)
  message(FATAL_ERROR "These native ABI checks use Apple's libc++ and compatibility headers")
endif()
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
add_executable(th20_scene_native_tests
  ../scene_native_tests.cpp ../../stone_menu/cursor.cpp ../../trophy_system/queue.cpp
  ../../replay_system/records.cpp ../../core_scheduler/scheduler.cpp
  ../../game_session/session.cpp ../../platform_services/configuration.cpp)
target_compile_features(th20_scene_native_tests PRIVATE cxx_std_20)
target_compile_definitions(th20_scene_native_tests PRIVATE TH20_IOS=1)
target_include_directories(th20_scene_native_tests PRIVATE
  "${ROOT}/ios/compat" "${ROOT}/ios/src" "${ROOT}/include"
  "${ROOT}/native_recovered" ../../core_scheduler)
target_compile_options(th20_scene_native_tests PRIVATE
  -include "${ROOT}/ios/compat/port_prefix.hpp"
  -fno-fast-math -ffp-contract=off -fno-strict-aliasing -g)
target_link_options(th20_scene_native_tests PRIVATE -Wl,-dead_strip)
option(TH20_SCENE_SANITIZERS "Check native menu/replay memory accesses" ON)
if(TH20_SCENE_SANITIZERS)
  target_compile_options(th20_scene_native_tests PRIVATE -fsanitize=address,undefined)
  target_link_options(th20_scene_native_tests PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME scene_native_checks COMMAND th20_scene_native_tests)
