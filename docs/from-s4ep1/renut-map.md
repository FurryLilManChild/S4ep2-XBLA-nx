# Renut at Sonic 4's addresses (XDK 2.0.11626, D3D9LTCG)

S4ep1-nx's Renut is TFU-nx's (katamari-nx's 321 hooks at TFU's addresses, TFU's device layout, XDK 2.0.6534). Moving it
to Sonic 4 takes two maps: TFU's hooked functions to Sonic 4's, and TFU's device offsets to Sonic 4's.

References (game data, used only here, never committed): TFU's default.xex (Mark's, SHA-256 `c28a5607...83f6316a`,
translated with TFU-nx's own manifest and config) and LoS1's (SHA-256 `0287bbd8...`, LoS1-nx's manifest and config).
LoS1 has Sonic 4's XDK build (11626) but plain D3D9; Sonic 4 links D3D9LTCG, so some small functions are reshaped or
inlined into the game's code.

## Functions (2026-11-14): 296 of TFU's 316 hooks moved, plus 3 TFU could not place

| How | Count | Evidence |
|---|---|---|
| State tables | 242 rows | Sonic 4's render table `0x824D6498` (101 records) and sampler table `0x824D6958` (20) pair row for row with TFU's `0x82F43938` / `0x82F43DF8` by their default column (`tools/renut_state_tables.py` with TFU's tables; `renut_map/state_tables.txt`) |
| Call-site propagation | the rest | `tools/s4_d3d_propagate.py` (d3d_propagate.py with Sonic 4's tables and range): from the tables, unique shapes and the seeds below, every aligned `bl` pairs its targets; 2058 pairs, 40 conflicts dropped (`renut_map/tfu_to_s4.json`) |
| Seeds | 45 | `renut_map/seeds.txt`: d3d_match >= 0.85 with a 0.1 lead (TFU or LoS1, `match_tfu.txt`, `match_los1.txt`), and by kernel imports: Swap `sub_821C0E58` (the only VdSwap caller), Present `sub_8249E2F8` (the same callees as TFU's Present; LTCG moved it out of the D3D block), Resolve `sub_821C1C80` and SetScissorRect `sub_821B5B28` (Present's callees), InitializePresentationParameters `sub_821CF318` (VdQueryVideoMode), SetRingBufferParameters `sub_821C6788` (VdInitializeRingBuffer), the gamma reader `sub_821CADB0`, VdRetrainEDRAM's caller `sub_821D33B8` |

Hooks TFU left unhooked that Sonic 4 can have (LoS1 matches at 1.00, same XDK): BeginVertices `sub_821BF968`,
DrawVerticesUP `sub_821BFE00`, XGRegisterPixelShader `sub_821B99F8`.

Still open (39, `renut_map/cov.txt`): InsertCallback, AddRef, Acquire/ReleaseThreadOwnership, Direct3D_CreateDevice
and XenonGpu_Initialize (TFU's device init `sub_822E1950` is inlined into Sonic 4's `sub_821CF800`, which calls
VdInitializeEngines directly), DrawIndexedTessellatedVertices, Clear, Set{Vertex,Pixel}Shader, the lock and unlock
family, surface-level getters, CreateIndexBuffer, texture size helpers, and the small setters LTCG likely inlined
(SetStreamSource, SetIndices, SetVertexDeclaration, SetViewport, the B constants, UnsetAll, ResetAllState).

## Device layout: Sonic 4 = LoS1 (confirmed)

`tools/renut_field_diff.py` over 446 paired Direct3D functions (TFU to Sonic 4, `renut_map/field_diff.txt`), checked
name for name against LoS1-nx's `d3d.h` constants (LoS1's layout, verified in that game): all 14 shared fields agree.

| TFU offsets | Sonic 4 | Fields (votes / LoS1 name check) |
|---|---|---|
| 10880 .. 10943 | the same | creation thread, fences, flags (equal in TFU's and LoS1's d3d.h) |
| 11820 | +172 (11992) | vertex declaration (LoS1) |
| 11848 .. 12752 | +176 | 30+ fields; index buffer, render targets, depth, streams, strides, textures, viewport, scissor, shaders, predication, snapshot size (LoS1) |
| 12992 .. 13260 | +176 to +180 | tiling block and KickOff (13232 -> 13408, LoS1); 12992 -> 13172 and 13256 -> 13436 at +180: to check field by field |
| 13376 .. 13444 | +204 / +208 | VerticesRingEnd 13444 -> 13652 (LoS1) |
| 13500 .. 15004 | +212 | command buffer, presentation interval, present fence tag, critical sections, gamma ramp (LoS1) |
| 16560 .. 16728 | +216 | swap count, side buffer (LoS1) |
| 21532 .. 21600 | +216 to +224 | to check field by field |
| 22300 | +1900 (24200) | behavior flags (LoS1) |
| 22400 (allocation) | 24320 (0x5F00) | LoS1 |

Open: 11824 and 11832 (between the +172 and +176 fields), the tiling block's +180 fields, 21532..21600, TFU's -1 words
at 21700 and gamma pair 22272/22276.

## Applied (2026-11-14, evening)

Hooks: `tools/s4_renut_retarget.py` (pairs plus its MANUAL and LATENT tables, each with its evidence; every changed line
carries `S4ep1-nx:`): 296 moved, 3 newly hooked (BeginVertices `sub_821BF968`, DrawVerticesUP `sub_821BFE00`,
XGRegisterPixelShader `sub_821B99F8`), 25 compiled unregistered, 35 left unregistered as in TFU.

The 20 hand pairs (MANUAL), by call pattern and fields: XenonGpu_Initialize `sub_821D5578` (its only caller is the
device init `sub_821CF800`, which is TFU's `sub_822E1950` with VdInitializeEngines inlined), Direct3D_CreateDevice
`sub_82494160`, D3DDevice_SetViewport `sub_821B35C8` (D3D_SetViewport inlined into it; BeginTiling and EndTiling call it as
often as TFU's do), ResetAllState `sub_821B30F8`, the surface locks, texture size helpers and surface-level getters (link
order and call neighbours), SetVertexShader `sub_821B9EC8` and SetPixelShader `sub_821B9B20` (they read and write the
device's +0x3248 / +0x3244), CleanupGprAllocationSetup `sub_821B8EB0`, UnsetAll `sub_821C4BD8`.

Unregistered (no standalone function found; mostly LTCG): InsertCallback (watch: if the game inserts GPU callbacks,
they are not seen), AddRef, Acquire/ReleaseThreadOwnership, DrawIndexedTessellatedVertices, Clear (inlined into the game's
scene begin `sub_8249CC00`; ClearF is hooked), the vertex, index and constant buffer locks and unlocks, LineTexture
LockRect, ArrayTexture UnlockRect (folded with Surface UnlockRect), CreateIndexBuffer, SetStreamSource, SetIndices,
SetVertexDeclaration, D3D_SetViewport, the B constant setters, Vertex/PixelShader GetFunction (not linked: no function of
their shape).

Device offsets: `tools/s4_device_offsets.py`, 151 lines (`renut_map/s4_offsets_report.txt`). Field checks on Sonic 4's
own code: ClearF, BeginTiling and EndTiling (the tiling block and the clear Z / stencil all +176, not +180),
XenonGpu_Initialize (TFU's `std 0x2E30` is `std 0x2EE0`: Sonic 4's 4 extra bytes sit right after the declaration),
the device init (-1 words 21776/21780, perf frequency 21804, the 41 -1 words `li 0x29` at +0x5C30 = 23600),
InitializePresentationParameters (display mode +0x5500..0x5508), SetPWLGamma and Swap (gamma pending 24172/24176, +0x541C
-> 21748, 21668 -> 23568). LoS1-nx's device.cpp values for 0x541C, 0x5D90/94 and 0x5DAC are N&B's numbers carried over,
not LoS1 measurements: not used. kStateTagChunks' second range widened to 0x2DFC + 0x1B8 (covers to 0x2FB3); the
replay's unread window kept at 10880..11776 (both on the safe side). queued.cpp's read-chunk count: 42 (TFU 39).

Guest data (by `ld`/`lwz` after `lis` at the same place in paired functions, then by content):

| Constant | TFU | Sonic 4 | Check |
|---|---|---|---|
| render / sampler state tables | 0x82F43938 / 0x82F43DF8 | 0x824D6498 / 0x824D6958 | defaults row for row |
| format table | 0x8202CBD8 | 0x8209BB70 | the same words |
| D3DCAPS9 | 0x8202D230 | 0x8209BD10 | the same words |
| XDK build qword | 0x82F43EE8 | 0x824E0A38 | CreateDevice `ld` then `std +0x2A80`; encodes 0x2D6A = 11626 |
| VdGlobalDevice import slot | 0x820008B0 | 0x820004FC | 2 paired references; a kernel variable address as TFU's |
| XPhysicalAlloc's no-large-pages flag | 0x82FBDB04 | 0x824E2994 | 1 paired reference |
| HiZ owner | 0x8303ED58 | 0x825836CC | 4 paired references |
| full viewport | 0x8202CAEC | none (inlined) | Renut passes TFU's values (0, 0, 65535, 65535, 0, 1) |
| GameXMemFree / GetCurrentThreadId | sub_821CAE78 / sub_822F7C38 | sub_82223908 / sub_82223E18 | LoS1 1.00; CreateDevice's call order |

Compile check (Linux, clang 20, Renut on, `-fsyntax-only` with the build's own commands): the five hook files,
d3d_native_state.cpp and backend/queued.cpp clean. render/draw.cpp needs the host shader headers (DXC, Windows): checked
in Mark's PC build.

Shaders: default.xex holds only 8 Xenos containers; the rest come from the AMB files or the run-time compiler (open).
Renut captures every shader its cache lacks at Create time, so the first PC run fills the dump either way.
