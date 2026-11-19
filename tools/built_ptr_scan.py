#!/usr/bin/env python3
"""S4ep1-nx (2026-11-18, GitHub issue 1): code addresses BUILT in code (lis rX,hi, then within 24 instructions
addi/ori rY,rX,lo, with other instructions between) that land in .text outside every .pdata function and are not
declared in app/overrides.toml. tools/pointer_fixpoint.py only takes pointers stored in data, so a callback whose
address is built this way and stored at run time (0x8210C638, Mad Gear's crash) was never registered.

  python3 -I tools/built_ptr_scan.py <image.pe from xex_unpack.py> app/overrides.toml [--registered <list>]

--registered: a file of the addresses the generator registered (one 0x... per line, from generated/renut_init.cpp's
table); with it, only the unregistered hits are printed. Switch table bases (the build is followed by add and mtctr)
are left out. Check every hit by hand (tools/ppc_dis.py): a real callback is stored, passed or called through CTR.
"""
import struct, sys, re
img, overrides = sys.argv[1], sys.argv[2]
registered = None
if '--registered' in sys.argv:
    registered = set(int(l, 16) for l in open(sys.argv[sys.argv.index('--registered') + 1]) if l.strip())
d = open(img, 'rb').read()
BASE = 0x82000000
e = struct.unpack('<I', d[0x3c:0x40])[0]
nsec = struct.unpack('<H', d[e+6:e+8])[0]; opt = struct.unpack('<H', d[e+20:e+22])[0]
secs = {}
for i in range(nsec):
    s = d[e+24+opt+40*i:e+24+opt+40*i+40]
    secs[s[:8].rstrip(b'\0').decode()] = (struct.unpack('<I', s[12:16])[0], struct.unpack('<I', s[8:12])[0])
tva, tvs = secs['.text']; pva, pvs = secs['.pdata']
funcs = []
for j in range(pva, pva+pvs, 8):
    b, w = struct.unpack('>II', d[j:j+8])
    if b == 0: break
    funcs.append((b, b + ((w >> 8) & 0x3FFFFF) * 4))
funcs.sort()
import bisect
starts = [f[0] for f in funcs]
def covered(a):
    i = bisect.bisect_right(starts, a) - 1
    return i >= 0 and funcs[i][0] <= a < funcs[i][1]
declared = set(int(m, 16) for m in re.findall(r'"0x([0-9A-Fa-f]{8})"', open(overrides).read()))
T_LO, T_HI = BASE + tva, BASE + tva + tvs
hits = {}
W = 24
for off in range(tva, tva + tvs, 4):
    w = struct.unpack('>I', d[off:off+4])[0]
    if w >> 26 != 15 or (w >> 16) & 31 != 0:  # lis = addis rD,0,imm
        continue
    rd = (w >> 21) & 31; hi = (w & 0xFFFF) << 16
    for k in range(1, W):
        o2 = off + 4*k
        w2 = struct.unpack('>I', d[o2:o2+4])[0]
        op = w2 >> 26; rt = (w2 >> 21) & 31; ra = (w2 >> 16) & 31
        if op in (14, 24) and ((ra == rd and op == 14) or (rt == rd and op == 24)):
            lo = w2 & 0xFFFF
            tgt = (hi + (lo - 0x10000 if (op == 14 and lo & 0x8000) else lo)) & 0xFFFFFFFF
            follow = [struct.unpack('>I', d[o2 + 4*j:o2 + 4*j + 4])[0] for j in range(1, 5)]
            switch_base = any(f == 0x7D8903A6 for f in follow)  # mtctr r12 right after: a switch table base
            if T_LO <= tgt < T_HI and tgt % 4 == 0 and not covered(tgt) and tgt not in declared and tgt not in starts \
                    and not switch_base and (registered is None or tgt not in registered):
                hits.setdefault(tgt, []).append(BASE + o2)
        # stop when rd is overwritten (rough: any D-form writing rd other than the uses)
        if op in (14, 15, 32, 24, 25) and rt == rd and not (op == 14 and ra == rd):
            break
        if w2 == 0x4E800020 or op == 18:  # blr / b
            break
for t in sorted(hits):
    print(hex(t), 'built at', ' '.join(hex(x) for x in hits[t]))
print(len(hits), 'candidates', file=sys.stderr)
