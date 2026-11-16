#!/usr/bin/env python3
"""S4ep1-nx (2026-11-14): moves Renut's Direct3D hooks (app/src/renut_engine/gpu/d3d/hooks/*.cpp) from TFU's addresses
to Sonic 4's (docs/renut-map.md). Each BANJO_RENUT_HOOK line's TFU address is looked up in the pairs
(docs/renut_map/tfu_to_s4.json, then MANUAL below); found: rewritten to Sonic 4's address with the evidence; not found:
compiled unregistered (BANJO_RENUT_UNHOOKED_HOOK). LATENT: hooks TFU could not place that Sonic 4 can (LoS1, same XDK).
Lines already carrying "S4ep1-nx:" are skipped, so the script can run again.

  python -I tools/s4_renut_retarget.py [--dry-run]
"""
import argparse
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HOOKS = os.path.join(ROOT, 'app', 'src', 'renut_engine', 'gpu', 'd3d', 'hooks')
PAIRS = os.path.join(ROOT, 'docs', 'renut_map', 'tfu_to_s4.json')

MANUAL = {  # TFU address: (Sonic 4 address, evidence)
    '822EDAB8': ('821D5578', 'XenonGpu_Initialize: its only caller is the device init sub_821CF800 (TFU: the device init); calls KickOff; TFU match top'),
    '822D8710': ('82494160', 'Direct3D_CreateDevice: calls the device init, Release, GameXMemFree and the physical allocator, as TFU\'s; LTCG moved it'),
    '822D7AC8': ('821B35C8', 'D3DDevice_SetViewport: BeginTiling and EndTiling call it as often as TFU\'s do; D3D_SetViewport inlined into it'),
    '822D7758': ('821B30F8', 'D3D_ResetAllState: called by BeginTiling, EndTiling and the device init as in TFU; TFU match top 0.50'),
    '822DB650': ('821B7DB8', 'D3D_LockSurface: link order before Lock2D/Lock3D; TFU match top'),
    '822DB718': ('821B7E78', 'D3D_Lock2DSurface: TFU match top 0.72, call neighbours'),
    '822DB7A8': ('821B7F08', 'D3D_Lock3DSurface: TFU match top 0.76, call neighbours'),
    '822DB190': ('821B7938', 'D3D_GetSubresourceLayout: call neighbours (4), TFU match top'),
    '822DA700': ('821B6D58', 'D3D_FindTextureSize: TFU match top 0.79, call neighbours (3)'),
    '822DA558': ('821B6BB0', 'D3D_GetMaxMipLevels: TFU match top, call neighbours'),
    '822DAA70': ('821B7210', 'D3D_GetMipTailLevelOffsetCoords: TFU match top, call neighbours (2)'),
    '822DCD78': ('821C3AA0', 'D3D_UnlockResource: TFU match top, its paired caller calls it'),
    '822DBDD0': ('821B8528', 'D3DTexture_GetSurfaceLevel: link order right after GetLevelCount sub_821B8518, size 0xD0 as TFU\'s'),
    '822DBEA0': ('821B85F8', 'D3DCubeTexture_GetCubeMapSurface: link order (second of the three)'),
    '822DBF60': ('821B86C0', 'D3DVolumeTexture_GetVolumeLevel: link order (third of the three)'),
    # 822DA420 D3DArrayTexture_UnlockRect: its best match sub_821B8B30 is D3DSurface_UnlockRect's pair (TFU 822DA438,
    # d3d_match 1.00); Sonic 4 has one function for the two (identical code folded), so it stays unhooked.
    '822E3E80': ('821B9EC8', 'D3DDevice_SetVertexShader: reads and writes the device\'s vertex shader +0x3248 (LoS1 12872); TFU match top'),
    '822E3B80': ('821B9B20', 'D3DDevice_SetPixelShader: reads and writes the device\'s pixel shader +0x3244 (LoS1 12868); TFU match top'),
    '822E3540': ('821B8EB0', 'D3D_CleanupGprAllocationSetup: TFU match 0.77 (next 0.13), call neighbours'),
    '822D85F8': ('821C4BD8', 'D3DDevice_UnsetAll: TFU match top 0.68, call neighbours (4)'),
}
LATENT = {  # TFU's unhooked line address: (Sonic 4 address, evidence)
    '824D92B8': ('821BF968', 'D3DDevice_BeginVertices: LoS1 d3d_match 1.00 (same XDK)'),
    '824D9758': ('821BFE00', 'D3DDevice_DrawVerticesUP: LoS1 d3d_match 1.00 (same XDK)'),
    '826375A0': ('821B99F8', 'XGRegisterPixelShader: LoS1 d3d_match 1.00 (same XDK)'),
}
LINE = re.compile(r'^(BANJO_RENUT_(?:UNHOOKED_)?HOOK(?:_SHARED|_RAW|_RAW_SHARED)?)\(sub_([0-9A-F]{8})(.*)$')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()
    pairs = {k: (v['nights'], v['how']) for k, v in json.load(open(PAIRS)).items()}
    for k, v in MANUAL.items():
        pairs.setdefault(k, v)
    used = {}
    counts = {'moved': 0, 'unhooked': 0, 'latent': 0, 'kept unhooked': 0}
    for f in sorted(os.listdir(HOOKS)):
        if not f.endswith('.cpp'):
            continue
        path = os.path.join(HOOKS, f)
        lines = open(path, encoding='utf-8', newline='').read().split('\n')
        for i, line in enumerate(lines):
            m = LINE.match(line)
            if not m or 'S4ep1-nx:' in line:
                continue
            macro, tfu, rest = m.groups()
            unhooked = 'UNHOOKED' in macro
            if unhooked:
                if tfu in LATENT:
                    s4, how = LATENT[tfu]
                    macro = macro.replace('UNHOOKED_', '')
                    lines[i] = f'{macro}(sub_{s4}{rest}  // S4ep1-nx: was unhooked in TFU (sub_{tfu}); {how}'
                    counts['latent'] += 1
                    used.setdefault(s4, []).append(f)
                else:
                    lines[i] = line + '  // S4ep1-nx: unhooked as in TFU'
                    counts['kept unhooked'] += 1
                continue
            if tfu in pairs:
                s4, how = pairs[tfu]
                lines[i] = f'{macro}(sub_{s4}{rest}  // S4ep1-nx: TFU sub_{tfu}, {how}'
                counts['moved'] += 1
                used.setdefault(s4, []).append(f)
            else:
                um = macro.replace('BANJO_RENUT_', 'BANJO_RENUT_UNHOOKED_')
                lines[i] = f'{um}(sub_{tfu}{rest}  // S4ep1-nx: no counterpart found in Sonic 4 (docs/renut-map.md, still open)'
                counts['unhooked'] += 1
        if not a.dry_run:
            open(path, 'w', encoding='utf-8', newline='').write('\n'.join(lines))
    dup = {k: v for k, v in used.items() if len(v) > 1}
    print(counts, 'duplicate targets:', dup)


if __name__ == '__main__':
    main()
