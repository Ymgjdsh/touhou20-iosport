# Japanese / Simplified Chinese resources

Settings → 游戏语言 / ゲーム言語 offers System, Japanese, and Simplified Chinese.
The default follows the first preferred system language: Chinese locales,
including Traditional Chinese locales, use the supplied Simplified Chinese
pack; other locales use Japanese. An explicit choice persists across launches.
Changing language reloads the title screen. During battle, confirmation is
required before ending the current run; previously saved records remain intact.

The integration reads the **2025-09-06 TH20 standalone Simplified Chinese patch**
as data. It never executes its Windows launcher, DLLs, or binary hooks.
Dialogue, endings, music, trophy, stone and spell translations and replacement
textures are compiled into an optional native resource directory. Original DAT
files remain unchanged, and Japanese bypasses resource overrides.
English artwork retained by the supplied patch remains English. The existing
mobile control/settings descriptions are Chinese; the language picker and
switch confirmation are bilingual.

## Preparing a pack

Requirements: a legally owned full-game archive, the standalone patch with
its sibling thpatch and nmlgc directories, Python 3.9+, Pillow, NumPy,
and a C++17 compiler on the build Mac. From the repository root:

```sh
clang++ -std=c++17 ios/tools/export_translation_sources.cpp \
  source_reconstruction/archive/archive.cpp -o /tmp/th20-export-translations
/tmp/th20-export-translations /path/to/game-assets/th20.dat /path/to/exported
python3 -m pip install Pillow numpy
python3 ios/tools/compile_translation.py \
  /path/to/patch/thcrap/repos/thpatch/lang_zh-hans/th20 \
  /path/to/exported /path/to/translations/zh-Hans
python3 ios/tools/verify_translation.py \
  /path/to/exported /path/to/translations/zh-Hans
TH20_CONFIGURATION=Release TH20_SDK=iphoneos TH20_TARGET=th20_ios_game \
TH20_ASSET_DIR=/path/to/game-assets \
TH20_TRANSLATION_DIR=/path/to/translations/zh-Hans bash ios/build_ios.sh
```

The compiler checks patch coverage and emits SHA-256 entries. The verifier
checks every resource hash, message entry and non-text instruction, all animation
headers/scripts/sprites and texture dimensions, image decoding and string
references. Short ASCII references fit the original fixed CP932 buffers;
CoreText resolves these to Unicode when drawing. Language reloads clear font
caches. Display translations never replace spell names in saved records.

Keep original assets, the supplied patch and generated packs outside the public
source repository. Do not commit private game saves or test logs.

## Translation credits

The supplied patch credits the thpatch Simplified Chinese community, THBWiki,
猫吧元老, BUNBUN^Aya, OWQzd3Rn, 剑客小鹿, Yzdnn, 希铁石z, 冰川寒焰,
ROCO2017, and the Touhou Patch Center developers and contributors. This is the
port's integration of their work, not a new translation by the port author.

- https://www.thpatch.net/wiki/Portal:Zh-hans
- https://github.com/thpatch/thcrap

Original game, artwork and music: ZUN / Team Shanghai Alice.
