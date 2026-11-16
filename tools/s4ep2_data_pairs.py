#!/usr/bin/env python3
"""S4ep2-XBLA-nx (2026-11-16): finds Episode II's address for each Episode I global that Renut names (device holder,
present parameters, D3D tables). In every paired function (out/align_pairs.txt plus extra pairs given) it finds the
instruction that forms the Episode I address (lis + addi/load/store displacement) and reads the address the same
instruction forms in the Episode II function, at the same position.

  python -I tools/s4ep2_data_pairs.py EP1_IMAGE EP2_IMAGE PAIRS ADDR... [--extra a:b ...]
"""
import struct, sys
BASE = 0x82000000


def insns(img, a, n=600):
    off = a - BASE
    return [struct.unpack_from('>I', img, off + 4 * k)[0] for k in range(n) if off + 4 * k + 4 <= len(img)]


def forms(ws):
    """(index, address) for each D-form whose base register was set by a lis earlier in the function."""
    hi = {}
    out = []
    for k, w in enumerate(ws):
        op = w >> 26
        rt, ra, d = (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
        if d & 0x8000:
            d -= 0x10000
        if op == 15 and ra == 0:  # lis
            hi[rt] = (w & 0xFFFF) << 16
            continue
        if op in (14, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55) and ra in hi:
            out.append((k, (hi[ra] + d) & 0xFFFFFFFF))
            if op == 14:
                hi[rt] = (hi[ra] + d) & 0xFFFFFFFF if rt != ra else hi[ra] + d
        if op not in (14, 15) and op in (32, 34, 40, 42, 33, 35, 41, 43, 46, 48, 50) and rt in hi:
            del hi[rt]
    return out


e1 = open(sys.argv[1], 'rb').read()
e2 = open(sys.argv[2], 'rb').read()
pairs = []
for line in open(sys.argv[3]):
    x, y = line.split()[:2]
    pairs.append((int(x[4:], 16), int(y[4:], 16)))
args = sys.argv[4:]
targets = []
i = 0
while i < len(args):
    if args[i] == '--extra':
        a, b = args[i + 1].split(':')
        pairs.append((int(a, 16), int(b, 16)))
        i += 2
    else:
        targets.append(int(args[i], 16))
        i += 1
for t in targets:
    votes = {}
    for a, b in pairs:
        f1 = forms(insns(e1, a))
        hits = [k for k, v in f1 if v == t]
        if not hits:
            continue
        f2 = dict(forms(insns(e2, b)))
        for k in hits:
            if k in f2:
                votes.setdefault(f2[k], []).append(f'{a:08X}')
    best = sorted(votes.items(), key=lambda kv: -len(kv[1]))
    print(f'{t:08X}: ' + '; '.join(f'{v:08X} x{len(w)} ({",".join(w[:3])})' for v, w in best[:3]) if best else f'{t:08X}: no pair forms it')
