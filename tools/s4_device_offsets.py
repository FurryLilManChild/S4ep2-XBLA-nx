#!/usr/bin/env python3
"""S4ep1-nx (2026-11-14): rewrites Renut's Direct3D device offsets from TFU's layout (XDK 2.0.6534, TFU-nx's Renut) to
Sonic 4's (XDK 2.0.11626, the same as LoS1's). docs/renut-map.md, "Device layout", has the evidence for every zone:
tools/renut_field_diff.py over 446 paired functions, LoS1-nx's d3d.h names, and field-by-field reads of Sonic 4's own
ClearF, BeginTiling, EndTiling, XenonGpu_Initialize, the device init, InitializePresentationParameters, SetPWLGamma and
Swap against TFU's.

Every integer literal in code (not in comments or strings) of app/src/renut_engine that falls in 11000..22400
(decimal) or 0x2AF8..0x5780 (hex) is moved by its zone. Lines already moved carry `S4ep1-nx: offset` and are skipped.
Below 11000 (the public device and the private words to 10943) nothing moved.

  python -I tools/s4_device_offsets.py [--dry-run]
"""
import argparse
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'app', 'src', 'renut_engine')
NUM = re.compile(r'(?<![0-9A-Za-z_.])(0x[0-9A-Fa-f]+|\d+)((?:u|U)?)(?![0-9A-Za-z_.])')
STR = re.compile(r'"(?:\\.|[^"\\])*"')

ZONES = [  # TFU offset range (inclusive) -> delta
    (11821, 13299, 176),   # declaration's neighbours 0x2E30 (XenonGpu_Initialize std 0x2E30 -> 0x2EE0) .. tiling, KickOff
    (13300, 13459, 208),   # VerticesRingEnd 13444 -> 13652 (LoS1)
    (13460, 15359, 212),   # command buffer 13516 -> 13728, presentation interval, present fence, gamma ramp (LoS1)
    (16500, 16799, 216),   # swap count 16560 -> 16776, side buffer 16728 -> 16944 (LoS1)
    (21500, 21539, 216),   # Swap's +0x541C test 21532 -> 21748
    (21540, 21640, 220),   # display mode 0x5424.. -> 0x5500.. (InitializePresentationParameters), -1 words 21556/21560
                           # -> 21776/21780 and perf frequency 21584 -> 21804 (device init)
    (21641, 22399, 1900),  # Swap's 21668 -> 23568, the 41 -1 words 21700 -> 23600 (device init: li 0x29 at +0x5C30),
                           # gamma pending 22272/22276 -> 24172/24176 (SetPWLGamma, Swap), BehaviorFlags 22300 -> 24200
]
EXPLICIT = {
    11772: 11772,   # kStateTagChunks' start: kept (the range is widened instead, below)
    11820: 11992,   # the vertex declaration (+172; LoS1 11992)
    0x5780: 0x5F00,  # the device allocation (LoS1, Direct3D_CreateDevice)
}
KEEP = {
    0x2B00,   # Renut's own split of the Device struct (public part), not a guest field
    11776,    # a 64-byte chunk boundary (queued.cpp, draw.h): still before Sonic 4's declaration (11992); kept (safe side)
    12000, 15000, 15360, 16000, 16383, 16384, 20000,  # masks, sizes and timings
}


def s4_offset(v):
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
                if 'S4ep1-nx: offset' in line:
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
                    if v in KEEP:
                        continue
                    in_range = (0x2AF8 <= v <= 0x5780) if hexa else (11000 <= v <= 22400)
                    if not in_range:
                        continue
                    t = s4_offset(v)
                    if t is None or t == v:
                        continue
                    new = ('0x%X' % t) if hexa else str(t)
                    pieces.append(line[last:m.start(1)] + new)
                    last = m.end(1)
                    what.append(f'{s}->{new}')
                if what:
                    pieces.append(line[last:])
                    new_line = ''.join(pieces)
                    note = 'S4ep1-nx: offset ' + ', '.join(what)
                    new_line += ('  // ' + note) if '//' not in line else ' (' + note + ')'
                    lines[i] = new_line
                    changed = True
                    report.append(f'{os.path.relpath(path, SRC)}:{i + 1}: {", ".join(what)}')
            if changed and not a.dry_run:
                open(path, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(nl.join(lines))
    out = os.path.join(ROOT, 'out', 'renut_map', 's4_offsets_report.txt')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, 'w').write('\n'.join(report) + '\n')
    print(f'{len(report)} lines {"would change" if a.dry_run else "changed"}; report {out}')


if __name__ == '__main__':
    main()
