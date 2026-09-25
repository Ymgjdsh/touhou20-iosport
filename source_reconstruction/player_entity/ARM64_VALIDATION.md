# Player ARM64 migration validation

The resumed migration restores the ten scalar words at `Option +0xf4` that
were accidentally replaced by a duplicate callback declaration. Native
callbacks, contexts, shot registries, animation-file addresses and SHT-buffer
addresses retain 64-bit storage. The original Win32 layout assertions remain;
ARM64 assertions separately verify `Option` (0x140), `Shot` (0x160),
`ShotController` (0x16248) and `Player` (0x18790), including the player's
resource, feedback, shot-pool, context and service boundaries.

The iOS shot initialization, update and hit bridges now use the native
argument lists, with compile-time signature checks. The x86 bridges still
consume their register-placeholder argument. Leaving that placeholder in an
ARM64 initializer would read the frame from the wrong argument register;
the hit bridge would likewise shift its pointer arguments.

Player-side accesses to the HUD dialogue, special-state active flag, stone
menu animation file and enemy identifier now follow their owning type or
accessor on iOS. Numeric `game_session::Player` records and SHT records keep
their original on-disk layout. Constructor byte ranges use member offsets;
the original intentionally untouched padding is preserved.

Native SHT tables retain relative 32-bit offsets and never contain truncated
native addresses. Pattern lookup rejects indices outside the count. Validation
rejects offsets at/past the resource end and incomplete live first records,
while preserving the actual single-byte negative-period empty-pattern
terminator. The full Win32 oracle found this real-resource boundary during
development; the native memory test now has a regression case for it.

## Verification

`ios/tests/player/check_native.py`, run on the isolated Mac checkout, compiles
all 37 production player translation units and the probe with Apple's
`arm64-apple-ios14.0` SDK. It retains every layout assertion and records each
source/header hash and any dependency error in
`ios/tests/player/results/validation.json`.

The same script executes the real Option/Shot/ShotController/Feedback
constructors and SHT validator/lookup on the Intel Mac with AddressSanitizer
and UndefinedBehaviorSanitizer. It tests all 256 pool elements, preserved
padding, list anchors, addresses above 4 GiB, native callback storage,
unchanged resource bytes, sentinels and malformed bounds. Execution logs are
in `ios/tests/player/results/memory-validation.log`.

The MSVC Win32 player oracle was rebuilt against the read-only verified
`th20.exe` and passes **1,032,161 checks, zero failures**. The result and
build-time hashes are in `ios/tests/player/win32-cpu-validation.json`. Two
old oracle fixtures were updated to refer to the already migrated typed
context and named callback members, preserving their RNG draw order and
comparison coverage. This result includes both actual archive SHT resources.

The 2026-09-25 run with Apple clang 14 passed **37/38 compilation units**
(including the probe) and **659 sanitizer checks**. The one compile failure
is `events_adapter.cpp`, blocked by the still-original `Entry` and
`Controller` layout assertions in `special_state/special.hpp`. No player
assertion was disabled to obtain this result.

These are module checks. macOS sanitizer execution does not establish ARM64
gameplay behavior, iOS device performance, complete linking, or a playable
game. The compile report explicitly lists any remaining foreign-header
blockers. The effect registry cursor still uses the original four-byte alias
inside `effects::Controller::files[4]`; it should receive a dedicated numeric
member when the effect-owner runtime representation is finalized.
