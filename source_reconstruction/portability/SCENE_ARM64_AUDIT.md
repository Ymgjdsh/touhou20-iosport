# Scene and menu native layout audit

Date: 2026-09-25 (Asia/Shanghai).

This covers the 166 shipping translation units in title, startup, options,
pause, ending, stage clear/completion, stone menu, help, key configuration,
replay, trophy, notice, overlay weapons, HUD, screen effects, small score,
and progress persistence. It is compile and component behavior evidence,
not proof of whole-game or iPhone behavior.

## Runtime and serialized storage

ARM64 owners now have explicit size and field-offset assertions measured
with Xcode's `arm64-apple-ios14.0` compiler. Original 32-bit assertions stay
in the other branch. The compiler record dump and extracted field table are
`build-native-reports/scene-layouts.txt` and `scene-layouts.json`.

Replay FileHeader, UserHeader, StageRecord and InputFrame remain packed at
their original disk sizes. Native RecordingChunk, PlaybackCursor and ReplayInf
use normal pointer alignment. Screen effects likewise use native base alignment.
Menu cursor containers, polymorphic scene owners, native save-buffer owners,
and weapon subclasses are verified separately from serialized data.

## Runtime access fixes

- Trophy deque map allocation, initialization and release use pointer width;
  PMR deallocation alignment matches allocation for both map and proxy.
- Overlay options store callbacks in the pointer-width named fields and access
  their actual Context member. The anchored option copies the two original
  coordinate bit patterns from the named Player field.
- HUD boss health, flags and timer accesses use the recovered EnemyState members.
  Spell and pause fields use their actual owners. Help/options transitions,
  stone menu status and player position also use named native fields.
- Stage-completion replay access no longer applies x86 owner/array strides to
  native pointers. Title name entry resolves its field with offsetof; the name
  byte span has a separate assertion.
- The original malformed replay-save text's x86 pointer word is explicitly
  truncated to 32 bits before emulating its original vararg word consumption.
- Native replay enumeration and replay/score writes use filesystem/stream
  operations. iOS retains the real worker threads. Help image loading calls
  the native image implementation directly.
- The native EffectInf hit-effect cursor has independent integer storage;
  advancing it never overwrites the low word of files[4]. Its original
  old-index return/new-index test order remains intact.
- CardInf and special-state list owners have native layout assertions.
  Spell portrait lookups index StageDefinition.fields_58 without applying
  the original pointer-dependent owner offset.
- Apple ARM64's 32-byte std::function is checked separately from Intel Apple's
  48-byte type in text_renderer/text.hpp.
- Background ScriptState and Background have explicit native layout assertions;
  packed STD Header/Object/Primitive/Instance/Instruction keep their original
  0x90/0x30/8/16/8-byte sizes. Native mesh pointers occupy separate pointer-width
  storage, and creation, distortion updates, reset and cleanup use that storage.
- Native STD loading resolves the serialized four-byte object offsets into a
  separately allocated native pointer table, freed by Background's destructor.
  It never writes eight-byte pointers into the resource's four-byte offset table.
  The loader checks header/table bounds and embedded filename termination.
  Background's header explicitly includes its animation member definition.

## Validation

`build-native-reports/scene-complete.json` is the earlier real iOS ARM64 compile
snapshot: all 166 originally assigned scene translation units passed. Expanded
coverage of card, special-state, effects and player modules reached 230/232.
The remaining card_system/finish.cpp and card_system/update.cpp background
dependencies are now resolved: `build-native-reports/background-complete.json`
records all 11 background and seven card translation units passing the real
arm64-apple-ios14.0 compiler (18/18). No assertion was disabled and no behavior
was replaced by a stub.

`source_reconstruction/portability/scene_tests/CMakeLists.txt` defines the
native component test target. The same component sources were compiled and
executed on the remote Intel Mac with AddressSanitizer and UndefinedBehaviorSanitizer,
and also as a real Windows x86 MSVC executable. Both executions passed:

- 120,000 mixed trophy-queue operations checked against std::deque;
- allocation memory poisoned with 0xa5, with exact size/alignment deallocation checks;
- menu forward/backward exclusion, clamp, snapshot and selection checks;
- 36,000 replay input frames, rollover/bounds checks, 1,200 FPS samples and
  playback active/inactive rewind.

The checked-in CMake target also built and passed CTest (1/1), recorded in
`build-native-reports/scene-cmake-test.log`. The native run is `build-native-reports/scene-native-tests-run.log`; the
Windows x86 run is `build-native-reports/scene-x86/run.log`. All 166 scene
translation units also passed MSVC x86 syntax checks, recorded in
`build-native-reports/scene-x86/compile-audit.json`. Native sanitizer
execution used macOS x86_64; ARM64 received compilation validation, not device
execution. Full title/HUD rendering, replay playback and end-to-end saves
remain integration validation tasks.

The additional `portability/background_native_tests.cpp` was compiled and run
against the real archive on Windows x86, then against those same extracted STD
bytes on Intel macOS with AddressSanitizer and UndefinedBehaviorSanitizer. Both
passed all ten original STD resources: 89 object references and 761 primitives.
The test checks that offset resolution leaves resource bytes unchanged, rejects
invalid indices, truncated tables and invalid offsets, initializes all eight
embedded animation self-links correctly, and round-trips both mesh pointers.
The native test requires pointer values above UINT32_MAX and checks that the
numeric script words are not overwritten by native pointer storage.

Evidence: `build-native-reports/background-native-tests-{build,run}.log` and
`build-native-reports/background-x86/{build,run}.log`. All 18 background/card
translation units also pass the original Windows x86 layout assertions, as
recorded in `build-native-reports/background-x86/compile-audit.json`. These tests
cover resource decoding and state layout, not full Background lifecycle/rendering
integration or ARM64 device execution.
