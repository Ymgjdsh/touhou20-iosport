# Enemy native-layout refactor regression validation

Validated on 2026-09-25 using MSVC 19.44, Windows x86 Release, `/fp:strict /arch:SSE2`, with the original x86 layout assertions enabled. Build and reports are isolated in `build-native-reports/enemy-win32`.

| Test | Result |
| --- | --- |
| `th20_enemy_cpu_compare` | 1,254,134 comparisons, 0 failed |
| `th20_enemy_frame_tests` | 7,682 assertions passed |
| `th20_enemy_resource_tests` | 3,304 assertions passed |

The CPU test executes the original EXE only as an isolated, read-only comparison oracle. Its verified SHA-256 is `a274b45fe6ec53511718bb328c2ff169a74e67f95d1b0c74d97d348b955a0897`. Every CPU coverage count matches the checked-in `enemy_cpu_validation.json` baseline. The archive test reads `G:/touhou20-web-main/th20/th20.dat`, SHA-256 `9db8d7c43fbacec95614163d4c3e1d254e82169f8550177ee849831130590494`.

Test fixtures now address the seven renamed Enemy state members explicitly instead of indexing beyond the shortened `fields_2c8` array. Random initialization still consumes the original ten words in the original order, and paired shot fixtures copy all ten values. This preserves the existing comparison inputs and coverage. No production behavior changes were needed for this validation.

The frame test's standalone target required the real `player_entity/collision.cpp` and `damage_regions/geometry.cpp` after the Enemy damage refactor began using the named Player position helper. The gameplay CMake target now links those production implementations. The initial missing-symbol link log is preserved as `build.log`; the successful build is `build-followup.log`.

The frame test includes protected retired object pages and 512 deletion masks with and without nested queries. The archive test covers all seven stage script sets: 145, 153, 167, 150, 134, 141 and 168 subroutines respectively. Full source hashes, coverage descriptions, executables and logs are retained in `cpu-report.json`, `frame-report.json`, `resource-report.json` and `validation-manifest.json` under the isolated report directory. No hashed source changed between the CPU build and manifest capture.

This demonstrates preserved Windows x86 behavior in the existing test domains after the Enemy native-layout refactor. It does not establish ARM64 runtime behavior, rendered gameplay, device performance, or complete-game equivalence. Recorded service boundaries and excluded domains remain documented in each JSON report.
