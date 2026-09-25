"""Compile the real UIKit engine entry and input bridge with Apple's iOS SDK."""
import concurrent.futures
import hashlib
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[3]
OUT = ROOT / "ios/tests/entry/results"
OUT.mkdir(parents=True, exist_ok=True)
compiler = subprocess.check_output(["xcrun", "--find", "clang++"], text=True).strip()
sdk = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], text=True).strip()
includes = [ROOT / "ios/compat", ROOT / "ios/src", ROOT / "native_recovered", ROOT / "include", *sorted((ROOT / "source_reconstruction").iterdir())]
common = ["-std=c++20", "-DTH20_IOS=1", "-include", str(ROOT / "ios/compat/port_prefix.hpp"), "-fno-fast-math", "-ffp-contract=off", "-fno-strict-aliasing", "-Wno-invalid-" + "offsetof", "-Wno-ignored-attributes"]
common += ["-I" + str(path) for path in includes if path.is_dir()]
sources = [ROOT / path for path in ["ios/src/ios_engine.cpp", "ios/src/ios_game_input.cpp", "source_reconstruction/player_entity/movement.cpp"]]

def compile_source(path):
    result = subprocess.run([compiler, *common, "-target", "arm64-apple-ios14.0", "-isysroot", sdk, "-fsyntax-only", str(path)], capture_output=True, text=True)
    log = result.stdout + result.stderr
    (OUT / (path.stem + ".arm64.log")).write_text(log)
    return {"path": str(path.relative_to(ROOT)), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "passed": result.returncode == 0, "errors": [line for line in log.splitlines() if "error:" in line]}

with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    compiled = list(pool.map(compile_source, sources))
report = {"target": "arm64-apple-ios14.0", "compiler": subprocess.check_output([compiler, "--version"], text=True).splitlines()[0], "scope": "Native entry and input Apple ARM64 compilation; game execution and touch visual alignment require full integration tests", "passed": sum(row["passed"] for row in compiled), "total": len(compiled), "sources": compiled}
(OUT / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
raise SystemExit(0 if all(row["passed"] for row in compiled) else 1)
