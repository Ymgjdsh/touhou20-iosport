"""Compare actual native decoder output to the independently recovered catalog."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("hashes", type=Path)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
expected = json.loads((root/"reports/assets_recovery.json").read_text(encoding="utf-8-sig"))
rows = [line.split("\t") for line in args.hashes.read_text().splitlines() if line]
actual = {int(index): {"bytes": int(size), "sha256": sha} for index, size, sha in rows}
assert len(rows) == len(actual) == expected["entry_count"], "Wrong entry count or duplicate index"
for entry in expected["entries"]:
    assert actual[entry["index"]] == {k: entry[k] for k in ("bytes", "sha256")}, entry["name"]
report = {
    "status": "passed", "scope": "native decoder file/memory parity and independent thtk catalog hashes",
    "archive_sha256": expected["archive_sha256_before"], "entries": len(actual),
    "decoded_bytes": sum(e["bytes"] for e in actual.values()), "reverse_reads": "passed by archive_stream_test",
    "gameplay_tested": False,
}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
print(json.dumps(report))
