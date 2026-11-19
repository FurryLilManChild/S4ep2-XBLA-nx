#!/usr/bin/env python3
"""S4ep1-nx (2026-11-18): the offline shader dump for Sonic 4 (TFU-nx's tools/tfu_pack_shaders.py, for data.cpk).

Sonic 4 keeps its shaders precompiled in data.cpk: NNSTDSHADER/SHADER.AMB (280 containers) and
DEMO/WORLDMAP/WORLDMAP.AMB (2). Only 8 are in default.xex. Release 0.5.0's cache had the image's shaders and what runs
had captured: 97 of the 282, so every draw with one of the other 185 was skipped (invisible cards, spheres, ropes and
enemies, GitHub issue 1). This writes every container in every file of data.cpk to <out>/{vs,ps}_<xxh3_64>.bin, the
names Renut's build reads its shader cache from (tools/renut_shader_dump.py's scan and naming).

  python3 -I tools/s4_cpk_shaders.py --cpk assets/extracted/data.cpk --out out/renut_shader_dump [--check <folder>]

--check: how many of the containers in a folder (a run's shader_capture) the dump has (the naming must agree).
The CPK is read in memory; only the containers are written, in out/ (not in git).
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xxhash  # noqa: E402

from cpk_extract import read_files  # noqa: E402
from renut_shader_dump import find_shader_headers  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cpk', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--check', default='')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    before = set(n for n in os.listdir(a.out) if n.endswith('.bin'))
    names = set()
    for fname, data in read_files(a.cpk):
        n = 0
        for h in find_shader_headers(data, aligned=False):
            virt, phys = struct.unpack_from('>II', data, h + 4)
            blob = data[h:h + virt + phys]
            if len(blob) != virt + phys:
                continue
            kind = 'vs' if struct.unpack_from('>I', data, h)[0] & 1 else 'ps'
            name = '%s_%s.bin' % (kind, xxhash.xxh3_64_hexdigest(blob))
            if name not in names:
                names.add(name)
                n += 1
            path = os.path.join(a.out, name)
            if not os.path.exists(path):
                open(path, 'wb').write(blob)
        if n:
            print('  %s: %d containers' % (fname, n))
    new = len(names - before)
    total = len([n for n in os.listdir(a.out) if n.endswith('.bin')])
    print('s4_cpk_shaders: %d containers in data.cpk, %d new to %s (%d there now)' % (len(names), new, a.out, total))
    if a.check:
        caught = set(n for n in os.listdir(a.check) if n.endswith('.bin'))
        print('check: %d of the %d containers in %s are in the dump' % (len(caught & names), len(caught), a.check))
    return 0


if __name__ == '__main__':
    sys.exit(main())
