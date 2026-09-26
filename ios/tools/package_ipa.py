"""Validate and package a native device build; this does not certify gameplay."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import tempfile
import zipfile


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def command(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT).strip()


parser = argparse.ArgumentParser()
parser.add_argument("app", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
app = args.app.resolve()
output = args.output.resolve()
if output.exists() or output.with_suffix(".manifest.json").exists():
    raise SystemExit("Choose a new versioned output path; an artifact already exists")
if not app.is_dir() or app.suffix != ".app":
    raise SystemExit("Expected an existing device .app bundle")
os.environ.setdefault("DEVELOPER_DIR", "/Applications/Xcode.app/Contents/Developer")
with (app / "Info.plist").open("rb") as source:
    info = plistlib.load(source)
executable = app / info["CFBundleExecutable"]
if executable.parent != app or not executable.is_file():
    raise SystemExit("Invalid bundle executable path")
if info.get("CFBundleIdentifier") != "org.th20.native":
    raise SystemExit("Only the TH20 native game bundle can be packaged")
if info.get("MinimumOSVersion") != "14.0":
    raise SystemExit("Unexpected minimum OS version")
architectures = command("xcrun", "lipo", "-archs", str(executable)).split()
if architectures != ["arm64"]:
    raise SystemExit(f"Expected a device ARM64 executable, got {architectures}")
load_commands = command("xcrun", "otool", "-l", str(executable))
if not re.search(r"platform\s+(2|IOS)\b", load_commands) or "LC_BUILD_VERSION" not in load_commands:
    raise SystemExit("Executable is not an iOS device build")
if re.search(r"(?:WebKit|JavaScriptCore)\.framework", command("xcrun", "otool", "-L", str(executable))):
    raise SystemExit("Unexpected browser runtime dependency")
for path in app.rglob("*"):
    if path.suffix.lower() in {".exe", ".dll", ".wasm", ".html", ".js"}:
        raise SystemExit(f"Unexpected non-native payload: {path.name}")
manifest = json.loads((Path(__file__).resolve().parents[1] / "assets_manifest.json").read_text())
for asset in manifest["assets"]:
    path = app / asset["name"]
    if not path.is_file() or path.stat().st_size != asset["bytes"] or digest(path) != asset["sha256"]:
        raise SystemExit(f"Bundled asset mismatch: {asset['name']}")
translation = app / "Translations" / "zh-Hans"
translation_count = 0
if translation.exists():
    with (translation / "translations.json").open(encoding="utf-8") as source:
        translated = json.load(source)
    files = translated.get("files")
    if translated.get("language") != "zh-Hans" or not isinstance(files, dict):
        raise SystemExit("Invalid bundled Simplified Chinese manifest")
    for name, expected in files.items():
        if Path(name).name != name or not re.fullmatch(r"[0-9a-f]{64}", expected):
            raise SystemExit(f"Invalid translated resource entry: {name}")
        path = translation / name
        if not path.is_file() or digest(path) != expected:
            raise SystemExit(f"Bundled translation mismatch: {name}")
    if {path.name for path in translation.iterdir() if path.is_file()} != set(files) | {"translations.json"}:
        raise SystemExit("Bundled translation contains unexpected or missing files")
    translation_count = len(files)
output.parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="th20-native-package-") as temporary:
    staging = Path(temporary)
    bundle = staging / "Payload" / "TH20.app"
    shutil.copytree(app, bundle)
    command("/usr/bin/codesign", "--force", "--sign", "-", "--timestamp=none", str(bundle))
    command("/usr/bin/codesign", "--verify", "--strict", str(bundle))
    executable_hash = digest(bundle / info["CFBundleExecutable"])
    partial = staging / "package.ipa"
    with zipfile.ZipFile(partial, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted((staging / "Payload").rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(staging).as_posix())
    with zipfile.ZipFile(partial) as archive:
        bad = archive.testzip()
        if bad:
            raise SystemExit(f"IPA archive CRC failure: {bad}")
    shutil.copyfile(partial, output)
report = {"artifact": output.name, "sha256": digest(output), "bytes": output.stat().st_size,
          "bundle_identifier": info["CFBundleIdentifier"], "version": info["CFBundleShortVersionString"],
          "build": info["CFBundleVersion"], "minimum_ios": info["MinimumOSVersion"],
          "architectures": architectures, "executable_sha256": executable_hash,
          "signature": "ad-hoc; TrollStore installation still requires device verification",
          "assets": manifest["assets"], "translation_files": translation_count,
          "validation_scope": "bundle, architecture, signature, resources, archive integrity; no gameplay certification"}
output.with_suffix(".manifest.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(json.dumps(report, indent=2))
