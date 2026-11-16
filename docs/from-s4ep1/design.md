# Design: S4ep1-nx

Reviewed with Mark before build work. Updated whenever a decision changes.

## Goal

A native Switch port: the game's PowerPC code translated to C++ (ReXGlue) and compiled as Switch code, game memory as
plain Switch memory, the game's Direct3D calls drawn natively by Renut on deko3d (no Xenos GPU emulation, no EDRAM model),
sound through the game's own CRI and XAudio2 code on the SDK's native audio backend. The emulated Xenos GPU stays in the
PC build only as the reference picture.

## What is known

- XBLA, title 58410A07 v1, about 6 MB of code, one 195 MB `data.cpk` (`docs/scan.md`).
- XDK 2.0.11626 throughout, the same build as LoS1. LoS1-nx's Direct3D map should carry over with little change.
- Middleware: CRI File System and CRI Audio (game code, recompiled like the rest), Sega's NN library for models and
  shaders. No Lua seen (so no setjmp/longjmp work expected), no Havok or PhysX.
- Frame rate on the original console and CPU load: to measure in the first PC and Switch runs; no target is promised
  before then. A small 2.5D game should be far lighter than TFU or LoS1, but that is a guess until profiled.

## Pieces

| Piece | Plan | Borrowed from |
|---|---|---|
| Function list | ReXGlue analysis, then the pointer sweep (`pointer_fixpoint.py`, `find_gaps.py`) until a pass finds none | TFU-nx / sa2-nx tools |
| Code generator | the SDK fork's `rexgluerd`, with `codegen_flavor.toml` and the post-codegen steps on from the first build | TFU-nx (rr6-nx 1.0.1) |
| Threads, kernel, XAM | the SDK's implementations; CRI's threads pinned by name once we see them | SDK |
| File system | `d:/data.cpk` read through the SDK's VFS as one file (no repacking needed; under 4 GB) | SDK |
| Audio | CRI Audio runs as game code into XAudio2; the SDK's XAudio2 backend on SDL | SDK |
| Direct3D front end | Renut, with LoS1-nx's 11626 map, glue (`s4_renut_glue.cpp`) and stable resource copies as needed | LoS1-nx, TFU-nx |
| Shaders | if the game compiles HLSL at run time (`docs/scan.md`): dump the microcode it produces in a PC run, translate and precompile it into the NRO's deko3d package, and skip the run-time compile on the Switch by recognising each shader by its source hash. If it ships microcode: dump it from the AMB files as usual | Renut, XenosRecomp |
| Switch renderer | deko3d (Mark's preference), package built into the NRO, variant cache from a PC run | TFU-nx |
| Controls, port menu | Switch layout matching the 360 pad; port menu later (LoS1-nx's) with resolution and clock options; no stick dead zone by default (Sonic games play better at 0) | LoS1-nx |
| Saves | `savegame.txt`-style local save in the app folder; achievements and leaderboards logged locally or stubbed | SDK |

## Phases

1. Codegen and boot in the cloud (Linux build, Xvfb, emulated GPU): fix and rebuild without asking until the title
   screen, updates every 10 minutes.
2. PC build on Mark's PC (`tools/win/build_pc.ps1`): game on the emulated GPU, then Renut.
3. First Switch NRO (cloud build): renderer, sound, input all in; boot to title screen and play.
4. Speed (codegen options are already in from phase 1, then console profile, hot functions, PGO), then the port menu.
5. Coverage test from the manual's modes and options, then release (version per Mark; 0.5 = feature-ready, not fully
   played through).

## Decisions

- License: GPL-3.0 (base is TFU-nx, from rr6-nx). Mark agreed 2026-11-14.
- Renderer: deko3d via Renut.
- Open: frame rate target (after the first measurement); default clocks (after the first profile).
