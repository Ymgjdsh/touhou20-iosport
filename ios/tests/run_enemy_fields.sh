#!/bin/bash
set -euo pipefail
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
output=${1:-"$root/build-native-reports/enemy-fields-test"}
mkdir -p "$(dirname "$output")"
includes=(-Iios/compat -Iios/src -Isource_reconstruction -Inative_recovered -Iinclude)
for directory in source_reconstruction/*/; do includes+=(-I"$directory"); done
xcrun clang++ -std=c++20 -DTH20_IOS=1 -include ios/compat/port_prefix.hpp "${includes[@]}" \
  -fno-fast-math -ffp-contract=off \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  ios/tests/enemy_fields_test.cpp source_reconstruction/gameplay/enemy_state.cpp -o "$output"
"$output"
