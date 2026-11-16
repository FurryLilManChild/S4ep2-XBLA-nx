#!/usr/bin/env python3
"""TFU-nx (2026-11-11): rewrites Renut's Direct3D device offsets from Beautiful Katamari's layout (XDK 2.0.5632, the
layout katamari-nx's Renut uses) to Star Wars: The Force Unleashed's (XDK 2.0.6534). docs/renut-map.md, "Device
layout", has the evidence for every zone (tools/renut_field_diff.py over the 316 paired functions, plus the tiling
pair, the device init 0x824E18D0 / 0x822E18E0... see the doc).

Every integer literal in code (not in comments or strings) of app/src/renut_engine that falls in 10944..22271
(decimal) or 0x2AC0..0x56FF (hex) is moved by its zone. Lines already moved carry `TFU-nx: offset` and are skipped.

  python -I tools/tfu_device_offsets.py [--dry-run]
"""
import argparse
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'app', 'src', 'renut_engine')
NUM = re.compile(r'(?<![0-9A-Za-z_.])(0x[0-9A-Fa-f]+|\d+)((?:u|U)?)(?![0-9A-Za-z_.])')
STR = re.compile(r'"(?:\\.|[^"\\])*"')

ZONES = [  # Katamari offset range (inclusive) -> delta
    (10944, 13180, 8),     # 11000 -> 11008 ... BeginTiling's flags 13180 -> 13188
    (13181, 13460, 16),    # tiling count 13184 -> 13200 ... VerticesRingEnd 13428 -> 13444
    (13461, 21600, 24),    # command buffer 13492 -> 13516 ... Swap 21596 -> 21620, device init 21560 -> 21584
    (21601, 22255, 28),    # 21640 -> 21668, the 41 -1 words 21672 -> 21700, gamma 22248 -> 22276
    (22256, 22271, 36),    # 22260 -> 22296, BehaviorFlags 22264 -> 22300
]
EXPLICIT = {0x5700: 0x5780}  # the device allocation: TFU's CreateDevice `li r3,22400`
KEEP = {
    0x2B00,   # Renut's own split of the Device struct (public part), not a guest field
    11776,    # a 64-byte chunk boundary of the replay delta compare: still the last before TFU's declaration (+11820)
    0x3C00, 0x3FFF, 0x4000, 12000, 15000, 16000, 16383, 16384, 20000,  # masks, sizes and timings
}


def tfu_offset(v):
    if v in EXPLICIT:
        return EXPLICIT[v]
    for lo, hi, d in ZONES:
        if lo <= v <= hi:
            return v + d
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()
    report = []
    for root, _, files in os.walk(SRC):
        for f in sorted(files):
            if not f.endswith(('.cpp', '.h')):
                continue
            path = os.path.join(root, f)
            raw = open(path, encoding='utf-8', errors='surrogateescape', newline='').read()
            nl = '\r\n' if '\r\n' in raw else '\n'
            lines = raw.split(nl)
            changed = False
            in_block = False
            for i, line in enumerate(lines):
                if 'TFU-nx: offset' in line:
                    continue
                # Code part: drop /* */ blocks and // comments, keep string positions out of the match.
                code = line
                if in_block:
                    end = code.find('*/')
                    if end < 0:
                        continue
                    in_block = False
                    code = ' ' * (end + 2) + code[end + 2:]
                start = code.find('/*')
                while start >= 0:
                    end = code.find('*/', start + 2)
                    if end < 0:
                        code = code[:start]
                        in_block = True
                        break
                    code = code[:start] + ' ' * (end + 2 - start) + code[end + 2:]
                    start = code.find('/*')
                cut = code.find('//')
                if cut >= 0:
                    code = code[:cut]
                code = STR.sub(lambda m: ' ' * len(m.group(0)), code)
                what = []
                pieces = []
                last = 0
                for m in NUM.finditer(code):
                    s, suffix = m.group(1), m.group(2)
                    hexa = s.lower().startswith('0x')
                    v = int(s, 16) if hexa else int(s)
                    if v in KEEP or v in (0x5700,) and False:
                        continue
                    in_range = (0x2AC0 <= v <= 0x56FF or v == 0x5700) if hexa else (10944 <= v <= 22271 or v == 22272)
                    if not in_range:
                        continue
                    t = tfu_offset(v)
                    if t is None or t == v:
                        continue
                    new = ('0x%X' % t) if hexa else str(t)
                    pieces.append(line[last:m.start(1)] + new)
                    last = m.end(1)
                    what.append(f'{s}->{new}')
                if what:
                    pieces.append(line[last:])
                    new_line = ''.join(pieces)
                    note = 'TFU-nx: offset ' + ', '.join(what)
                    new_line += ('  // ' + note) if '//' not in line else ' (' + note + ')'
                    lines[i] = new_line
                    changed = True
                    report.append(f'{os.path.relpath(path, SRC)}:{i + 1}: {", ".join(what)}')
            if changed and not a.dry_run:
                open(path, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(nl.join(lines))
    out = os.path.join(ROOT, 'out', 'renut_map', 'tfu_offsets_report.txt')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, 'w').write('\n'.join(report) + '\n')
    print(f'{len(report)} lines {"would change" if a.dry_run else "changed"}; report {out}')


if __name__ == '__main__':
    main()
