# Language integration checks

The Release simulator smoke run passes **21 checks, 0 failures** for version
0.4.0 (build 5). The separate ARM64 iOS 14+ Release game target also builds.

The isolated smoke app removes its language preference and launches with a
Chinese preferred language to test a first launch. It uses the real settings
picker and engine resource/font paths, switches to Japanese and back to Chinese,
opens the music room, starts a stage, and displays translated dialogue.
In battle it checks that switching asks first and that cancelling retains the
current battle and language. The production app contains none of this driver.

Additional checks cover Japanese/non-Chinese system defaults, manual overrides,
all 3,247 translated token strings through CoreText (including missing-glyph
checks), static translations and formatted values, spell-name lookup, translated
help/title resources, and Japanese resource fallback. Screenshots are captured
only after the relevant animation or dialogue reaches its visible state.

Resource verification passes for **137 files, 178 message entries, 488 animation
entries, and 5,336 text references**. Non-text message instructions, sprite/script
data, texture dimensions, file hashes and image decoding are checked against the
exported originals. This verifies integration, not translation authorship or a
complete playthrough of every ending and character route.

Run on the build Mac with a dedicated simulator and the optional pack configured:

```sh
TH20_CONFIGURATION=Release TH20_SDK=iphonesimulator \
TH20_TARGET=th20_ios_game_smoke \
TH20_ASSET_DIR=/path/to/game-assets \
TH20_TRANSLATION_DIR=/path/to/translations/zh-Hans bash ios/build_ios.sh
SIMCTL_CHILD_TH20_LANGUAGE_PROBE=1 python3 ios/tests/run_mobile_smoke.py \
  SIMULATOR_UDID \
  build-native-iphonesimulator/Release-iphonesimulator/th20_ios_game_smoke.app \
  /path/to/reports/language
python3 ios/tools/verify_translation.py \
  /path/to/exported /path/to/translations/zh-Hans
```

The smoke app uses an isolated bundle ID and 50% output resolution to reduce
software GLES costs on Intel simulators. It does not establish real-device
performance, installation compatibility or coverage of all gameplay content.
Real-device checks remain necessary after installing the IPA.
