#!/bin/bash
set -euo pipefail
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
export PATH="/usr/local/bin:/Applications/CMake.app/Contents/bin:$PATH"
root=$(cd "$(dirname "$0")/.." && pwd)
sdk=${TH20_SDK:-iphoneos}
architecture=arm64
if [ "$sdk" = iphonesimulator ]; then architecture=$(uname -m); fi
target=${TH20_TARGET:-th20_ios_game}
configuration=${TH20_CONFIGURATION:-Debug}
assets=${TH20_ASSET_DIR:-"$root/../assets"}
build="$root/build-native-$sdk"
mkdir -p "$build"
cmake -S "$root/ios" -B "$build" -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT="$sdk" \
  -DCMAKE_OSX_ARCHITECTURES="$architecture" -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DTH20_ASSET_DIR="$assets" \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build "$build" --config "$configuration" --target "$target" --parallel 4
