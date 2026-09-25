"""Compile a user-supplied thpatch zh-hans pack into native, data-only resources.

Inputs are exported by export_translation_sources.cpp. Requires Pillow and NumPy.
Original assets and generated packs must not be committed to the source repository.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import struct

import numpy as np
from PIL import Image


def u32(data, at=0):
    return struct.unpack_from('<I', data, at)[0]


def crypt(data):
    key, step = 0x77, 7
    result = bytearray()
    for value in data:
        result.append(value ^ key)
        key, step = (key + step) & 255, (step + 16) & 255
    return bytes(result)


def clean(value):
    # thcrap tab-stop markup aligns speaker labels; keep the visible labels.
    value = re.sub(r'<ts\$[^>]*>', '', value)
    value = re.sub(r'<[rlc]\$([^>]*)>', lambda m: m[1].removesuffix('$'), value)
    if re.search(r'<\w+\$', value):
        raise ValueError(f'Unsupported text markup: {value!r}')
    return value


class Compiler:
    def __init__(self, patch, original, output):
        self.patch, self.original, self.output = patch, original, output
        self.tokens, self.reverse, self.direct, self.formats = {}, {}, {}, []
        self.files, self.counts = {}, {'message_boxes': 0, 'textures': 0}

    def token(self, value):
        value = clean(value)
        if value.startswith('|'):
            left, span, ruby = value[1:].split(',', 2)
            # thpatch ruby uses tab-delimited text spans, not numeric offsets.
            if '\t' in left or '\t' in span:
                # The game's text API doubles these logical coordinates.
                left = str(len(left.replace('\t', '')) * 16)
                span = str(max(1, len(span.replace('\t', ''))) * 16 // max(1, len(ruby)))
            return f'|{left},{span},{self.token(ruby)}'
        if value not in self.reverse:
            key = f'~ZH{len(self.tokens):06d}~'
            self.tokens[key] = value
            self.reverse[value] = key
        return self.reverse[value]

    def load(self, filename):
        return json.loads((self.patch / filename).read_text(encoding='utf-8-sig'))

    def write(self, name, data):
        (self.output / name).write_bytes(data)
        self.files[name] = hashlib.sha256(data).hexdigest()

    def messages(self, filename):
        name = filename.name.removesuffix('.jdiff')
        source = (self.original / name).read_bytes()
        patch = self.load(filename.name)
        count = u32(source)
        assert 0 < count < 100 and 4 + count * 8 <= len(source)
        offsets = [u32(source, 4 + i * 8) for i in range(count)]
        out = bytearray(source[:4 + count * 8])
        seen = set()
        ending = name.startswith('e')
        line_op = 3 if ending else 17
        end_ops = {0, 5, 6, 9} if ending else {0, 7, 8, 9, 11}
        for entry, begin in enumerate(offsets):
            struct.pack_into('<I', out, 4 + entry * 8, len(out))
            stop = offsets[entry + 1] if entry + 1 < count else len(source)
            commands, at = [], begin
            while at < stop:
                time, op, size = struct.unpack_from('<HBB', source, at)
                assert at + 4 + size <= stop, (name, at)
                commands.append((time, op, source[at+4:at+4+size]))
                at += 4 + size
            groups, group = [], []
            for i, (_, op, _) in enumerate(commands):
                if op in end_ops and group:
                    groups.append(group); group = []
                if op == line_op:
                    group.append(i)
            if group:
                groups.append(group)
            replacements, deleted, times = {}, set(), {}
            for group in groups:
                time = commands[group[0]][0]
                index = times.get(time, 0); times[time] = index + 1
                key = f'{time}_{index}'
                edit = patch.get(str(entry), {}).get(key)
                if edit is None:
                    continue
                lines = edit['lines'] if isinstance(edit, dict) else edit
                assert isinstance(lines, list)
                visible = sum(not line.startswith('|') for line in lines)
                assert visible <= (5 if ending else 2), (name, key, 'too many lines')
                replacement = bytearray()
                for line in lines:
                    payload = crypt(self.token(line).encode('ascii') + b'\0')
                    assert len(payload) <= 255
                    replacement += struct.pack('<HBB', time, line_op, len(payload)) + payload
                replacements[group[0]] = replacement
                deleted.update(group[1:])
                seen.add((str(entry), key)); self.counts['message_boxes'] += 1
            for i, (time, op, payload) in enumerate(commands):
                if i in replacements:
                    out += replacements[i]
                elif i not in deleted and (ending or op != 25):
                    out += struct.pack('<HBB', time, op, len(payload)) + payload
        expected = {(entry, key) for entry, edits in patch.items() for key in edits}
        # The supplied pack retains alternate timecodes for older revisions.
        # Accept only unused entries identical to one matched in this entry.
        unmatched = {(e,k) for e,k in expected-seen if not any(
            e == se and patch[e][k] == patch[se][sk] for se,sk in seen)}
        if unmatched:
            raise ValueError(f'{name}: unmatched translation boxes: {unmatched}')
        self.write(name, out)

    def sections(self, name):
        text = (self.original / name).read_bytes().decode('cp932').replace('\r', '').replace('\f', '')
        result = []
        for line in text.splitlines():
            if line.startswith('\\'):
                break
            if line.startswith('@'):
                result.append([line[1:], []])
            elif result and line and not line.startswith('#'):
                result[-1][1].append(line)
        return result

    def alias(self, original, translated):
        if original.strip() and translated.strip() and original != translated:
            self.direct.setdefault(original, clean(translated))

    def text_tables(self):
        data = self.load('stonetext.js')
        labels = re.findall(r'"([^"\n]+)"', (Path(__file__).resolve().parents[2] / 'source_reconstruction/stone_menu/names.inc').read_text())
        rows = []
        for label, lines in self.sections('stonetext.txt'):
            index = labels.index(label)
            rows.append('@' + label)
            has_name = index >= 72 or index % 4 == 0
            if has_name:
                title = data.get(str(index), lines[0]); self.alias(lines[0], title)
                rows.append(self.token(title)); lines = lines[1:]
            translated = data.get(f'{index}_0', lines)
            if isinstance(translated, str): translated = [translated]
            assert len(translated) <= 5
            rows += [self.token(x or '　') for x in translated]
        self.write('stonetext.txt', ('\n'.join(rows) + '\n\\\n').encode('ascii'))
        data = self.load('trophy.js'); rows = []
        for key, lines in self.sections('trophy.txt'):
            assert len(lines) == 7, (key, len(lines))
            rows.append('@' + key); rows.append(self.token(data.get(key, lines[0])))
            for variant in range(2):
                translated = data.get(f'{key}_{variant}', lines[1 + variant*3:4 + variant*3])
                if isinstance(translated, str): translated = [translated]
                assert len(translated) <= 3
                translated = translated + ['　'] * (3-len(translated))
                rows += [self.token(x or '　') for x in translated]
        self.write('trophy.txt', ('\n'.join(rows) + '\n\\\n').encode('ascii'))
        data = self.load('musiccmt.js')
        themes = json.loads((self.patch.parent/'themes.js').read_text(encoding='utf-8-sig'))
        rows = []
        for index, (key, lines) in enumerate(self.sections('musiccmt.txt'), 1):
            assert len(lines) == 9, (key, len(lines))
            title = themes[f'th20_{index:02d}']
            self.alias(lines[0].split('  ', 1)[-1], title)
            comment = list(data[str(index)])
            while len(comment) > 8 and any(not x.strip() for x in comment):
                comment.pop(next(i for i,x in enumerate(comment) if not x.strip()))
            assert len(comment) <= 8
            comment = [f'♪{title}' if x == '@' else x for x in comment]
            rows += ['@'+key, self.token(f'No.{index:2d}  {title}')]
            rows += [self.token(x or '　') for x in comment + ['　']*(8-len(comment))]
        self.write('musiccmt.txt', ('\n'.join(rows)+'\n\\\n').encode('ascii'))
        spells = self.load('spells.js')
        # thpatch entries inherit forward until the next numbered translation.
        last = None; self.spells = {}
        for i in range(113):
            last = spells.get(str(i), last)
            if last is not None: self.spells[str(i)] = self.token(last)

    def static_strings(self):
        # Map thpatch's read-only string addresses to our recovered constants,
        # never to executable instructions. English texture labels are retained
        # when the supplied pack itself does not translate them.
        location_text = (self.patch.parents[2] / 'nmlgc/base_tsa/th20/stringlocs.v1.00a.js').read_text(encoding='utf-8-sig')
        locations = json.loads(re.sub(r',\s*}', '}', location_text))
        definitions = json.loads((self.patch.parent/'stringdefs.js').read_text(encoding='utf-8-sig'))
        constants = {}
        source = Path(__file__).resolve().parents[2] / 'source_reconstruction'
        for file in source.rglob('*'):
            if file.suffix not in {'.hpp', '.cpp', '.mm'}:
                continue
            for address, raw in re.findall(r'char s_([0-9a-fA-F]{8})\[\]\s*=\s*"((?:\\x[0-9a-fA-F]{2})*)"', file.read_text(encoding='utf-8')):
                constants[int(address, 16)] = bytes.fromhex(raw.replace('\\x', '')).decode('cp932')
        spec = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?(?:ll|l|h|z)?[diuxXfs]')
        count = 0
        for address, key in locations.items():
            original = constants.get(int(address[2:], 16) + 0x400000)
            if original is None or key not in definitions:
                continue
            translated = clean(definitions[key])
            if original == translated:
                continue
            if spec.search(original):
                before, after = list(spec.finditer(original)), list(spec.finditer(translated))
                if len(before) != len(after) or [m[0][-1] for m in before] != [m[0][-1] for m in after]:
                    raise ValueError(f'Format argument mismatch: {key}')
                pattern, cursor = '^', 0
                for match in before:
                    pattern += re.escape(original[cursor:match.start()]) + '(.*?)'
                    cursor = match.end()
                pattern += re.escape(original[cursor:]) + '$'
                pieces, cursor = [], 0
                for index, match in enumerate(after):
                    pieces += [translated[cursor:match.start()], index]
                    cursor = match.end()
                pieces.append(translated[cursor:])
                self.formats.append({'pattern': pattern, 'pieces': pieces, 'source': original, 'target': translated})
            else:
                self.alias(original, translated)
            count += 1
        for original, translated in {
            'Game Start': '开始游戏',
            'Extra Start': '开始 Extra 游戏',
            'Practice': '练习模式',
            'Spell Practice': '符卡练习',
            'Replay': '游戏录像',
            'Player Data': '玩家资料',
            'Music Room': '音乐室',
            'Option': '设置',
            'Manual': '操作说明',
            'Quit': '退出游戏',
        }.items():
            self.alias(original, translated)
            count += 1
        self.counts['static_strings'] = count

    def animations(self):
        used = set()
        for filename in sorted(self.original.glob('*.anm')):
            source = filename.read_bytes(); out = bytearray(); at = 0; changed = False
            while True:
                version, ns, nc = struct.unpack_from('<IHH', source, at)
                assert version == 8
                name_at, = struct.unpack_from('<I', source, at+16)
                name = source[at+name_at:source.index(0, at+name_at)].decode('ascii')
                tex, nxt = u32(source, at+28), u32(source, at+36)
                entry = bytearray(source[at:at+nxt if nxt else len(source)])
                patch = self.patch / name
                if patch.is_file() and source[at+32] and tex:
                    size = u32(entry, tex+12)
                    original = Image.open(io.BytesIO(entry[tex+16:tex+16+size])).convert('RGBA')
                    replacement = Image.open(patch).convert('RGBA')
                    x, y = struct.unpack_from('<hh', entry, 20)
                    assert x >= 0 and y >= 0 and replacement.width >= x+original.width and replacement.height >= y+original.height, (filename.name,name,original.size,replacement.size,x,y)
                    replacement = replacement.crop((x,y,x+original.width,y+original.height))
                    dst, rep = np.array(original), np.array(replacement)
                    rectangles = []
                    for i in range(ns):
                        off = u32(entry,64+i*4)
                        _,sx,sy,w,h = struct.unpack_from('<Iffff',entry,off)
                        rectangles.append((max(0,int(sx)),max(0,int(sy)),min(original.width,int(sx+w)),min(original.height,int(sy+h))))
                    if not rectangles: rectangles = [(0,0,original.width,original.height)]
                    # Use each original sprite's alpha, as thcrap's automatic blit
                    # mode does. Transparent areas outside translated sprites stay.
                    baseline = dst.copy()
                    for left,top,right,bottom in rectangles:
                        if right<=left or bottom<=top: continue
                        r=rep[top:bottom,left:right]; d=baseline[top:bottom,left:right]
                        if not r[:,:,3].any(): continue
                        if np.all(d[:,:,3] == 255):
                            d=d.astype(np.uint32);r=r.astype(np.uint32);a=r[:,:,3:4]
                            dst[top:bottom,left:right,:3]=((d[:,:,:3]*(255-a)+r[:,:,:3]*a)>>8).astype(np.uint8)
                            dst[top:bottom,left:right,3]=np.minimum(255,d[:,:,3]+r[:,:,3]).astype(np.uint8)
                        else: dst[top:bottom,left:right]=r
                    png=io.BytesIO();Image.fromarray(dst).save(png,format='PNG')
                    payload=png.getvalue();entry=entry[:tex+16]+payload
                    struct.pack_into('<I',entry,tex+12,len(payload))
                    entry += b'\0' * (-len(entry)%4)
                    changed=True;used.add(name);self.counts['textures']+=1
                struct.pack_into('<I',entry,36,len(entry) if nxt else 0)
                out += entry
                if not nxt: break
                at += nxt
            if changed: self.write(filename.name,out)
        expected={p.relative_to(self.patch).as_posix() for p in self.patch.rglob('*.png')}
        for name in sorted(expected-used):
            if re.fullmatch(r'help_0[1-9]\.png',name):
                self.write(name,(self.patch/name).read_bytes());used.add(name)
        # Trial-version watermark is not referenced by the full-game archive.
        expected.discard('title/title_ver_tr.png')
        if expected-used: raise ValueError(f'Unmatched PNG translations: {sorted(expected-used)}')

    def run(self):
        self.output.mkdir(parents=True,exist_ok=True)
        for file in sorted(self.patch.glob('*.msg.jdiff')): self.messages(file)
        self.text_tables(); self.static_strings(); self.animations()
        manifest={'version':1,'language':'zh-Hans','source':'thpatch lang_zh-hans, standalone pack 2025-09-06',
                  'tokens':self.tokens,'direct':self.direct,'formats':self.formats,'spells':self.spells,'files':self.files,'counts':self.counts}
        (self.output/'translations.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({'files':len(self.files),'strings':len(self.tokens),**self.counts}))


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('patch',type=Path,help='lang_zh-hans/th20 directory')
    parser.add_argument('original',type=Path,help='export_translation_sources output')
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    Compiler(args.patch,args.original,args.output).run()
