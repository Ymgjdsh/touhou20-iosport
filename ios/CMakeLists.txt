cmake_minimum_required(VERSION 3.20)
project(th20_ios_native LANGUAGES C CXX OBJCXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(TH20_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  message(FATAL_ERROR "Configure with -DCMAKE_SYSTEM_NAME=iOS and an Apple SDK")
endif()
if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "14.0")
  message(FATAL_ERROR "This native target requires iOS 14.0 or later")
endif()

add_library(th20_ios_options INTERFACE)
target_compile_definitions(th20_ios_options INTERFACE TH20_IOS=1)
target_compile_options(th20_ios_options INTERFACE -UNDEBUG -fno-fast-math -ffp-contract=off
  -fno-strict-aliasing -Wall -Wextra -Wno-invalid-offsetof)
# Xcode 14 lacks standard PMR and jthread. The compatibility code is app-local;
# it must not pull a newer libc++ ABI into an iOS 14 executable.
target_compile_options(th20_ios_options INTERFACE
  "-include${CMAKE_CURRENT_SOURCE_DIR}/compat/port_prefix.hpp")
file(GLOB module_paths LIST_DIRECTORIES true "${TH20_ROOT}/source_reconstruction/*")
set(module_includes)
foreach(path IN LISTS module_paths)
  if(IS_DIRECTORY "${path}")
    list(APPEND module_includes "${path}")
  endif()
endforeach()
target_include_directories(th20_ios_options INTERFACE compat src
  "${TH20_ROOT}/include" "${TH20_ROOT}/native_recovered"
  "${TH20_ROOT}/source_reconstruction" ${module_includes})

# Platform code can be compiled and checked independently of unported runtime
# layouts. It is never packaged as a replacement game or a successful IPA.
add_library(th20_ios_platform STATIC src/ios_host.mm src/ios_d3d9.mm src/ios_audio.mm src/ios_platform.mm src/ios_math.cpp)
target_link_libraries(th20_ios_platform PUBLIC th20_ios_options)
target_compile_options(th20_ios_platform PRIVATE -fobjc-arc)
target_link_libraries(th20_ios_platform PUBLIC
  "-framework UIKit" "-framework Foundation" "-framework QuartzCore"
  "-framework OpenGLES" "-framework ImageIO" "-framework CoreGraphics" "-framework CoreText" "-framework AVFoundation")
add_library(th20_ios_archive STATIC "${TH20_ROOT}/source_reconstruction/archive/archive.cpp")
target_link_libraries(th20_ios_archive PUBLIC th20_ios_options)

add_executable(th20_ios_host_probe MACOSX_BUNDLE EXCLUDE_FROM_ALL tests/host_probe.mm tests/renderer_probe.cpp)
target_link_libraries(th20_ios_host_probe PRIVATE th20_ios_platform)
target_compile_options(th20_ios_host_probe PRIVATE -fobjc-arc)
set_target_properties(th20_ios_host_probe PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/tests/host_probe.Info.plist"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.th20.port.platform-diagnostics"
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")

# Explicit engine audit target: retains assertions in modules whose raw memory
# accesses have not yet been ported. No assertion-disabling macro is allowed.
file(READ "${TH20_ROOT}/SOURCE_BUILD_VERIFICATION.json" manifest)
string(JSON source_count LENGTH "${manifest}" compiled_sources)
math(EXPR source_last "${source_count} - 1")
set(engine_sources)
foreach(index RANGE 0 ${source_last})
  string(JSON relative GET "${manifest}" compiled_sources ${index})
  if(relative MATCHES "^source_reconstruction/.*\\.(cpp|c|mm)$" AND
     NOT relative MATCHES "/(link_probe|diagnostics)/" AND
     NOT relative MATCHES "/program_entry/(program_entry|winmain)\\.cpp$")
    list(APPEND engine_sources "${TH20_ROOT}/${relative}")
  endif()
endforeach()
list(APPEND engine_sources
  "${TH20_ROOT}/src/binary.cpp" "${TH20_ROOT}/src/ecl.cpp"
  "${TH20_ROOT}/source_reconstruction/text_renderer/native_font.cpp")
add_library(th20_ios_engine STATIC EXCLUDE_FROM_ALL ${engine_sources})
target_link_libraries(th20_ios_engine PRIVATE th20_ios_options)

# Native entry owns initialization and advances the recovered schedulers from
# UIKit's display callback. Windows message-loop sources never enter this app.
add_executable(th20_ios_game MACOSX_BUNDLE EXCLUDE_FROM_ALL
  src/ios_engine.cpp src/ios_game_input.cpp src/ios_unlock.cpp src/ios_cheats.cpp)
target_link_libraries(th20_ios_game PRIVATE th20_ios_platform)
add_dependencies(th20_ios_game th20_ios_engine)
# Xcode hashes object names when modules share filenames. Let its archive
# target resolve those names; passing TARGET_OBJECTS can reference stale names.
# Force-load also retains the recovered global constructors and registrations.
target_link_options(th20_ios_game PRIVATE "LINKER:-force_load,$<TARGET_FILE:th20_ios_engine>")
set_target_properties(th20_ios_game PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.th20.native"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
set(TH20_ASSET_DIR "" CACHE PATH "Directory containing the original TH20 data archives")
add_executable(th20_ios_game_smoke MACOSX_BUNDLE EXCLUDE_FROM_ALL
  src/ios_engine.cpp src/ios_game_input.cpp src/ios_unlock.cpp src/ios_cheats.cpp tests/game_smoke.mm tests/mobile_settings_probe.mm tests/dev_probe.mm)
target_compile_options(th20_ios_game_smoke PRIVATE -fobjc-arc)
target_link_libraries(th20_ios_game_smoke PRIVATE th20_ios_platform)
add_dependencies(th20_ios_game_smoke th20_ios_engine)
target_link_options(th20_ios_game_smoke PRIVATE "LINKER:-force_load,$<TARGET_FILE:th20_ios_engine>")
set_target_properties(th20_ios_game_smoke PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.th20.native.smoke"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
foreach(icon IN ITEMS TH20Icon120.png TH20Icon180.png TH20Icon76.png
    TH20Icon152.png TH20Icon167.png TH20Icon1024.png)
  set(icon_path "${CMAKE_CURRENT_SOURCE_DIR}/icons/${icon}")
  set_source_files_properties("${icon_path}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  target_sources(th20_ios_game PRIVATE "${icon_path}")
  target_sources(th20_ios_game_smoke PRIVATE "${icon_path}")
endforeach()
if(TH20_ASSET_DIR)
  foreach(asset IN ITEMS th20.dat thbgm.dat)
    if(NOT EXISTS "${TH20_ASSET_DIR}/${asset}")
      message(FATAL_ERROR "Missing game asset: ${TH20_ASSET_DIR}/${asset}")
    endif()
    target_sources(th20_ios_game PRIVATE "${TH20_ASSET_DIR}/${asset}")
    target_sources(th20_ios_game_smoke PRIVATE "${TH20_ASSET_DIR}/${asset}")
    set_source_files_properties("${TH20_ASSET_DIR}/${asset}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  endforeach()
endif()
