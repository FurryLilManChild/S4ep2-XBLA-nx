#!/usr/bin/env python3
"""S4ep1-nx: unpack a normal-compression (LZX) XEX2 to its PE image, for scanning.

Usage: python tools/xex_unpack.py default.xex image.pe
Needs tools/xexunpack/lzxd_mem built first (see tools/xexunpack/README.md). Reads only.
"""
import os
import struct
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
xex, out = sys.argv[1], sys.argv[2]
subprocess.check_call([sys.executable, '-I', os.path.join(here, 'xexscan.py'), xex, out])
d = open(xex, 'rb').read()
nopt = struct.unpack('>I', d[20:24])[0]
opts = dict(struct.unpack('>II', d[24 + i * 8:32 + i * 8]) for i in range(nopt))
ff = opts[0x3FF]
window, bs = struct.unpack('>II', d[ff + 8:ff + 16])
s = open(out + '.lzx', 'rb').read()
p, raw = 0, bytearray()
while bs:
    blk = s[p:p + bs]
    nbs, q = struct.unpack('>I', blk[:4])[0], 24
    while q < len(blk):
        c = struct.unpack('>H', blk[q:q + 2])[0]
        q += 2
        if c == 0:
            break
        raw += blk[q:q + c]
        q += c
    p, bs = p + bs, nbs
open(out + '.lzxraw', 'wb').write(raw)
sec = struct.unpack('>I', d[16:20])[0]
size = struct.unpack('>I', d[sec + 4:sec + 8])[0]
subprocess.check_call([os.path.join(here, 'xexunpack', 'lzxd_mem'), out + '.lzxraw', out, '%x' % window, '%x' % size])
os.remove(out + '.lzx')
os.remove(out + '.lzxraw')
