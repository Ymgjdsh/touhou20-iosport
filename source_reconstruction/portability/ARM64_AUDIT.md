# TH20 native ABI audit and verified foundations

Date: 2026-09-25. This is an implementation record, not a full-game acceptance report.

## Implemented and tested

- `core_scheduler`: native 64-bit linked lists, callbacks, iterators and state. Original 32-bit assertions remain for Windows/Web builds; iOS has explicit checked 64-bit sizes and offsets. No callback order or return-code behavior was changed.
- `game_session`: `Context` and `Session` now use native pointers on iOS. Pointer-free `Player` and `PlayerTable` retain every original binary-layout assertion. Raw session reads at original +0x60 (score), +0x78 (credits), and +0x2b0 (clock origin) in loading, pause and playtime code were replaced with named accessors. The generic `player_state` byte view rejects a native `Session` at compile time.
- `runtime_core`: checked native sizes for PMR allocation owner and log. Small allocations use ordinary aligned-enough allocation; larger requested alignments use aligned allocation.
- `runtime_core/JoiningThread`: actual native `std::thread`, joined on destruction and move assignment. All ten production `std::jthread(...)` construction sites now use this local type. Windows/Web still alias it to `std::jthread`. The recovered code does not use `stop_token` or `request_stop`; its existing `Worker::close_requested` atomic remains its cancellation protocol.
- `native_recovered/scalar_sse.hpp`: native scalar equivalents for the SSE/SSE2 arithmetic and conversions used by gameplay. Preserves scalar upper lanes, signed zero bit operations, x86 NaN result selection, and integer-indefinite (`INT_MIN`) results for invalid truncating conversions. No emulated game instructions, original EXE code, WASM, or browser is involved.
- `sprite_renderer/anm_vm.cpp`: iOS invokes the recovered animation callback through its real virtual interface, instead of incorrectly indexing the MSVC x86 vtable. This source correction has not yet been linked into a complete native renderer.

On the Intel build Mac with Xcode 14:

1. Compared 100,000 binary32 operand pairs and 100,000 binary64 operand pairs, four arithmetic operations per pair, with hardware SSE. Includes signed zeros, subnormal values, infinities, quiet/signalling NaNs, conversion boundaries and random raw bit patterns. Result digest: `9924b16d9e2c2027`.
2. Tested scheduler priority ordering, deleting the next callback while dispatching, callback self-removal, lock depth and shutdown; session pointer preservation and scalar accessor isolation; native thread destruction, move and detach.
3. Ran these tests under AddressSanitizer and UndefinedBehaviorSanitizer: PASS.
4. Cross-compiled and linked the foundation executable as Mach-O ARM64 with `LC_BUILD_VERSION minos 14.0`, SDK 16.0: PASS. ARM64 code execution has **not** been performed on this Intel Mac. The arithmetic digest remains a separate hardware reference check when the tests run on ARM64 later.
5. Parent-provided app-local PMR shim: native allocations/containers/deallocations PASS; ARM64 iOS 14 link PASS. The probe has no unresolved PMR or `memory_resource` symbols, so it does not depend on newer system PMR singletons.

Numeric limits: this does not prove whole-game or libm (`sin`, `atan2`, etc.) cross-architecture equivalence. The scalar arithmetic requires normal IEEE mode, `-fno-fast-math` and `-ffp-contract=off`. It does not advertise MXCSR exception flags or non-default DAZ/FTZ settings. Source oracle exports using `_mm_getcsr`/`_mm_setcsr` remain x86 testing tools.

## Why full native gameplay still fails compilation

Many remaining assertions correctly reject a native 64-bit object. Disabling them globally would allow silent memory corruption. The critical next conversion sequence is:

1. **Animation and sprite controller**. `AnimationBase` is numeric, has no runtime pointers, and can retain its 0x4c0-byte layout. `AnimationLink` and `Animation` suffixes must grow or move to explicit native fields. Preserve ANM file headers and opcode operands as 32-bit file data.
2. **Player/Shot/Option plus cross-module consumers**. Use named native members, keep numeric save/profile records fixed, and replace pointer words and callbacks inside numeric arrays. Then audit every shared byte-view accessor.
3. **Enemy runtime/script manager and gameplay containers**. Preserve ECL operand words and relative script offsets; expand actual pointers, C++ containers and polymorphic owners. Do not confuse ECL integer data with addresses.
4. **Remaining owners and platform state**. Native vtables, C++ strings, vectors, queues, workers and audio structures need explicit checked native layouts or field-level access, including program-entry worker storage alignment.

## Concrete Animation hazards

| Original location | Actual use | Required native treatment |
|---|---|---|
| `Animation::field_4e8` | animation list pointer | native pointer, all assignment/cast sites updated |
| `links[5]` at +0x4ec | five-pointer linked nodes | native nodes compatible with `scheduler::Link`; assert that equivalence |
| `fields_550[2]`, `[3]` | root/parent animation pointers | explicit native pointer fields; retain numeric flags separately |
| `fields_550[5]`, `[6]` | allocated geometry and polymorphic callback pointers | native ownership fields and destruction paths |
| `field_5c8` | callback owner/mesh/bullet pointer | native address, not a 32-bit scalar |
| `field_5dc`, `field_5e0` | update and sprite-remap callbacks | typed native function pointers |
| `TextureRecord::unknown_04` | decoded texture input pointer | native pointer; `unknown_08` remains byte count |
| `Controller::cached_texture`, `field_e18` | cached texture/sprite pointer identities | native pointers or typed cache keys |

Affected files include `binding.cpp`, `pool.cpp`, `pool_spawn.cpp`, `named_spawn.cpp`, `anm_vm.cpp`, `draw.cpp`, `quad.cpp`, `render_mesh.cpp`, `geometry_draw.cpp`, `projected_draw.cpp`, plus callbacks in bullet, bomb, laser, effects, player and overlay modules. The audit found 93 source lines using the key suffix fields (excluding CPU-comparison/oracle files); this is a search inventory, not a proof that no other indirect access exists.

Controller buffer limits currently use `&controller + 0x7c00dcc` and `+0x7d40e80` in rendering/effects. Replace them with named array endpoints **before** changing controller layout. The controller also allocates 65,536 animations and 1,048,576 textured vertices: its original size is `0x7d40e94` (about 125 MiB). Native pointer expansion increases that footprint until pools are separately profiled and redesigned without changing allocation/handle semantics.

`AnimationBase` ANM byte accesses below +0x4c0 can stay fixed only while its numeric layout is retained. Do not globally shift offsets: values such as +0x498 address flag bytes used directly by opcodes.

`gameplay/enemy_frame.cpp` additionally writes animation slowdown directly at +0x560 and uses raw native-unsafe enemy vector pointers at +0x98/+0x9c. The slowdown write must become a named animation member when its suffix grows.

## Concrete Player/Option/Shot hazards

| Original location | Current consumers / risk |
|---|---|
| Player +0x10/+0x14 | state and flags after a 16-byte polymorphic base; native vptr makes this base grow |
| Player +0x614 | position in collision, items, stone UI and enemy reads |
| Player +0x204c/+0x2050 | focus byte and invulnerability/death timers |
| Player +0x2094/+0x2098/+0x209c/+0x20a8 | collision radius and extents |
| Player +0x20ec | clock-scale writes from `enemy_shot_adapter.cpp` |
| Player +0x2244 | feedback object, accessed by `enemy_damage.cpp` / `enemy_defeat.cpp` |
| Player +0x22b4+0x12430 | active shot list, accessed by `damage_regions/hit_callbacks.cpp` |
| Player +0x14858 | context pointer, read in collision/events |
| Option +0x11c/+0x120/+0x128 | callbacks and context pointer encoded in 32-bit `fields_f4` words |
| ShotController `field_12558` | shot-data pointer encoded as a 32-bit word |

`player_entity/events.cpp` and `collision.cpp` are still void-pointer byte views. `overlay_system` uses additional raw views into `Player`, `Option`, and polymorphic weapon state. They must be converted together with the actual object declarations; a new larger `Player` declaration by itself is unsafe.

## Remaining acceptance limits

These foundations do not yet link the complete game, validate stage completion, validate any iPhone framerate or memory budget, or produce a playable IPA. Original full-game/replay equivalence remains unverified. No existing TH06/TH07/TH08/TH095 project was edited during this work.
