"""Package only TH20 native sources. Never include credentials or old projects."""
import argparse
import pathlib
import tarfile

parser = argparse.ArgumentParser()
parser.add_argument("--output", required=True)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[2]
output = pathlib.Path(args.output).resolve()
allowed = {".hpp", ".h", ".cpp", ".c", ".inc", ".mm", ".m", ".json", ".plist", ".sh", ".py", ".md", ".txt"}
files = [root / "SOURCE_BUILD_VERIFICATION.json"]
for directory in ("ios", "source_reconstruction", "native_recovered", "include", "src"):
    for path in sorted((root / directory).rglob("*")):
        if not path.is_file() or any(p in ("__pycache__", ".git", "reports") or p.startswith("build") for p in path.relative_to(root).parts[:-1]):
            continue
        if path.suffix.lower() in allowed or path.name == "memory_resource":
            files.append(path)
output.parent.mkdir(parents=True, exist_ok=True)
with tarfile.open(output, "w:gz") as archive:
    for path in files:
        archive.add(path, arcname=path.relative_to(root).as_posix(), recursive=False)
print(f"Packaged {len(files)} source files ({output.stat().st_size} bytes)")
