#!/usr/bin/env python3
"""TFU-nx (2026-11-12): every Renut host shader's deko3d header (gpu.cmake renut_host_shader), made with the Linux
DXC, SPIRV-Cross and uam (tools/deko3d_host_shader.py for each), into one folder the Switch build copies them from
(-DBANJO_DEKO3D_HOST_SHADERS_PREBUILT=<folder>). The Windows host then needs no SPIRV-Cross or uam of its own.

  python3 tools/deko3d_host_shaders_all.py --dxc <dxc> --spirv-cross <spirv-cross> --uam <uam> --out <folder>
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GPU = os.path.join(ROOT, 'app', 'src', 'renut_engine', 'gpu')


def main():
    ap = argparse.ArgumentParser()
    for k in ('dxc', 'spirv-cross', 'uam', 'out'):
        ap.add_argument('--' + k, required=True)
    a = ap.parse_args()
    calls = re.findall(r'^\s*renut_host_shader\(\$\{TARGET\}\s+(\w+)\s+(\w+)\)', open(os.path.join(GPU, 'gpu.cmake')).read(),
                       re.M)
    os.makedirs(a.out, exist_ok=True)
    failed = 0
    env = dict(os.environ)
    d = os.path.dirname(os.path.abspath(a.dxc))
    env['LD_LIBRARY_PATH'] = d + os.pathsep + os.path.join(d, '..', '..', 'lib', 'x64') + os.pathsep + env.get('LD_LIBRARY_PATH', '')
    for stem, profile in calls:
        r = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'deko3d_host_shader.py'), '--dxc', a.dxc,
                            '--spirv-cross', a.spirv_cross, '--uam', a.uam, '--profile', profile, '--stem', stem,
                            '--src', os.path.join(GPU, 'shaders', 'host', stem + '.hlsl'),
                            '--out', os.path.join(a.out, stem + '.deko3d.h')], capture_output=True, text=True, env=env)
        ok = r.returncode == 0
        failed += not ok
        print('%-28s %-7s %s' % (stem, profile, 'ok' if ok else 'FAILED: ' + (r.stderr or r.stdout).strip()[-400:]))
    print('%d host shaders, %d failed' % (len(calls), failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
