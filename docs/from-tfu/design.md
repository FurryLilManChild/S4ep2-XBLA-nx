# TFU-nx design (draft, 2026-11-10, for review with Mark)

## Goal

Star Wars: The Force Unleashed (Xbox 360, the HD version with Havok, Euphoria and DMM) running natively on the Switch:
game functions as real Switch functions, game memory as plain Switch memory, the game's Direct3D calls drawn on deko3d.
PC build first, as the step on the way.

## What we know (docs/scan.md)

- XDK 2.0.6534, one disc, 6.79 GB, 13.5 MB of code (large), Havok 5.5.0 + Euphoria + DMM, two Lua versions, WMV
  cutscenes through XMedia.
- The original targets 30 fps on the 360. The CPU-heavy physics is the main risk; GPU is second.

## Base tree (decision for Mark)

| Option | For | Against |
|---|---|---|
| **rr6-nx 1.0.1** (ChanseyIsTheBest, GPL-3.0) | newest everything: the "ultimate" Renut (katamari-nx 1.0.1 plus fixes from many ports), the GPU and drawing-thread optimisations, compressed game files, LTO + PGO for the SDK too, built-in settings, web installer | XDK 2135 hooks (oldest of all); TFU-nx would be GPL-3.0 |
| LoS1-nx (ours) | our port menu, our stock-clock work, XDK 11626 map | older renderer and SDK, misses a month of others' fixes |

Recommended: rr6-nx's tree (SDK fork, app, Renut, tools), with **katamari-nx's XDK 5632 Direct3D map** (also GPL-3.0)
as the starting point for the 6534 mapping, plus our LoS1-nx port menu and stock-clock code moved over later.

## What rr6-nx 1.0.1 adds over what we used on LoS1-nx

1. **Ultimate renderer base**: plain texture fetches gated (saved 3.2 ms of GPU a frame), lazy depth decompress, many
   ports' pixel-identical GPU rounds, render thread about 15 us a draw.
2. **Drawing-thread CPU**: a lock-free record lookup table and a call-free guest clock read took RR6's racing from
   42-48 fps to 60 at stock clocks.
3. **Bulk machine copies of hot functions** (217 from one console profile) plus a memcpy native.
4. **LTO + PGO for the whole NRO including the SDK and FFmpeg**, profile collected in Ryujinx or on the console;
   NRO 68 MB to 62 MB. A CPU verify mode compares every native function with its recompiled original (39 million calls,
   0 differences).
5. **Compressed game files** (`.lz4b`, read transparently by the SDK's file system): LZ4 HC 9 in 256 KB blocks for the
   archives, ZXC 7 in 512 KB blocks for media; RR6 about 800 MB smaller. Load times unchanged (the SD card's 45-55 MB/s
   read is the limit, decoding is 112 ms of a 4.2 s boot). Packed in the browser by a web installer.
6. **Player build**: settings built into the NRO, a short `config.toml`, crash-only logs, Sphaira forwarder with a
   39-bit address space, dynamic resolution docked only.

For TFU: the level packs are the obvious compression target; the WMV files are already compressed and will shrink little.

## Pieces

| Piece | Plan | From |
|---|---|---|
| Function list | rexglue analysis, then pointer and gap sweeps to a fixpoint | rr6-nx tools |
| Code generator | the fork's rexglue with the full codegen speed set from day one | rr6-nx `sdk/` |
| setjmp / longjmp | find both Lua copies' pairs before the first boot | porting guide 3.2 |
| Threads, kernel, XAM | SDK as is | rr6-nx `sdk/` |
| Audio | SDK XAudio / XMA | rr6-nx `sdk/` |
| Cutscenes (WMV9 via XMedia) | first: let the game decode; if slow, a native FFmpeg `wmv3` path | new |
| Renderer | Renut on deko3d, D3D map for XDK 6534 | rr6-nx renderer, katamari-nx map |
| Shaders | find where TFU keeps compiled shaders, dump, translate, precompiled package | rr6-nx tools |
| Port menu | ours from LoS1-nx (presets, resolution, fps cap, clocks) | LoS1-nx |
| Game files | `.lz4b` packing, web installer later | rr6-nx |

## Phases

1. Codegen on the cloud machine; sweeps; first PC boot (Mark builds).
2. PC on the emulated GPU to menus and first level; then Renut on PC.
3. First rough Switch build: how fast is it really? This decides the project.
4. Speed work (the checklist), port menu, physics quality options if the engine has them.
5. Compression, installer, release.

## Open decisions for Mark

- Base tree (above) and GPL-3.0 for TFU-nx.
- OK to run ChanseyIsTheBest's scripts (rr6-nx, katamari-nx) in the cloud and on his PC.
- Frame rate target: 30 fps (the original's) is the realistic goal.
