# Developer menu and Auto Bomb checks

Version 0.3.0 (build 4) passed the native iPhone simulator probe: **27 checks,
0 failures**. The production ARM64 Release target also built successfully.

The probe runs against an initialized game in the isolated smoke bundle and
uses the production circle collision, hit interception, character bomb, power,
resource setters, bullet spawning/cancellation and laser spawning/cancellation.
An observing event service counts calls downstream of the hit interception,
including death sounds and hit effects. It does not replace the bomb logic.

Coverage:

- Developer actions are unavailable while Developer Mode is disabled.
- Maximum score, power, item counters, stone gauge, life/bomb stock and Max All.
- A real spawned bullet and laser enter their cancellation states.
- Invincibility skips the hit graph; disabling Developer Mode restores it.
- Auto Bomb with no stock retains normal hit handling.
- An eligible collision starts Reimu's actual bomb, spends exactly one stock,
  preserves life count and skips death sound/effect/notification calls.
- Further collisions in that frame neither spend another bomb nor kill the player.
- Developer actions are rejected during replay playback.
- The DEV button opens the panel and pauses play; closing resumes play.
- The invincibility row updates the engine and its visible ON/OFF label.
- The panel fits after rotation and its list can scroll to the Close row.
- Both new settings switches update the host and persisted preferences.

Reproduce on macOS with a dedicated iPhone simulator and owned game assets:

```sh
TH20_CONFIGURATION=Release TH20_SDK=iphonesimulator \
TH20_TARGET=th20_ios_game_smoke TH20_ASSET_DIR=/path/to/game-assets \
bash ios/build_ios.sh

SIMCTL_CHILD_TH20_DEV_PROBE=1 python3 ios/tests/run_mobile_smoke.py \
  SIMULATOR_UDID \
  build-native-iphonesimulator/Release-iphonesimulator/th20_ios_game_smoke.app \
  build-native-reports/dev-menu
```

The test driver is linked only into the smoke app. It modifies that app's
isolated session and preferences, never the production app's save data.
The results do not establish every character/stone combination, replay
compatibility for assisted runs, or real-device performance.
