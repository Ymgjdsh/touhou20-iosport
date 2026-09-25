"""Compile each shipping engine translation unit for real iOS ARM64.

This audit deliberately retains layout assertions. Results are evidence of
compile coverage only, never a declaration that gameplay is correct.
"""
import argparse
import concurrent.futures
import json
import os
import pathlib
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--jobs", type=int, default=4)
parser.add_argument("--output", required=True)
parser.add_argument("--module", action="append", help="Limit audit to the named source module (repeatable)")
parser.add_argument("--source", action="append", help="Limit audit to an exact shipping source path (repeatable)")
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(args.output).resolve()
out.mkdir(parents=True, exist_ok=True)
os.environ.setdefault("DEVELOPER_DIR", "/Applications/Xcode.app/Contents/Developer")
sdk = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], text=True).strip()
compiler = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--find", "clang++"], text=True).strip()
includes = [root/"ios/compat", root/"ios/src", root/"native_recovered", root/"include", root/"source_reconstruction"]
includes += [p for p in (root/"source_reconstruction").iterdir() if p.is_dir()]
base = [compiler, "-target", "arm64-apple-ios14.0", "-isysroot", sdk, "-std=c++20",
        "-DTH20_IOS=1", "-include", str(root/"ios/compat/port_prefix.hpp"),
        "-fno-fast-math", "-ffp-contract=off", "-fno-strict-aliasing",
        "-fsyntax-only", "-ferror-limit=0", "-Wno-invalid offsetof".replace(" ", "-")]
for directory in includes:
    base += ["-I", str(directory)]
manifest = json.loads((root/"SOURCE_BUILD_VERIFICATION.json").read_text(encoding="utf-8-sig"))
sources = [s for s in manifest["compiled_sources"] if s.startswith("source_reconstruction/") and pathlib.Path(s).suffix in (".cpp", ".c", ".mm") and
           not any(f"/{part}/" in s for part in ("diagnostics", "link_probe"))]
sources = [s for s in sources if s not in (
    "source_reconstruction/program_entry/program_entry.cpp",
    "source_reconstruction/program_entry/winmain.cpp")]
sources += ["src/binary.cpp", "src/ecl.cpp", "source_reconstruction/text_renderer/native_font.cpp"]
sources += [s.relative_to(root).as_posix() for s in sorted((root/"ios/src").glob("*"))
            if s.suffix in (".cpp", ".mm")]
if args.module:
    sources = [s for s in sources if s.split("/")[1] in args.module]
if args.source:
    unknown = set(args.source) - set(sources)
    if unknown:
        raise SystemExit(f"Not a selected shipping source: {sorted(unknown)}")
    sources = [s for s in sources if s in args.source]
if not sources:
    raise SystemExit("No matching shipping sources")

def check(source):
    flags = ["-fobjc-arc"] if source.endswith(".mm") else []
    result = subprocess.run(base+flags+[str(root/source)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    text = result.stdout.replace(str(root)+"/", "")
    errors = [line for line in text.splitlines() if re.search(r"(?:fatal )?error:", line)]
    log = source.replace("/", "__")+".txt"
    (out/log).write_text(text, encoding="utf-8")
    return {"source": source, "exit_code": result.returncode, "errors": errors, "log": log}

results=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
    for result in pool.map(check, sources):
        results.append(result)
        print(f"[{len(results)}/{len(sources)}] {'PASS' if not result['exit_code'] else 'FAIL'} {result['source']}", flush=True)
unique=sorted(set(error for result in results for error in result["errors"]))
report={"target":"arm64-apple-ios14.0", "scope":"compile-only; assertions retained; no gameplay executed",
        "total":len(results), "passed":sum(r["exit_code"]==0 for r in results),
        "failed":sum(r["exit_code"]!=0 for r in results),"unique_errors":unique,"results":results}
(out/"native-audit.json").write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
(out/"unique-errors.txt").write_text("\n".join(unique),encoding="utf-8")
print(json.dumps({key:report[key] for key in ("target","total","passed","failed")}),flush=True)
raise SystemExit(1 if report["failed"] else 0)
