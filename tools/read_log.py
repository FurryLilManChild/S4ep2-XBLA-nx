#!/usr/bin/env python3
"""Gate 3 (S4ep2-XBLA-nx, 2026-11-19): the facts of a test, read the same way every time.

Claude runs this on Mark's logs before saying anything about a test. It prints the build's commit first and stops with
"WRONG BUILD" when it is not the commit Claude expects (the 2026-11-19 mistake: two rounds read on an older build),
then the facts a report may rest on: how the run ended, crashes (with the host PC for llvm-symbolizer), bad calls,
frame rate and draws, shaders missing or dropped draws, the shader counts the build took from the game's files, and the
diagnostics present. Standard library only (it runs on Mark's PC through the device shell).

  python3 tools/read_log.py --logs out/logs --expect <commit> [--log out/logs/boot_latest.log]
"""
import argparse
import os
import re
import sys


def first(lines, pat):
    for l in lines:
        m = re.search(pat, l)
        if m:
            return m
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--logs', required=True, help='the out/logs folder')
    ap.add_argument('--log', default='', help='the run log (default: boot_latest.log there)')
    ap.add_argument('--expect', required=True, help='the commit Claude last pushed (first 8+ hex digits)')
    a = ap.parse_args()
    path = a.log or os.path.join(a.logs, 'boot_latest.log')
    lines = open(path, encoding='utf-8', errors='replace').read().splitlines()

    print('== log %s (%d lines)' % (path, len(lines)))
    m = first(lines, r'pc build ([0-9a-f]{7,}), built ([^\s]+ [^\s]+)')
    built = m.group(1) if m else ''
    if not built:
        print('WRONG BUILD: no build stamp in the log. Stop: nothing below is about a known build.')
        return 2
    if not (built.startswith(a.expect[:len(built)]) or a.expect.startswith(built)):
        print('WRONG BUILD: the log is from %s, the latest push is %s. Stop: nothing below is about the latest push.'
              % (built, a.expect))
        return 2
    print('build: %s (built %s UTC), matches the latest push %s' % (built, m.group(2), a.expect))

    # Build-side facts: the shaders the build took from the game's own files.
    for name, pat in (('cpk_shaders.log', r's4_cpk_shaders: (\d+) containers in data\.cpk, (\d+) new .*\((\d+) there now\)'),
                      ('shader_dump.log', r'renut_shader_dump: (\d+) vertex and (\d+) pixel shader containers in the image')):
        p = os.path.join(a.logs, name)
        if not os.path.exists(p):
            print('shaders: %s missing (that step did not run in this build)' % name)
            continue
        mm = first(open(p, encoding='utf-8', errors='replace').read().splitlines(), pat)
        cfg = os.path.join(a.logs, 'pc_configure.log')
        stale = os.path.exists(cfg) and os.path.getmtime(p) < os.path.getmtime(cfg) - 3600
        print('shaders: %s: %s%s' % (name, mm.group(0) if mm else 'no count line (the step failed?)',
                                     ' (STALE: older than this build\'s configure, so not from this build)' if stale else ''))

    end = [l for l in lines if l.startswith('[boot_test]')]
    print('end: %s' % (end[-1] if end else 'no [boot_test] line (the run was stopped by hand or the script died)'))

    crashes = [l for l in lines if re.search(r'FATAL|Unhandled guest access violation at host PC|\[bad call\]|'
                                             r'Call to invalid', l)]
    for l in crashes[:6]:
        print('crash: %s' % l[l.find(']', 30) + 1:].strip()[:220])
    hp = first(lines, r'at host PC (0x[0-9a-f]+) \(([^)]+)\+0x([0-9A-Fa-f]+)\)')
    if hp:
        print('crash host PC: %s+0x%s (symbolize: llvm-symbolizer --obj=<exe> 0x%X with the .pdb beside it)'
              % (os.path.basename(hp.group(2)), hp.group(3), 0x140000000 + int(hp.group(3), 16)))
    if not crashes:
        print('crash: none')

    perf = [re.search(r'\[renut\] ([\d.]+) s: (\d+) game frames \(([\d.]+) fps\).*?; (\d+) draws', l) for l in lines]
    perf = [p for p in perf if p]
    for p in perf[:12]:
        print('frames: %s s: %s frames (%s fps), %s draws' % p.groups())
    if not perf:
        print('frames: no [renut] 10 s lines (the run ended before 10 s, or Renut is not in this build)')

    missing = sorted(set(re.findall(r'Shader ([0-9A-F]{16}) is not in the shader cache', '\n'.join(lines))))
    print('shaders missing at run time: %d%s' % (len(missing), (' (' + ' '.join(missing[:8]) + ')') if missing else ''))
    dropped = [l for l in lines if 'Draw dropped' in l]
    print('draws dropped: %d%s' % (len(dropped), (': ' + dropped[0][dropped[0].find('Draw dropped'):][:120]) if dropped else ''))
    # Frame captures (TFU_PRESENT_DIAG): every one, since a single non-black capture matters (2026-11-19: Present 700
    # had 239,900 pixels and the report missed it).
    for l in lines:
        mm = re.search(r'\[tfu diag\] (Present \d+ back|Present \d+: scan-out).*?average RGB ([\d/]+), (\d+) of (\d+) pixels', l)
        if mm:
            print('capture: %s: average %s, %s of %s pixels not black' % mm.groups())
    for tag in ('[drawlog]', '[tfu diag]', '[s4 speed]', '[s4 rt]'):
        n = sum(1 for l in lines if tag in l)
        if n:
            print('diagnostic lines %s: %d' % (tag, n))
    warn = sorted(set(re.sub(r'^.*?\] \[(warning|error)\] \[[^]]+\] \[t\d+\] ', '', l)[:100] for l in lines
                      if re.search(r'\[(warning|error)\]', l) and 'not in the shader cache' not in l
                      and 'Draw dropped' not in l))
    for w in warn[:15]:
        print('warning/error: %s' % w)
    return 0


if __name__ == '__main__':
    sys.exit(main())
