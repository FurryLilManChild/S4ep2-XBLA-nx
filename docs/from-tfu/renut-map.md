# Renut at TFU's addresses (XDK 2.0.6534)

Renut's hooks were written for N&B's title update 3 (XDK 2.0.7978), then moved to Beautiful Katamari (XDK 2.0.5632,
katamari-nx `tools/renut_hook_map_katamari.txt`), then to Ridge Racer 6 (XDK 2.0.2135, rr6-nx, with XDK 2135 adapters).
TFU's XDK sits between Katamari's and N&B's, so the Katamari-layout hooks (no RR6 adapters) are the base.

## Found without a reference game (2026-11-11)

| What | TFU | Evidence |
|---|---|---|
| Render state table | `0x82F43938`, 101 records {getter, setter, default} | all 101 defaults equal N&B's, in N&B's order (`tools/rr6_renut_port.py` `kRenderStateDefaults`) |
| Sampler state table | `0x82F43DF8`, 20 records | all 20 defaults equal N&B's |
| State functions | 224 distinct, `0x822D4E78..0x822D7AB8` plus two shared stubs (`0x822C0100`, `0x82BA88F0`, 10 uses each) | the tables |
| Direct3D library | about `0x822C0000..0x822F5000` | the state functions and the Vd* callers below |
| Swap | `sub_822E9340` | the only caller of VdSwap (also calls VdGetSystemCommandBuffer) |
| Ring buffer init | `sub_822D9F40` | the only caller of VdInitializeRingBuffer |
| Graphics interrupt callback setters | `sub_822E1C20`, `sub_822E1860` | the two callers of VdSetGraphicsInterruptCallback |
| VdRetrainEDRAM caller | `sub_822F3488` | |
| VdQueryVideoMode callers | `sub_822F07D0`, `sub_822E1480` | |

Same-order tables mean the state hooks pair by index with N&B's (as Katamari's 222 did).

## Mapped against Beautiful Katamari (2026-11-11)

Reference: Mark's Beautiful Katamari (USA) disc, default.xex SHA-256 `562e2517...916279f1`, translated with katamari-nx's
own manifest and config (`9ff5d7a`), used only here. Katamari's 321 Renut hooks (katamari-nx `hooks/*.cpp`):

- 222 state hooks paired by table index (Katamari's tables `0x82AD8E48` / `0x82AD9308` and TFU's: identical defaults).
- 74 by `tools/d3d_match.py` (score >= 0.85, 0.05 ahead of the next; most 1.00): DrawVertices `sub_8281E2B8`,
  DrawIndexedVertices `sub_8281E6A0`, Present `sub_822E98D8`, Swap `sub_822E9340`, Clear / ClearF, CreateDevice parts,
  shaders, resources, surfaces, the ring. (TFU's draw functions sit at `0x8281E...`, apart from the D3D block.)
- 20 by hand: twins ordered as in Katamari, and the XDK's link order (each in the slot between confident neighbours).
- 5 left unhooked: BeginVertices and DrawVerticesUP (no function of their shape in TFU: probably not linked; the draw
  census will tell), SetTexture_Inline (game-side), BeginRingManual (a 2-instruction thunk), CDevice_Uninitialize.
- Every address and its evidence: `docs/renut_map/kat_to_tfu.tsv`; raw matcher output `kat_match.txt`.

Mirror of Fate (XDK 21256) as a second reference agreed where it was confident (`docs/renut_map/mof_match.txt`).

### Device layout, Katamari to TFU (`tools/renut_field_diff.py` over the 316 pairs, `docs/renut_map/field_diff.txt`)

| Katamari offsets | TFU | Note |
|---|---|---|
| below ~10944 | the same | |
| ~11000 .. 13176 | +8 | (N&B - 4) |
| 13216 .. ~13470 | +16 | equals N&B |
| 13492 .. 16684, 21508 .. 21524 | +24 | equals N&B |
| 21592 .. 21640, 22244 .. 22248 | +28 | |
| 22260 .. 22264 | +36 | |
| device size 0x5700 | 0x5780 | TFU's CreateDevice `li r3,22400` |

Confirmed field by field (the tiling pair, the device init `824E18D0`/`822E1950`, Swap, the setters): +8 for
10944..13180, +16 for 13184..~13460, +24 for ~13466..21600 (TFU equals N&B here), +28 for 21601..22255 (the 41 -1 words
21672 -> 21700, gamma 22244/8 -> 22272/6), +36 for 22256..22271 (BehaviorFlags 22264 -> 22300), allocation 0x5780.
`tools/tfu_device_offsets.py` rewrote 153 lines (each marked `TFU-nx: offset`).

## The port (2026-11-11)

- Hook files: katamari-nx's (`9ff5d7a`), every hook moved to TFU's address (`TFU-nx: Katamari sub_..., evidence`),
  the 5 with no counterpart compiled unregistered. Then the ultimate base's generic changes from rr6-nx's copies,
  without its XDK 2135 object layer: rr6_records_lockfree (resource.cpp), the automatic front buffer (pacman-nx),
  the gamma form (halo3-nx), recorded vertex binds (halo3-nx/sasrt-nx, shader.cpp), container version 0x0E (kameo-nx).
- `d3d.h`, `d3d_native.h`, `d3d_native_state.cpp`: katamari-nx's (rr6-nx's differ only by XDK 2135), plus
  `CreateFrontBufferTexture`.
- Guest data: XDK build qword `0x82F43EE8`, VdGlobalDevice import `0x820008B0`, D3DCAPS9 `0x8202D230`, full viewport
  `0x8202CAEC`, XPhysicalAlloc's no-large-pages flag `0x82FBDB04` (XPhysicalAlloc `sub_821CB658`, the same
  instructions as Katamari's), format table `0x8202CBD8`, HiZ owner `0x8303ED58`, GetCurrentThreadId `sub_822F7C38`,
  GameXMemFree `sub_821CAE78`, BeginTiling `sub_822F36E0` (0.69, same argument registers), EndTiling `sub_822F3B78`.
- Compile check (Linux, clang 20, Renut on): the five hook files, d3d_native_state.cpp, backend and guest records
  without errors.

## (Reference) other candidates

The draw, Present, CreateDevice, shader, resource, surface, tiling and ring hooks (about 120), by call-site
propagation (`tools/d3d_propagate.py`) and signatures (`tools/renut_sigmatch.py`) against a game whose map is known:
Beautiful Katamari (5632, closest), LoS1 (11626, LoS1-nx), Mirror of Fate (21256, mof-nx), SA2 XBLA, RR6, Zuma,
Feeding Frenzy. Then the device layout zones (`tools/renut_field_diff.py`) from the paired functions' displacements.

## Shaders

- 68 Xenos shader containers (magic `0x102A11xx`) inside default.xex's image (from `0x821092A8`).
- The rest are in the level packs, which are XMemCompress (LZX) streams (magic `0x0FF512ED`, the SDK's "XCTD
  compression"): not readable raw. Renut captures every shader its cache lacks at Create time
  (`hooks/shader.cpp` `CaptureMissingShader`), so a PC play-through fills the dump once the hooks work.
  `tools/lzx_tool.c` (worldkeflings-nx) is there if an offline dump is wanted.
