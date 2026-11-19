# S4ep2-XBLA-nx: map an Episode I device offset to Episode II by the field votes over the paired Direct3D functions
# (out/field_votes.txt) where a field has them, else by tools/s4ep2_device_offsets.py's bands; and check every
# "S4ep2-XBLA-nx: offset A->B" tag in app/src against that.
import re, os, sys, glob
sys.path.insert(0, os.path.dirname(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BANDS = [(0x0, 0x2A80, 0), (0x2A80, 0x2AC1, 128), (0x2AC1, 0x2B00, 132), (0x2B00, 0x2C94, 140), (0x2C94, 0x2EE0, 248),
         (0x2EE0, 0x2F40, 256), (0x2F40, 0x2FAC, 230), (0x2FAC, 0x3048, 204), (0x3048, 0x3460, 200), (0x3460, 0x7000, 192)]
VOTES = {}
for l in open(os.path.join(ROOT, 'out/field_votes.txt')):
    f = l.split()
    m = re.match(r'\d+ (0x[0-9A-F]+) -> -?\d+ (0x[0-9A-F]+) delta [-+]\d+  (\d+)/(\d+)', l)
    if not m:
        continue
    a, b, good, total = int(m.group(1), 16), int(m.group(2), 16), int(m.group(3)), int(m.group(4))
    if good == total and (total >= 2 or a >= 0x2A80):
        VOTES[a] = b
def move(v):
    if v in VOTES:
        return VOTES[v]
    # an array element: the nearest voted field at or below, if within 0x20 and the votes around agree with a band
    for lo, hi, d in BANDS:
        if lo <= v < hi:
            return v + d
    return v
if __name__ == '__main__':
    bad = 0
    for f in glob.glob(os.path.join(ROOT, 'app/src/**/*.*'), recursive=True):
        for k, line in enumerate(open(f, encoding='utf-8', errors='replace'), 1):
            for m in re.finditer(r'S4ep2-XBLA-nx: offsets? (?:moved )?(0x[0-9A-Fa-f]+|\d+)->(0X[0-9A-Fa-f]+|0x[0-9A-Fa-f]+|\d+)', line):
                a, b = int(m.group(1), 0), int(m.group(2).lower(), 0)
                if move(a) != b:
                    bad += 1
                    print(f'{os.path.relpath(f, ROOT)}:{k}: tagged {a:#x}->{b:#x}, evidence says {move(a):#x}')
    print('mismatches', bad)
