"""Verify pack hashes, script preservation and image integrity against exports."""
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import sys
from PIL import Image
from compile_translation import crypt, u32


def messages(data):
    count = u32(data)
    assert 0 < count < 100
    offsets = [u32(data, 4 + i * 8) for i in range(count)] + [len(data)]
    assert offsets[0] >= 4 + count * 8 and offsets == sorted(offsets)
    result = []
    for start, end in zip(offsets, offsets[1:]):
        commands = []
        while start < end:
            time, opcode, size = struct.unpack_from('<HBB', data, start)
            assert start + 4 + size <= end
            commands.append((time, opcode, data[start+4:start+4+size]))
            start += 4 + size
        result.append(commands)
    return result


def animations(data):
    at, result = 0, []
    while True:
        assert u32(data, at) == 8
        texture, following = u32(data, at+28), u32(data, at+36)
        entry = data[at:at+following if following else len(data)]
        header = bytearray(entry[:texture] if texture else entry)
        struct.pack_into('<I', header, 36, 0)
        size = None
        if texture:
            assert entry[texture:texture+4] == b'THTX'
            length = u32(entry, texture+12)
            assert texture+16+length <= len(entry)
            with Image.open(io.BytesIO(entry[texture+16:texture+16+length])) as image:
                image.load()
                size = image.size
        result.append((header, size))
        if not following:
            break
        assert following >= 64
        at += following
        assert at < len(data)
    return result


def verify(original, pack):
    manifest = json.loads((pack/'translations.json').read_text(encoding='utf-8'))
    assert manifest['version'] == 1 and manifest['language'] == 'zh-Hans'
    tokens = manifest['tokens']
    assert all(re.fullmatch(r'~ZH\d{6}~', key) for key in tokens)
    assert all(isinstance(value, str) and '\0' not in value for value in tokens.values())
    counts = {'files': 0, 'message_entries': 0, 'animation_entries': 0, 'text_references': 0}

    def references(data):
        for token in re.findall(rb'~ZH\d{6}~', data):
            assert token.decode('ascii') in tokens
            counts['text_references'] += 1

    for name, digest in manifest['files'].items():
        assert Path(name).name == name
        data = (pack/name).read_bytes()
        assert hashlib.sha256(data).hexdigest() == digest, name
        if name.endswith('.msg'):
            source = (original/name).read_bytes()
            before, after = messages(source), messages(data)
            assert len(before) == len(after), name
            for i, (old, new) in enumerate(zip(before, after)):
                assert source[8+i*8:12+i*8] == data[8+i*8:12+i*8], name
                line = 3 if name.startswith('e') else 17
                ignored = {line} if line == 3 else {line, 25}
                assert [c for c in old if c[1] not in ignored] == [c for c in new if c[1] not in ignored], (name, i)
                for _, op, payload in new:
                    if op == line:
                        references(crypt(payload))
                counts['message_entries'] += 1
        elif name.endswith('.anm'):
            before, after = animations((original/name).read_bytes()), animations(data)
            assert before == after, (name, 'animation instructions, sprites or dimensions changed')
            counts['animation_entries'] += len(after)
        elif name.endswith('.png'):
            with Image.open(io.BytesIO(data)) as image:
                image.verify()
        elif name.endswith('.txt'):
            data.decode('ascii')
            references(data)
        counts['files'] += 1
    for token in manifest['spells'].values():
        assert token in tokens
    print(json.dumps({'status': 'passed', **counts}))


if __name__ == '__main__':
    verify(Path(sys.argv[1]), Path(sys.argv[2]))
