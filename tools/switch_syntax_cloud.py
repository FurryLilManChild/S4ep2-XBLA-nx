#!/usr/bin/env python3
"""S4ep2-XBLA-nx (2026-11-19, Mark: "enhance your pre-flight check"): the Switch build's own compile of chosen sources,
syntax only, in the cloud, with clang 20 targeting aarch64 and devkitPro's headers from Mark's C:\\devkitPro.

Inputs (game data none; refreshed when the Switch configuration changes):
  out/switch_ref/sw_cmds.txt   the Switch build.ninja's blocks (DEFINES, FLAGS, INCLUDES) for the reference objects
  out/switch_ref/...           the Switch build folder's generated headers (generated-shaders, renut_gpu/host_shaders,
                               rexglue-sdk/include, tfu_build_stamp)
  /home/claude/dkp             devkitA64's newlib and libstdc++ headers, libnx/include, portlibs/switch/include

  python3 tools/switch_syntax_cloud.py [file ...]   (default: every source that only the Switch build compiles, the
                                                     Switch-only blocks' files, and every source changed since the
                                                     last Switch build Mark ran)

A source is checked with the flags of the reference object of its target (plume, banjo_renut_gpu or renut). GCC-only
flags are dropped. Exit 1 on any error.
"""
import os
import re
import shlex
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.path.join(ROOT, 'out', 'switch_ref')
DKP = '/home/claude/dkp'
GCC = '16.1.0'
GCC_ONLY = ('-mearly-ra=none', '-mno-outline-atomics', '-mtp=soft', '-Wno-psabi')


def blocks():
    text = open(os.path.join(REF, 'sw_cmds.txt'), encoding='utf-8', errors='replace').read()
    out = {}
    for b in text.split('-----'):
        m = re.search(r'build (\S+\.obj): \S+ C\$:(\S+)', b)
        if not m:
            continue
        vars_ = dict(re.findall(r'\n  (DEFINES|FLAGS|INCLUDES) = ([^\n]*)', b))
        src = m.group(2).replace('/S4ep2-XBLA-nx/', '')
        target = 'plume' if m.group(1).startswith('plume/') else 'banjo_renut_gpu' if 'banjo_renut_gpu' in m.group(1) else 'renut'
        out[target] = vars_
    return out


def mapped(word):
    word = word.replace('C:/S4ep2-XBLA-nx/out/switch', REF).replace('C:/S4ep2-XBLA-nx', ROOT)
    return word.replace('C:/devkitPro', DKP)


def command(vars_, src):
    args = ['clang++-20', '--target=aarch64-none-elf', '-fsyntax-only', '-w', '-ferror-limit=20', '-nostdlibinc',
            '-nostdinc++']
    for part in ('DEFINES', 'FLAGS'):
        for w in shlex.split(vars_.get(part, '').replace('\\"', '"')):
            if w in GCC_ONLY or w.startswith('-mtune') or w.startswith('-march'):
                continue
            args.append(mapped(w))
    args += ['-march=armv8-a+crc+crypto']
    inc = shlex.split(vars_.get('INCLUDES', ''))
    i = 0
    while i < len(inc):
        if inc[i] == '-isystem':
            args += ['-isystem', mapped(inc[i + 1])]
            i += 2
        else:
            args.append(mapped(inc[i]))
            i += 1
    a64 = DKP + '/devkitA64'
    args += ['-isystem', f'{a64}/aarch64-none-elf/include/c++/{GCC}',
             '-isystem', f'{a64}/aarch64-none-elf/include/c++/{GCC}/aarch64-none-elf',
             '-isystem', f'{a64}/aarch64-none-elf/include/c++/{GCC}/backward',
             '-isystem', f'{a64}/aarch64-none-elf/include',
             '-D__SWITCH__', '-D__aarch64__', src]
    return args


def target_of(path):
    if '/thirdparty/plume/' in path:
        return 'plume'
    if '/src/renut_engine/' in path:
        return 'banjo_renut_gpu'
    return 'renut'


def main():
    b = blocks()
    files = [os.path.abspath(f) for f in sys.argv[1:] if f != '-v']
    if not files:
        # Every app and plume source the Switch build compiles (out/switch_ref/sw_sources.txt, from Mark's Switch
        # build.ninja), plus the app sources app/CMakeLists.txt lists that are newer than that list.
        listed = [l.strip() for l in open(os.path.join(REF, 'sw_sources.txt')) if l.strip()]
        listed = [l for l in listed if l.startswith('app/src/') or l.startswith('app/thirdparty/plume/')]
        cm = open(os.path.join(ROOT, 'app', 'CMakeLists.txt')).read()
        listed += ['app/' + x for x in re.findall(r'^\s*(src/s4/[^\s)]+\.cpp)', cm, re.M)]
        files = sorted(set(os.path.join(ROOT, f) for f in listed if os.path.exists(os.path.join(ROOT, f))))
    if not files:
        files = [os.path.join(ROOT, f) for f in (
            'app/thirdparty/plume/plume_deko3d.cpp',
            'app/src/renut_engine/gpu/banjo_renut_deko3d.cpp',
            'app/src/renut_engine/gpu/d3d/hooks/device.cpp',
            'app/src/s4/s4_shader_prep.cpp',
            'app/src/s4/s4_metal.cpp',
            'app/src/s4/s4_rt_diag.cpp',
            'app/src/s4/s4_menu.cpp',
        )]
    from concurrent.futures import ThreadPoolExecutor

    def one(f):
        t = target_of(f)
        if t not in b:
            return f, 1, ['no reference flags for target ' + t]
        r = subprocess.run(command(b[t], f), capture_output=True, text=True, cwd=ROOT)
        return f, r.returncode, [l for l in r.stderr.splitlines() if ' error: ' in l or 'fatal error' in l]

    bad = 0
    with ThreadPoolExecutor(2) as ex:
        for f, rc, errs in ex.map(one, files):
            if rc or '-v' in sys.argv:
                print('%s %s%s' % ('OK  ' if rc == 0 else 'FAIL', os.path.relpath(f, ROOT),
                                   ('\n    ' + '\n    '.join(errs[:6])) if errs else ''))
            bad += rc != 0
    print('switch syntax: %d of %d fail' % (bad, len(files)))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
