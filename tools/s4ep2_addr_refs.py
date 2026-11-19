#!/usr/bin/env python3
"""S4ep2-XBLA-nx (2026-11-19): every game-data address in app/src on a line tagged S4ep2-XBLA-nx that also names a
function (sub_XXXXXXXX) must be built by that function's code (lis + a D-form with the low half, within 24
instructions, the function being its .pdata range). Found after kDeviceCapsVa was 0x820F80E0 instead of 0x821080E0
(a slip in adding lis and addi by hand): GetDeviceCaps copied game strings as caps and every D3DXCreateTexture failed.

  python3 tools/s4ep2_addr_refs.py [image]    prints MISMATCH lines and a count; exit 1 on any mismatch
"""
import bisect, glob, os, re, struct, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
img = sys.argv[1] if len(sys.argv) > 1 else ROOT + '/out/renut_image.bin'
d = open(img, 'rb').read(); BASE = 0x82000000
e = struct.unpack('<I', d[0x3c:0x40])[0]; nsec = struct.unpack('<H', d[e+6:e+8])[0]; opt = struct.unpack('<H', d[e+20:e+22])[0]
secs = {}
for i in range(nsec):
    s = d[e+24+opt+40*i:e+24+opt+40*i+40]
    secs[s[:8].rstrip(b'\0').decode()] = (struct.unpack('<I', s[12:16])[0], struct.unpack('<I', s[8:12])[0])
pva, pvs = secs['.pdata']
funcs = {}
for j in range(pva, pva + pvs, 8):
    b, w = struct.unpack('>II', d[j:j+8])
    if b == 0: break
    funcs[b] = b + ((w >> 8) & 0x3FFFFF) * 4

def built(lo, hi):
    out = set()
    for off in range(lo - BASE, hi - BASE, 4):
        i = struct.unpack('>I', d[off:off+4])[0]
        if i >> 26 == 15 and (i >> 16) & 31 == 0:
            rx = (i >> 21) & 31; h = i & 0xffff
            for k in range(1, 25):
                if off + 4*k >= hi - BASE: break
                j = struct.unpack('>I', d[off+4*k:off+4*k+4])[0]; op = j >> 26
                if op in (14, 24, 32, 34, 36, 38, 40, 44, 48, 50, 52, 54, 58, 62) and (j >> 16) & 31 == rx:
                    imm = j & 0xffff
                    if op == 24: out.add(((h << 16) | imm) & 0xffffffff)
                    else: out.add(((h << 16) + (imm - 0x10000 if imm & 0x8000 else imm)) & 0xffffffff)
                if op == 15 and (j >> 21) & 31 == rx: break
    return out

bad = n = 0
for f in glob.glob(ROOT + '/app/src/**/*.*', recursive=True):
    if not f.endswith(('.cpp', '.h')): continue
    for ln, line in enumerate(open(f, encoding='utf-8', errors='replace'), 1):
        if 'S4ep2-XBLA-nx' not in line: continue
        m = re.match(r'\s*constexpr\s+\w+\s+\w+\s*=\s*0x(82[0-9A-Fa-f]{6})u?;\s*//(.*)', line)
        if not m: continue
        a = int(m.group(1), 16)
        subs = [int(x, 16) for x in re.findall(r'sub_([0-9A-Fa-f]{8})', m.group(2))]
        # a leaf with no .pdata entry (GetDeviceCaps sub_824040C8): its first 64 instructions
        subs = [s for s in subs if BASE <= s < BASE + len(d)]
        if not subs: continue
        n += 1
        if not any(a in built(s, funcs.get(s, s + 256)) for s in subs):
            bad += 1
            print('MISMATCH %s:%d 0x%08X not built in %s' % (os.path.relpath(f, ROOT), ln, a, ' '.join('sub_%08X' % s for s in subs)))
print('checked %d addresses with a named function, %d mismatches' % (n, bad))
sys.exit(1 if bad else 0)
