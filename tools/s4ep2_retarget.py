#!/usr/bin/env python3
"""S4ep2-XBLA-nx (2026-11-16): moves Renut's Direct3D hooks (app/src/renut_engine/gpu/d3d/hooks/*.cpp) from Episode I's
addresses to Episode II's. Pairs: tools/d3d_align.py over the Direct3D block (out/align_pairs.txt, ratio >= 0.6; the
library is linked in the same order, so the alignment places the tiny render-state accessors d3d_match cannot), then
MANUAL (functions LTCG moved out of order, each with its evidence). Registered hook lines whose Episode I address has
no pair are compiled unregistered (BANJO_RENUT_UNHOOKED_HOOK...). Lines already carrying "S4ep2-XBLA-nx:" are skipped.

  python -I tools/s4ep2_retarget.py [--dry-run]
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HOOKS = os.path.join(ROOT, 'app', 'src', 'renut_engine', 'gpu', 'd3d', 'hooks')
MANUAL = {  # Episode I: (Episode II, evidence)
    '821C2BC0': ('823FC190', 'SetPredication: d3d_match 1.00; in Episode II it is linked before Resolve'),
    '821C2D30': ('823FC300', 'BeginTiling: d3d_match 0.75 (next 0.19), linked after SetPredication as in Episode I'),
    '821C30D0': ('823FC678', 'EndTiling: d3d_match 0.92 (next 0.19), right after BeginTiling'),
    '821C6B80': ('824040C8', 'GetDeviceCaps: 1.00 between its aligned neighbours'),
    '821C6B98': ('824040E0', 'SetScreenExtentQueryMode: 1.00 between its aligned neighbours'),
    '82494160': ('828631F0', 'Direct3D_CreateDevice: d3d_match 0.97 (next 0.42); clears 0x6080 bytes (Episode I 0x5F00)'),
    '82488958': ('828604B8', 'the frame pacer step: d3d_match 0.94 (next 0.30)'),
}
pairs = {}
for line in open(os.path.join(ROOT, 'out', 'align_pairs.txt')):
    x, y, r = line.split()
    if float(r) >= 0.6:
        pairs[x[4:]] = (y[4:], f'aligned {r}')
for k, v in MANUAL.items():
    pairs[k] = v
LINE = re.compile(r'^(BANJO_RENUT_(?:UNHOOKED_)?HOOK(?:_SHARED|_RAW|_RAW_SHARED)?)\(sub_([0-9A-F]{8})(.*)$')
dry = '--dry-run' in sys.argv
counts = {'moved': 0, 'unhooked': 0, 'kept unhooked': 0}
used = {}
for f in sorted(os.listdir(HOOKS)):
    if not f.endswith('.cpp'):
        continue
    p = os.path.join(HOOKS, f)
    lines = open(p, encoding='utf-8', newline='').read().split('\n')
    for i, line in enumerate(lines):
        m = LINE.match(line)
        if not m or 'S4ep2-XBLA-nx:' in line:
            continue
        macro, ep1, rest = m.groups()
        if 'UNHOOKED' in macro:
            counts['kept unhooked'] += 1
            continue
        if ep1 in pairs:
            ep2, how = pairs[ep1]
            if ep2 in used:
                print(f'{f}: {ep1} -> {ep2} already used by {used[ep2]}: unhooked')
                lines[i] = f'{macro.replace("BANJO_RENUT_", "BANJO_RENUT_UNHOOKED_")}(sub_{ep1}{rest}  // S4ep2-XBLA-nx: duplicate pair'
                counts['unhooked'] += 1
                continue
            used[ep2] = ep1
            lines[i] = f'{macro}(sub_{ep2}{rest}  // S4ep2-XBLA-nx: Episode I sub_{ep1}, {how}'
            counts['moved'] += 1
        else:
            lines[i] = f'{macro.replace("BANJO_RENUT_", "BANJO_RENUT_UNHOOKED_")}(sub_{ep1}{rest}  // S4ep2-XBLA-nx: no Episode II pair'
            counts['unhooked'] += 1
            print(f'{f}: {ep1} unhooked ({rest.strip()[:50]})')
    if not dry:
        open(p, 'w', encoding='utf-8', newline='').write('\n'.join(lines))
print(counts)
