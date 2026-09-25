"""Real Apple SDK compilation and native ASan/UBSan player layout validation."""
import concurrent.futures
import hashlib
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[3]
OUT = ROOT / "ios/tests/player/results"
OUT.mkdir(parents=True, exist_ok=True)
compiler = subprocess.check_output(["xcrun", "--find", "clang++"], text=True).strip()
sdk = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], text=True).strip()
includes = [ROOT / "ios/compat", ROOT / "native_recovered", ROOT / "include", *sorted((ROOT / "source_reconstruction").iterdir())]
common = ["-std=c++20", "-DTH20_IOS=1", "-include", str(ROOT / "ios/compat/port_prefix.hpp"), "-fno-fast-math", "-ffp-contract=off", "-fno-strict-aliasing", "-Wno-invalid-" + "offsetof", "-Wno-ignored-attributes"]
common += ["-I" + str(path) for path in includes if path.is_dir()]

def compile_source(path):
    result = subprocess.run([compiler, *common, "-target", "arm64-apple-ios14.0", "-isysroot", sdk, "-fsyntax-only", str(path)], capture_output=True, text=True)
    log = result.stdout + result.stderr
    (OUT / (path.stem + ".arm64.log")).write_text(log)
    return {"path": str(path.relative_to(ROOT)), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "passed": result.returncode == 0, "errors": [line for line in log.splitlines() if "error:" in line]}

sources = sorted(path for path in (ROOT / "source_reconstruction/player_entity").glob("*.cpp") if path.name != "cpu_compare.cpp")
sources.append(ROOT / "ios/tests/player/native_probe.cpp")
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    compiled = list(pool.map(compile_source, sources))
native = subprocess.run([compiler, *common, "-g", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", str(ROOT / "ios/tests/player/native_probe.cpp"), str(ROOT / "source_reconstruction/player_entity/state_layout.cpp"), str(ROOT / "source_reconstruction/player_entity/shot_data.cpp"), "-o", str(OUT / "player_native_probe")], capture_output=True, text=True)
native_log = native.stdout + native.stderr
run = None
if native.returncode == 0:
    run = subprocess.run([str(OUT / "player_native_probe")], capture_output=True, text=True)
    native_log += run.stdout + run.stderr
(OUT / "memory-validation.log").write_text(native_log)
headers = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted((ROOT / "source_reconstruction/player_entity").glob("*.hpp"))}
report = {"target": "arm64-apple-ios14.0", "compiler": subprocess.check_output([compiler, "--version"], text=True).splitlines()[0], "scope": "Apple ARM64 compile; constructors and SHT parser executed on macOS x86_64 with ASan/UBSan; no game or simulator execution asserted", "passed": sum(row["passed"] for row in compiled), "total": len(compiled), "sources": compiled, "header_sha256": headers, "native_memory_probe_passed": native.returncode == 0 and run is not None and run.returncode == 0}
(OUT / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(f"ARM64 compile: {report['passed']}/{report['total']}; macOS ASan/UBSan: {report['native_memory_probe_passed']}")
print(native_log)
for row in compiled:
    if not row["passed"]:
        print(row["path"], *row["errors"], sep="\n  ")
