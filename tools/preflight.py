#!/usr/bin/env python3
"""Gate 2 (S4ep2-XBLA-nx, 2026-11-19): every check that must pass before Mark gets build commands.

Runs in Claude's cloud workspace from the repo root. Each check prints PASS or FAIL with its evidence; the whole report
goes to out/preflight.txt, and Claude sends Mark build commands only with a passing report (and quotes its summary).
A failure is Claude's to fix before the handoff, never Mark's to find in a build.

  python3 tools/preflight.py [--codegen] [--full]

  always:    tree against the base port, every app source through clang -fsyntax-only, the Direct3D sweep against
             docs/d3d/unpaired-explained.txt, every device offset against Episode II's field votes, no untagged
             device-range or game-address constants, the shaders in default.xex
  --codegen: also run the code generator and fail on any warning
  --full:    also run post_codegen.py and clang -fsyntax-only over every generated file (about 15 minutes)

Cloud-only paths: the base port's checkout (/home/claude/s4ep1-nx), its Linux configure (out/linux-renut, for the
compile commands), the generator binary, out/default_image.bin (the generator's image dump, game data, not in git).
"""
import argparse
import glob
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = '/home/claude/s4ep1-nx'
BASE_BUILD = BASE + '/out/linux-renut'
GENERATOR = BASE + '/sdk/out/linux-amd64/Release/rexglue'
REPORT = []


def say(line):
    print(line, flush=True)
    REPORT.append(line)


def check(name, ok, evidence):
    say('%s  %s: %s' % ('PASS' if ok else 'FAIL', name, evidence))
    return ok


def sh(cmd, cwd=ROOT, timeout=7200):
    return subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True, timeout=timeout)


def tree():
    base = set(sh('git ls-files', cwd=BASE).stdout.split('\n'))
    ours = set(sh('git ls-files').stdout.split('\n'))
    missing = sorted(f for f in base - ours if f and not f.startswith('docs/'))
    dirty = sh('git status --porcelain --untracked-files=no').stdout.strip()
    return check('tree', not missing, '%d files the base port tracks are missing outside docs/%s%s'
                 % (len(missing), (': ' + ', '.join(missing[:5])) if missing else '',
                    '; uncommitted: ' + dirty.replace('\n', ' ') if dirty else '; nothing uncommitted'))


def compile_commands(pattern):
    out = sh('ninja -t commands', cwd=BASE_BUILD).stdout.splitlines()
    return sorted(set(l for l in out if re.search(pattern, l)))


def syntax(lines, template=None, sources=None):
    jobs = []
    if template is not None:
        jobs = [(template, s) for s in sources]
    else:
        jobs = [(l, None) for l in lines]

    def run(job):
        line, src = job
        line = line.replace(BASE + '/app', ROOT + '/app').replace(BASE + '/sdk', ROOT + '/sdk')
        a = shlex.split(line)
        out, skip = [], 0
        for i, x in enumerate(a):
            if skip:
                skip -= 1
                continue
            if x == '-Xclang' and i + 1 < len(a) and a[i + 1] in ('-include-pch', '-include'):
                skip = 3
                continue
            if x in ('-o', '-MF'):
                skip = 1
                continue
            if x in ('-MD', '-c'):
                continue
            if src and x.endswith('.cpp'):
                x = src
            out.append(x)
        name = src or [x for x in a if x.endswith('.cpp')][-1]
        r = subprocess.run(out + ['-fsyntax-only', '-w'], capture_output=True, text=True, cwd=BASE_BUILD)
        errs = [l for l in r.stderr.splitlines() if 'error' in l]
        return name, r.returncode, errs

    bad = []
    with ThreadPoolExecutor(2) as ex:
        for name, rc, errs in ex.map(run, jobs):
            if rc:
                bad.append('%s: %s' % (os.path.basename(name), errs[0] if errs else 'rc %d' % rc))
    return bad, len(jobs)


def app_sources():
    lines = [l for l in compile_commands(r' %s/app/src/[^ ]*\.cpp' % re.escape(BASE)) if 'banjo_hot' not in l]
    bad, n = syntax(lines)
    return check('app sources compile', not bad, '%d of %d fail%s' % (len(bad), n, ('; ' + '; '.join(bad[:3])) if bad else ''))


def generated():
    r = sh('python3 tools/post_codegen.py')
    if not check('post_codegen', r.returncode == 0, 'exit %d' % r.returncode):
        return False
    tmpl = compile_commands(r'generated/renut_recomp\.0\.cpp')
    hot = compile_commands(r'cpu/banjo_hot_guest\.cpp')
    if not tmpl or not hot:
        return check('generated compile', False, 'no compile command template in the base port\'s configure')
    gen = sorted(glob.glob(ROOT + '/app/generated/renut_recomp.*.cpp')) + [ROOT + '/app/generated/renut_init.cpp']
    hot_src = [ROOT + '/app/src/cpu/banjo_hot_guest.cpp'] + \
        [ROOT + '/app/src/cpu/banjo_hot_shard_%d.cpp' % i for i in range(12)]
    bad1, n1 = syntax(None, tmpl[0], gen)
    bad2, n2 = syntax(None, hot[0], hot_src)
    bad = bad1 + bad2
    return check('generated compile', not bad, '%d of %d fail%s' % (len(bad), n1 + n2, ('; ' + '; '.join(bad[:3])) if bad else ''))


def codegen():
    env = dict(os.environ, REXGLUE_DUMP_IMAGE='../out/default_image.bin')
    r = subprocess.run([GENERATOR, 'codegen', 'renut_manifest.toml'], cwd=ROOT + '/app', capture_output=True,
                       text=True, env=env, timeout=7200)
    warn = [l for l in (r.stdout + r.stderr).splitlines() if re.search(r'warn|unresolved|error', l, re.I)]
    empty = sh("grep -l 'DECLARE_REX_FUNC();' app/generated/*.h").stdout.split()
    return check('codegen', r.returncode == 0 and not warn and not empty,
                 'exit %d, %d warning lines%s, %d headers with an unnamed call'
                 % (r.returncode, len(warn), (': ' + warn[0][:120]) if warn else '', len(empty)))


def d3d_sweep():
    r = sh('python3 s4ep2-xbla-nx/tools/s4ep2_d3d_sweep.py', cwd=os.path.dirname(ROOT))
    found = re.findall(r'^sub_([0-9A-F]{8}) reached', r.stdout, re.M)
    known = set(l.split()[0] for l in open(ROOT + '/docs/d3d/unpaired-explained.txt') if l.strip() and not l.startswith('#'))
    new = [f for f in found if f not in known]
    head = r.stdout.splitlines()[0] if r.stdout else 'no output (%s)' % r.stderr[-200:]
    return check('Direct3D sweep', r.returncode == 0 and not new,
                 '%s; %d not explained%s' % (head, len(new), (': ' + ' '.join(new)) if new else ''))


def offsets():
    r = sh('python3 tools/s4ep2_offset_check.py')
    m = re.search(r'mismatches (\d+)', r.stdout)
    ok1 = check('device offsets', bool(m) and m.group(1) == '0', r.stdout.strip().splitlines()[-1] if r.stdout else r.stderr[-200:])
    untagged, addr = [], []
    allow = ('mnk_controls.cpp', 'photo_capture.cpp')
    for f in glob.glob(ROOT + '/app/src/**/*.*', recursive=True):
        if '/me/' in f or 'widescreen' in f or not f.endswith(('.cpp', '.h')):
            continue
        raw = open(f, encoding='utf-8', errors='replace').read()
        code = re.sub(r'//[^\n]*|/\*.*?\*/', lambda mm: re.sub(r'[^\n]', ' ', mm.group(0)), raw, flags=re.S).splitlines()
        rl = raw.splitlines()
        for k, c in enumerate(code):
            line = rl[k] if k < len(rl) else ''
            if 'S4ep2-XBLA-nx' in line:
                continue
            for mm in re.finditer(r'\b0x82[0-9A-Fa-f]{6}\b', c):
                if not f.endswith(allow):
                    addr.append('%s:%d %s' % (os.path.relpath(f, ROOT), k + 1, mm.group(0)))
    ok2 = check('game addresses', not addr, '%d untagged 0x82xxxxxx constants outside comments%s'
                % (len(addr), (': ' + '; '.join(addr[:4])) if addr else ''))
    return ok1 and ok2


def shaders():
    r = sh('python3 tools/renut_shader_dump.py --image out/default_image.bin --out /tmp/preflight_shaders')
    m = re.search(r'(\d+) vertex and (\d+) pixel shader containers in the image', r.stdout)
    n = int(m.group(1)) + int(m.group(2)) if m else 0
    return check('shaders in default.xex', n > 0, (m.group(0) if m else r.stderr[-200:]) +
                 '; data.cpk\'s are counted on Mark\'s PC by the build (cpk_shaders.log, read by tools/read_log.py)')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--codegen', action='store_true')
    ap.add_argument('--full', action='store_true')
    a = ap.parse_args()
    head = sh('git log --oneline -1').stdout.strip()
    say('preflight for %s' % head)
    results = [tree()]
    if a.codegen or a.full:
        results.append(codegen())
    if a.full:
        results.append(generated())
    results += [app_sources(), d3d_sweep(), offsets(), shaders()]
    ok = all(results)
    say('RESULT: %s (%d of %d checks passed)' % ('PASS' if ok else 'FAIL', sum(results), len(results)))
    os.makedirs(ROOT + '/out', exist_ok=True)
    open(ROOT + '/out/preflight.txt', 'w').write('\n'.join(REPORT) + '\n')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
