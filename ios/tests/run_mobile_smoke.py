"""Run the isolated native game smoke and capture actual simulator output.

Run on the build Mac; arguments are the dedicated simulator UDID and .app.
The production bundle is deliberately rejected.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

os.environ.setdefault("DEVELOPER_DIR", "/Applications/Xcode.app/Contents/Developer")
simulator, app, report = sys.argv[1:4]
report = Path(report)
report.mkdir(parents=True, exist_ok=True)


def simctl(*args, check=True):
    return subprocess.run(["xcrun", "simctl", *args], text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, check=check).stdout.strip()


import plistlib
with (Path(app) / "Info.plist").open("rb") as source:
    bundle = plistlib.load(source)["CFBundleIdentifier"]
if bundle != "org.th20.native.smoke":
    raise SystemExit("Only the isolated smoke bundle may run this test")
simctl("terminate", simulator, bundle, check=False)
simctl("install", simulator, app)
container = Path(simctl("get_app_container", simulator, bundle, "data"))
docs = container / "Documents"
for name in ("game-smoke.json", "mobile-settings-probe.json", "smoke-milestone.txt"):
    (docs / name).unlink(missing_ok=True)
print(simctl("launch", simulator, bundle), flush=True)
seen = set()
deadline = time.monotonic() + 1860
while time.monotonic() < deadline:
    milestone = docs / "smoke-milestone.txt"
    if milestone.exists():
        name = milestone.read_text().strip()
        if name and name not in seen:
            seen.add(name)
            simctl("io", simulator, "screenshot", str(report / (name + ".png")))
            print("Screenshot: " + name, flush=True)
    result = docs / "game-smoke.json"
    if result.exists():
        try:
            outcome = json.loads(result.read_text())
        except json.JSONDecodeError:
            time.sleep(.25)
            continue
        (report / result.name).write_text(json.dumps(outcome, indent=2) + "\n")
        settings = docs / "mobile-settings-probe.json"
        if settings.exists():
            (report / settings.name).write_bytes(settings.read_bytes())
        logs = sorted((docs / "Logs").glob("*.log"), key=lambda p: p.stat().st_mtime)
        if logs:
            (report / "game.log").write_bytes(logs[-1].read_bytes())
        simctl("io", simulator, "screenshot", str(report / "final.png"))
        print(json.dumps(outcome), flush=True)
        sys.exit(0 if outcome["passed"] else 1)
    time.sleep(.5)
logs = sorted((docs / "Logs").glob("*.log"), key=lambda p: p.stat().st_mtime)
if logs:
    (report / "game.log").write_bytes(logs[-1].read_bytes())
simctl("io", simulator, "screenshot", str(report / "timeout.png"))
raise SystemExit("Smoke timed out; captured current screen and latest game log")
