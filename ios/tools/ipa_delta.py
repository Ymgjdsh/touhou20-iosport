"""Transfer a signed IPA without retransmitting unchanged compressed assets.

Reconstruction must match the complete Mac-produced IPA's SHA-256 exactly.
No bundle files are modified or re-signed by this transport helper.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile

ASSETS = ("Payload/TH20.app/th20.dat", "Payload/TH20.app/thbgm.dat")


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(data)
    return result.hexdigest()


def spans(path):
    result = {}
    with zipfile.ZipFile(path) as archive, path.open("rb") as stream:
        for name in ASSETS:
            info = archive.getinfo(name)
            stream.seek(info.header_offset)
            header = stream.read(30)
            if header[:4] != b"PK\x03\x04":
                raise ValueError("Invalid local ZIP header")
            name_length, extra_length = struct.unpack_from("<HH", header, 26)
            result[name] = (info.header_offset + 30 + name_length + extra_length, info.compress_size)
    return result


def create(source, delta):
    if delta.exists():
        raise FileExistsError(delta)
    parts = []
    with source.open("rb") as stream, zipfile.ZipFile(delta, "w", zipfile.ZIP_STORED) as archive:
        cursor = 0
        for name, (offset, size) in sorted(spans(source).items(), key=lambda item: item[1][0]):
            segment = "segment-" + str(len(parts))
            stream.seek(cursor)
            archive.writestr(segment, stream.read(offset - cursor))
            parts.append({"literal": segment})
            compressed_hash = hashlib.sha256()
            remaining = size
            while remaining:
                data = stream.read(min(remaining, 1024 * 1024))
                if not data:
                    raise EOFError(source)
                compressed_hash.update(data)
                remaining -= len(data)
            parts.append({"reuse": name, "bytes": size, "sha256": compressed_hash.hexdigest()})
            cursor = offset + size
        segment = "segment-" + str(len(parts))
        stream.seek(cursor)
        archive.writestr(segment, stream.read())
        parts.append({"literal": segment})
        archive.writestr("recipe.json", json.dumps({"sha256": digest(source), "bytes": source.stat().st_size, "parts": parts}))
    print(json.dumps({"delta_bytes": delta.stat().st_size, "delta_sha256": digest(delta)}), flush=True)


def apply(previous, delta, output):
    temporary = output.with_name(output.name + ".assemble")
    if output.exists() or temporary.exists():
        raise FileExistsError(output)
    available = spans(previous)
    with zipfile.ZipFile(delta) as archive, previous.open("rb") as old, temporary.open("xb") as result:
        recipe = json.loads(archive.read("recipe.json"))
        for part in recipe["parts"]:
            if "literal" in part:
                result.write(archive.read(part["literal"]))
                continue
            offset, size = available[part["reuse"]]
            if size != part["bytes"]:
                raise ValueError("Compressed asset size differs: " + part["reuse"])
            old.seek(offset)
            compressed_hash = hashlib.sha256()
            while size:
                data = old.read(min(size, 1024 * 1024))
                if not data:
                    raise EOFError(previous)
                compressed_hash.update(data)
                result.write(data)
                size -= len(data)
            if compressed_hash.hexdigest() != part["sha256"]:
                raise ValueError("Compressed asset hash differs: " + part["reuse"])
    if temporary.stat().st_size != recipe["bytes"] or digest(temporary) != recipe["sha256"]:
        raise ValueError("Reconstructed IPA does not match the signed Mac IPA")
    temporary.rename(output)
    print(json.dumps({"output": str(output), "bytes": output.stat().st_size, "sha256": recipe["sha256"]}), flush=True)


if __name__ == "__main__":
    if sys.argv[1] == "create":
        create(Path(sys.argv[2]), Path(sys.argv[3]))
    elif sys.argv[1] == "apply":
        apply(Path(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[4]))
    else:
        raise SystemExit("Use create SOURCE DELTA or apply PREVIOUS DELTA OUTPUT")
