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
    # 2026-11-19: sources this port added (listed in app/CMakeLists.txt, absent from the base port's build) are not in
    # the base's compile commands; check them with an app source's command as the template (s4_speed_diag.cpp's).
    known = set(re.findall(r'%s/app/(src/[^ ]+?\.cpp)' % re.escape(BASE), ' '.join(lines)))
    listed = set(re.findall(r'^\s*(src/[^\s)]+\.cpp)', open(ROOT + '/app/CMakeLists.txt').read(), re.M))
    # Switch-only sources (app/CMakeLists.txt's Switch list: *_switch.cpp and the console PGO runtime) need libnx.
    extra = sorted(x for x in listed - known if os.path.exists(ROOT + '/app/' + x) and 'banjo_hot' not in x
                   and '_switch' not in x and not x.endswith('banjo_pgo.cpp'))
    tmpl = [l for l in lines if 's4/s4_speed_diag.cpp' in l]
    if extra:
        if not tmpl:
            return check('app sources compile', False, 'no template command for the %d added sources' % len(extra))
        bad2, n2 = syntax(None, tmpl[0], [ROOT + '/app/' + x for x in extra])
        bad += bad2
        n += n2
    return check('app sources compile', not bad, '%d of %d fail (%d added by this port: %s)%s'
                 % (len(bad), n, len(extra), ' '.join(extra) or 'none', ('; ' + '; '.join(bad[:3])) if bad else ''))


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


def anon_externs():
    # 2026-11-19: an extern declared inside an anonymous namespace names a variable only that file can define, so
    # one meant for another file's global compiles (clang -fsyntax-only passes) and fails at the link (Mark's build of
    # 5c61c45b: g_banjo_game_frames in render/draw.cpp). The syntax check does not link, so this looks for them.
    hits = []
    for f in glob.glob(ROOT + '/app/src/**/*.*', recursive=True):
        if not f.endswith(('.cpp', '.h')):
            continue
        raw = open(f, encoding='utf-8', errors='replace').read()
        code = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])\'',
                      lambda mm: re.sub(r'[^\n]', ' ', mm.group(0)), raw, flags=re.S)
        stack = []
        for ln, line in enumerate(code.split('\n'), 1):
            for m in re.finditer(r'namespace\s*\{|namespace\s+[\w:]+\s*\{|\{|\}|\bextern\s+(?!"C")[^;(){}]*\b(\w+)\s*;', line):
                t = m.group(0)
                if t.startswith('namespace'):
                    stack.append('anon' if re.match(r'namespace\s*\{', t) else 'ns')
                elif t == '{':
                    stack.append('b')
                elif t == '}':
                    if stack:
                        stack.pop()
                elif 'anon' in stack:
                    hits.append('%s:%d %s' % (os.path.relpath(f, ROOT), ln, m.group(1)))
    return check('externs outside anonymous namespaces', not hits,
                 '%d extern declarations inside an anonymous namespace%s' % (len(hits), (': ' + '; '.join(hits[:4])) if hits else ''))


def addr_refs():
    r = sh('python3 tools/s4ep2_addr_refs.py')
    last = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr[-200:]
    mism = [l for l in r.stdout.splitlines() if l.startswith('MISMATCH')]
    return check('addresses against their functions', r.returncode == 0, last + ('; ' + '; '.join(mism[:3]) if mism else ''))


def overrides_check():
    # 2026-11-19: two function entries appended after the file's [[switch_tables]] block became keys of that table;
    # the generator ignored them without a warning and the crash they were meant to fix stayed. Every [functions]
    # address must be registered by the generated code, and no table may carry keys the generator does not read.
    import tomllib
    t = tomllib.load(open(ROOT + '/app/overrides.toml', 'rb'))
    bad = []
    for st in t.get('switch_tables', []):
        extra = set(st) - {'address', 'register', 'labels'}
        if extra:
            bad.append('switch table %s has keys %s' % (hex(st.get('address', 0)), ' '.join(sorted(extra))))
    init = open(ROOT + '/app/generated/renut_init.cpp').read() if os.path.exists(ROOT + '/app/generated/renut_init.cpp') else ''
    missing = [k for k, v in t.get('functions', {}).items()
               if k.lower().startswith('0x') and not v.get('size') == 0 and ('{ 0x%08X,' % int(k, 16)) not in init]
    if missing:
        bad.append('%d [functions] entries not registered (renut_init.cpp): %s' % (len(missing), ' '.join(missing[:5])))
    return check('overrides in force', not bad, '; '.join(bad) if bad else '%d functions, %d switch tables, all registered'
                 % (len(t.get('functions', {})), len(t.get('switch_tables', []))))


def switch_only_calls():
    # 2026-11-19: device.cpp and s4_shader_prep.cpp called banjo_renut_deko3d.cpp's functions, which only the Switch
    # build compiles: the PC link failed (Mark's build of 93f76a21). Every use of a function declared in
    # banjo_renut_deko3d.h outside the Switch-only sources must sit inside an #if naming __SWITCH__, PLUME_DEKO3D or
    # REX_PLATFORM_SWITCH. (The syntax check does not link, so it cannot see this.)
    hdr = open(ROOT + '/app/src/renut_engine/gpu/banjo_renut_deko3d.h', encoding='utf-8', errors='replace').read()
    hdr = re.sub(r'//[^\n]*', '', hdr)
    names = set(re.findall(r'\b(\w+)\s*\([^;{]*\)\s*;', hdr)) - {'if', 'for', 'while', 'return'}
    switch_only = ('banjo_renut_deko3d.cpp',)
    bad = []
    for f in glob.glob(ROOT + '/app/src/**/*.cpp', recursive=True):
        if f.endswith(switch_only) or '_switch' in os.path.basename(f) or '/me/' in f:
            continue
        stack = []
        for ln, line in enumerate(open(f, encoding='utf-8', errors='replace'), 1):
            t = line.strip()
            if t.startswith('#if'):
                stack.append(bool(re.search(r'__SWITCH__|PLUME_DEKO3D|REX_PLATFORM_SWITCH', t)))
            elif t.startswith('#el'):
                if stack:
                    stack[-1] = False if not re.search(r'__SWITCH__|PLUME_DEKO3D|REX_PLATFORM_SWITCH', t) else stack[-1]
            elif t.startswith('#endif'):
                if stack:
                    stack.pop()
            elif not t.startswith('//'):
                code = re.sub(r'//.*', '', line)
                for m in re.finditer(r'deko3d::(\w+)\s*\(', code):
                    if m.group(1) in names and not any(stack):
                        bad.append('%s:%d %s' % (os.path.relpath(f, ROOT), ln, m.group(1)))
    return check('Switch-only functions guarded', not bad, '%d unguarded calls into banjo_renut_deko3d.cpp%s'
                 % (len(bad), (': ' + '; '.join(bad[:4])) if bad else ''))


def link_check():
    # 2026-11-19 (Mark: "enhance your pre-flight check"): the whole program built and linked for Linux in the cloud
    # (out/linux-link: the SDK, every generated file, every app source, clang 20 with the PC build's -march), so a
    # missing or duplicate symbol, or a call into code only another platform compiles, fails here and not on Mark's PC.
    # Incremental after the first run. Not covered: Windows-only sources and the Switch NRO's own link.
    build = ROOT + '/out/linux-link'
    if not os.path.exists(build + '/build.ninja'):
        return check('Linux link', False, 'no out/linux-link (configure it: see this function)')
    r = sh('ninja -j2 -k 0', cwd=build, timeout=14400)
    errs = [l for l in (r.stdout + r.stderr).splitlines() if 'error:' in l or 'undefined reference' in l or 'FAILED:' in l]
    return check('Linux link', r.returncode == 0, 'exit %d%s' % (r.returncode, ('; ' + ' | '.join(errs[:3])[:400]) if errs else ''))


def switch_compile():
    # 2026-11-19 (Mark: "enhance your pre-flight check"): every app and plume source the Switch build compiles, with
    # the Switch build's own defines, flags and include paths and devkitPro's headers (tools/switch_syntax_cloud.py), so
    # code only the Switch compiles (PLUME_DEKO3D, __SWITCH__) is checked before Mark builds. The game code itself is
    # the same text the PC compile checks.
    r = sh('python3 tools/switch_syntax_cloud.py', timeout=3600)
    last = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr[-300:]
    fails = [l for l in r.stdout.splitlines() if l.startswith('FAIL') or l.startswith('    ')]
    return check('Switch compile', r.returncode == 0, last + ('; ' + ' | '.join(fails[:4])[:400] if fails else ''))


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
    results += [app_sources(), anon_externs(), switch_only_calls(), overrides_check(), d3d_sweep(), offsets(), addr_refs(), shaders(), switch_compile(), link_check()]
    ok = all(results)
    say('RESULT: %s (%d of %d checks passed)' % ('PASS' if ok else 'FAIL', sum(results), len(results)))
    os.makedirs(ROOT + '/out', exist_ok=True)
    open(ROOT + '/out/preflight.txt', 'w').write('\n'.join(REPORT) + '\n')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
