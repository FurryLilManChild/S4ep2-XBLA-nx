#!/usr/bin/env python3
"""TFU-nx (2026-11-11): the offline shader dump. Unpacks every level pack (LevelPacks/*.lp, LZX: tools/tfu_lp_unpack.c)
and writes each Xenos shader container in it to <out>/{vs,ps}_<xxh3_64>.bin, the names Renut's build reads its shader
cache from (tools/renut_shader_dump.py's scan and naming). Without it, every new area showed black until a run had
captured its shaders and a rebuild added them (the first level: 863 shaders, 67,000 to 153,000 draws skipped per 10 s).

  python3 tools/tfu_pack_shaders.py --unpacker out/tools/tfu_lp_unpack --packs assets/game_root/LevelPacks
                                    --out out/renut_shader_dump [--check out/pc/shader_capture]

--check: how many of the shaders a run captured the dump also has (the naming must agree with Renut's).
The unpacked packs are game data and go to a temporary folder; only the containers are kept, in out/ (not in git).
"""
import argparse
import glob
import os
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xxhash  # noqa: E402

from renut_shader_dump import find_shader_headers  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--unpacker', required=True)
    ap.add_argument('--packs', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--check', default='')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    before = set(n for n in os.listdir(a.out) if n.endswith('.bin'))
    names = set()
    packs = sorted(glob.glob(os.path.join(os.path.abspath(a.packs), '**', '*.lp'), recursive=True))
    incomplete = 0
    with tempfile.TemporaryDirectory() as tmp:
        unpacked = os.path.join(tmp, 'pack.bin')
        for i, pack in enumerate(packs):
            r = subprocess.run([os.path.abspath(a.unpacker), pack, unpacked], cwd=tmp, capture_output=True, text=True)
            if r.returncode:
                incomplete += 1
            if not os.path.exists(unpacked):
                print('%s: nothing unpacked' % os.path.basename(pack))
                continue
            data = open(unpacked, 'rb').read()
            found = 0
            for h in find_shader_headers(data, aligned=False):
                virt, phys = struct.unpack_from('>II', data, h + 4)
                blob = data[h:h + virt + phys]
                if len(blob) != virt + phys:
                    continue
                kind = 'vs' if struct.unpack_from('>I', data, h)[0] & 1 else 'ps'
                name = '%s_%s.bin' % (kind, xxhash.xxh3_64_hexdigest(blob))
                names.add(name)
                found += 1
                path = os.path.join(a.out, name)
                if not os.path.exists(path):
                    open(path, 'wb').write(blob)
            last = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else ''
            print('[%d/%d] %s: %d containers; %s' % (i + 1, len(packs), os.path.basename(pack), found,
                                                     last.split(': ', 1)[-1]))
            sys.stdout.flush()
            os.remove(unpacked)
    after = set(n for n in os.listdir(a.out) if n.endswith('.bin'))
    print('tfu_pack_shaders: %d distinct shaders in %d packs (%d packs not unpacked to the end); %d new in %s, %d '
          'there now' % (len(names), len(packs), incomplete, len(after - before), a.out, len(after)))
    if a.check and os.path.isdir(a.check):
        captured = set(n for n in os.listdir(a.check) if n.endswith('.bin'))
        print('check: %d of the %d shaders captured in %s are in the dump' % (len(captured & after), len(captured),
                                                                          a.check))


if __name__ == '__main__':
    sys.exit(main())
