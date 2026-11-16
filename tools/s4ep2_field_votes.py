#!/usr/bin/env python3
"""S4ep2-XBLA-nx (2026-11-16): votes for each Episode I device offset's Episode II value, over aligned function pairs
(tools/d3d_align.py --pairs-out) whose displacement lists have the same length and opcodes. Unchanged offsets vote
too, so a field that did not move shows as X -> X.

  python -I tools/s4ep2_field_votes.py <ep1 generated> <ep2 generated> <pairs> > out/field_votes.txt
"""
import collections, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from renut_sigmatch import load_tree
from renut_field_diff import disps

ep1, ep2 = load_tree(sys.argv[1]), load_tree(sys.argv[2])
votes = collections.defaultdict(collections.Counter)
for line in open(sys.argv[3]):
    x, y = line.split()[:2]
    if x not in ep1 or y not in ep2:
        continue
    dx, dy = disps(ep1[x][4]), disps(ep2[y][4])
    if len(dx) != len(dy) or any(p[0] != q[0] for p, q in zip(dx, dy)):
        continue
    for p, q in zip(dx, dy):
        if p[1] >= 0x2000:
            votes[p[1]][q[1]] += 1
for k in sorted(votes):
    c = votes[k]
    best, n = c.most_common(1)[0]
    tot = sum(c.values())
    print(f'{k} 0x{k:X} -> {best} 0x{best:X} delta {best - k:+d}  {n}/{tot}' + ('' if n == tot else f'  others {dict(c.most_common()[1:4])}'))
