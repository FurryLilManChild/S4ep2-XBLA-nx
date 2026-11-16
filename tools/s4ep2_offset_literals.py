#!/usr/bin/env python3
"""S4ep2-XBLA-nx (2026-11-16): moves the device-offset literals in Renut's .cpp sources (the lines
tools/renut_offset_scan.py reports) from Episode I's layout to Episode II's, with tools/s4ep2_device_offsets.py's
bands. Masks and comments are left alone (SKIP). Each changed line gets a trailing "// S4ep2-XBLA-nx: offsets moved".

  python -I tools/s4ep2_offset_literals.py [--dry-run]
"""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GPU = os.path.join(ROOT, 'app', 'src', 'renut_engine', 'gpu')
import importlib.util
spec = importlib.util.spec_from_file_location('dev', os.path.join(ROOT, 'tools', 's4ep2_device_offsets.py'))
BANDS = None
src = open(os.path.join(ROOT, 'tools', 's4ep2_device_offsets.py')).read()
BANDS = eval(re.search(r'BANDS = (\[.*?\])\n', src, re.S).group(1))


def move(v):
    for lo, hi, d in BANDS:
        if lo <= v < hi:
            return v + d
    return v


SKIP_FILES = {'d3d/d3d.h', 'd3d/d3d_native.h', 'backend/queued.cpp', 'guest/photo_capture.cpp'}
SKIP_LITERALS = {'0x3FFF', '0x3C00'}
NUM = re.compile(r'(?<![0-9A-Za-z_.])(0x[0-9A-Fa-f]+|\d+)(?:[uU])?(?![0-9A-Za-z_.])')
out = subprocess.run([sys.executable, '-I', os.path.join(ROOT, 'tools', 'renut_offset_scan.py')], capture_output=True,
                     text=True).stdout
targets = {}
for line in out.splitlines():
    m = re.match(r'([^:]+):(\d+):', line)
    if m and m.group(1) not in SKIP_FILES:
        targets.setdefault(m.group(1), set()).add(int(m.group(2)))
dry = '--dry-run' in sys.argv
for rel, lines in sorted(targets.items()):
    p = os.path.join(GPU, rel)
    text = open(p, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in text else '\n'
    L = text.split(nl)
    for n in sorted(lines):
        code, sep, comment = L[n - 1].partition('//')
        changed = []

        def rep(m):
            lit = m.group(1)
            if lit in SKIP_LITERALS:
                return m.group(0)
            v = int(lit, 0)
            if not (0x2A80 <= v < 0x7000):
                return m.group(0)
            w = move(v)
            if w == v:
                return m.group(0)
            changed.append(f'{lit}->{w:#X}' if lit.startswith('0x') else f'{lit}->{w}')
            new = f'0x{w:X}' if lit.lower().startswith('0x') else str(w)
            return m.group(0).replace(lit, new, 1)
        code2 = NUM.sub(rep, code)
        if changed:
            tag = 'S4ep2-XBLA-nx: offsets moved ' + ', '.join(changed)
            L[n - 1] = code2 + sep + comment + (' (' + tag + ')' if sep else '  // ' + tag)
            print(f'{rel}:{n}: {", ".join(changed)}')
    if not dry:
        open(p, 'w', encoding='utf-8', newline='').write(nl.join(L))
