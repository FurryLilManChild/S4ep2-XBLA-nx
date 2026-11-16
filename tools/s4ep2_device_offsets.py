#!/usr/bin/env python3
"""S4ep2-XBLA-nx (2026-11-16): moves Renut's device offsets from Episode I's layout (XDK 2.0.11626) to Episode II's
(XDK 2.0.20871). The rule is read off tools/s4ep2_field_votes.py over the aligned Direct3D pairs
(out/field_votes.txt): every voted field in a band moved by the same amount.

  below 0x2A80     +0    the register packets and constants (hardware layout; 2934, 2948 ... 42 and 19 votes)
  0x2A80..0x2AC0   +128  creation thread, fences, predication words, flags (2A90 21/21, 2ABD 21/21)
  0x2AC1..0x2AFF   +132  (2ACC, 2AEC, 2AF0)
  0x2B00..0x2C93   +140  (2B00, 2B04)
  0x2C94..0x2EDF   +248  (2C94, 2EC8, 2ED8 vertex declaration)
  0x2EE0..0x2F3F   +256  (2EF8 23/23, 2EFC 24/24)
  0x2F40..0x2FAB   +230  (2F5E, 2F78: halfword fields)
  0x2FAC..0x3047   +204  (2FAC..2FB8)
  0x3048..0x345F   +200  (index buffer .. tiling extent; 3148 9/9, 3158 12/12, 3254 5/5, 3268..327C 6/6)
  0x3460 and up    +192  (command buffer 35A0 4/4, swap count 4188 3/3, behaviour flags 5E88 6/6, side buffer 4230)

  python -I tools/s4ep2_device_offsets.py [--dry-run]
"""
import os, re, sys

BANDS = [(0x0, 0x2A80, 0), (0x2A80, 0x2AC1, 128), (0x2AC1, 0x2B00, 132), (0x2B00, 0x2C94, 140), (0x2C94, 0x2EE0, 248),
         (0x2EE0, 0x2F40, 256), (0x2F40, 0x2FAC, 230), (0x2FAC, 0x3048, 204), (0x3048, 0x3460, 200), (0x3460, 0x7000, 192)]


def move(v):
    for lo, hi, d in BANDS:
        if lo <= v < hi:
            return v + d
    return v


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = ['app/src/renut_engine/gpu/d3d/d3d.h', 'app/src/renut_engine/gpu/d3d/d3d_native.h']
LINE = re.compile(r'^(\s*constexpr \w+ (\w+)\s*=\s*)(0x[0-9A-Fa-f]+|\d+)(u?;)(.*)$')
dry = '--dry-run' in sys.argv
for rel in FILES:
    p = os.path.join(ROOT, rel)
    src = open(p, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in src else '\n'
    out = []
    for line in src.split(nl):
        m = LINE.match(line)
        if m:
            name, lit = m.group(2), m.group(3)
            v = int(lit, 0)
            if 0x2A80 <= v < 0x7000 and not name.startswith(('kStateTag',)):
                w = move(v)
                if w != v:
                    new = f'0x{w:X}' if lit.lower().startswith('0x') else str(w)
                    line = f'{m.group(1)}{new}{m.group(4)}{m.group(5)} (S4ep2-XBLA-nx: offset 0x{v:X}->0x{w:X})'
                    print(f'{rel}: {name} 0x{v:X} -> 0x{w:X}')
        out.append(line)
    if not dry:
        open(p, 'w', encoding='utf-8', newline='').write(nl.join(out))
