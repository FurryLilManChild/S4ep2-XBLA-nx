# Progress log

Newest first.

## 2026-11-18 (night, later): every shader from the game's files, no capture loop

- S4ep1-nx's fix for its GitHub issue 1 (tools/cpk_extract.py, tools/s4_cpk_shaders.py, and its build_pc.ps1 step)
  carried over, with tools/built_ptr_scan.py. Episode II's data.cpk has the same NNSTDSHADER/SHADER.AMB: 469
  containers (read on Mark's PC through the bridge). default.xex holds 121 (Episode I: 8).
- Check against the third boot's 79 captured shaders: 37 are in SHADER.AMB, the other 42 in default.xex; none from
  anywhere else. The first build's default.xex dump had run before the code image existed (the dump folder was made
  empty and never filled again); build_pc.ps1 now runs the default.xex dump whenever the image is there and the
  data.cpk dump (every file in it) on every build.

## 2026-11-18 (night): third boot runs (60 fps, light blue screen), device offsets the first pass missed

- No crash: 90 s at 60 fps, audio running, data.cpk read; about 37 draws a frame reach Renut and none are drawn. The
  log: "Draw dropped: vertex shader 00000000" and 71 shaders captured (not in the build's shader cache yet; the next
  build adds them from out\pc\shader_capture, Renut's normal first-run step).
- The vertex shader 0: d3d_native_state.cpp keeps its own copy of 27 device offsets (blend, depth, stencil, point,
  sampler mips, viewport, declaration, shaders, export, present interval, behaviour flags) that the band tool did not
  cover; SetVertexShader wrote Episode I's 0x3248, the draws read 0x3310. Also device.cpp's flag bytes, fence
  completion pointer, screen-extent word and ring-reset loop, and draw.h's census boundaries.
- `tools/s4ep2_offset_check.py`: maps each offset by its own field votes where it has them, else the bands, and checks
  every "S4ep2-XBLA-nx: offset" tag in app/src against that (0 mismatches). The votes show two fields the bands get
  wrong: max anisotropy (0x2F44) and the Z filter (0x2F92) are at 0x2A70 and 0x2A8A in Episode II. A scan for
  untagged constants in 0x2A80-0x6080 outside comments now finds only non-device numbers.

## 2026-11-18 (late): whole-library sweep instead of crash by crash

- `tools/s4ep2_d3d_sweep.py`: every Direct3D function (0x823EC100-0x82413800) the game can reach through unhooked
  functions (bl, b, pointers in code and data; .pdata excluded), checked against the Episode I pairs. 400 reachable,
  276 hooked. 20 reachable with no Episode I pair, each read: SetIndices sub_823EFB68 (writes ring packets: now
  hooked), Bind and SetVertexDeclaration (hooked earlier today); the rest call hooked functions (thunks to
  Get2DSurfaceDesc, Lock2DSurface, UnlockResource; an integer SetViewport), read device fields Renut keeps
  (GetStreamSource sub_823EFAE0, an is-resource-set test sub_823FEC30), the device AddRef, the kernel export lookup,
  or are the XGraphics CPU library (0x82413xxx).
- The same sweep run on Episode I: 11 paired functions that Episode II reaches and Episode I never did, all on the
  path of D3DDevice_Release sub_823FFC68 (the game's graphics shutdown sub_828631B0) and the gamma setters' helpers,
  which run only behind hooked functions or at exit.

## 2026-11-18 (later): second boot, D3DVertexShader_Bind

- The state-table fix held: the device's settings now apply, the render thread starts, 9 shaders are captured and
  data.cpk opens. Then a guest read at the stack's top (0x70140000), the stack overwritten with 0xAF.
- Found with llvm-symbolizer on Mark's renut.exe and renut.pdb (staged from out\pc): host PC renut.exe+0x983684 is
  sub_82403FB8, Episode II's D3DVertexShader_Bind (shader, declaration, strides; no Episode I pair, since Episode I
  never calls it). Its loop writes (stride >> 2) as bytes for [declaration + 0x1C] elements: a Renut declaration
  record there and strides from uninitialised stack (0xBEBEBEBE >> 2 = 0x2FAFAFAF). Hooked to Renut's Bind.
- Also hooked: sub_823F4050, D3DDevice_SetVertexDeclaration (a function in Episode II, inline in Episode I).
- Of 132 Direct3D functions the game calls from outside the library, 13 have no Episode I pair; besides the two above,
  sub_823EFAE0 and sub_823EFB68 read the device's own slots and write ring packets, and are not yet checked.

## 2026-11-18: first boot crash found (Episode I's state tables)

- Mark's boots stopped right after the device was created, on a call to 0x80C10054. The registers logged by the new
  bad-call line (r3 the device, r9 0x8294040C, r7 0x820B48DC, r8 0x82ABF0A0) place it in sub_823E45D8 (called by
  sub_823E4010 from the device setup sub_82862E40's caller): it calls each render state's setter through the
  device's own table, `lwz r11,0x40(device + offset)`. Renut fills that table from the game's {getter, setter,
  default} records, and d3d_native_state.cpp still named Episode I's tables (0x824D6498, 0x824D6958), which are game
  code in Episode II: 0x80C10054 is the instruction at 0x824D6530. Now Episode II's 0x8293D300 and 0x8293D7C0.
- Sweep of every 0x82xxxxxx constant outside comments in app/src: one more Episode I address, the second copy of
  the format table in d3d_native_state.cpp (0x8209BB70, now 0x82107E50 as in resource.cpp). The remaining two
  (mnk_controls, photo_capture) are other games' and were in Episode I's working build unchanged.

## 2026-11-16 (evening): five failed first PC builds, and the checks that should have come first

- Failures, in order: missing `app/generated/rexglue.cmake` and FFmpeg's three config headers (force-tracked in
  Episode I, dropped by the kickoff copy); fmt's README.md (one of 125 README/LICENSE files the copy dropped); the
  hot-copy sources compiled before the code generator on a fresh tree (now OBJECT_DEPENDS on its stamp); a call with
  no name in renut_recomp.31.cpp (the first codegen's open warning, "Unresolved b target 0x821F3A10": declared as a
  function); gotos to missing labels in sub_826D0340, found by the new check below (sub_826D01F0's byte jump table at
  0x82073710 has no bounds check and was read to 513 entries: a manual 16-entry [[switch_tables]] entry).
- Now `git ls-files` matches Episode I's outside docs/, codegen gives no warnings, and after post_codegen.py every
  generated file and hot-copy file (140) and every app source passes clang -fsyntax-only in the cloud.
- docs/pitfalls.md: the pre-build checklist, since the first two were a repeat of a 2026-11-14 pitfall.

## 2026-11-16 (afternoon): Episode I's renderer hooks moved to Episode II, first PC build ready

- Direct3D: `tools/d3d_align.py` (an order-kept alignment of the two Direct3D libraries, 527 pairs) and the state
  tables (render 0x8293D300, sampler 0x8293D7C0; 221 getters and setters, 0 disagreements). `tools/s4ep2_retarget.py`
  moved 298 hooks; SetSamplerState slot 7 (sub_823EF380) by hand from the sampler table.
- Device layout: `tools/s4ep2_field_votes.py` over the pairs gave ten offset bands (+0 below 0x2A80 up to +256);
  `tools/s4ep2_device_offsets.py` and `s4ep2_offset_literals.py` applied them (88 literals). Allocation 0x6080 (CreateDevice
  li r5,0x6080). The render thread's unread private range is now 0x2A80-0x2EC0 (queued.cpp; still 42 chunks compared).
- Data, each from the paired function: build qword 0x8295BA48 (CreateDevice ld), device holder 0x82AAC01C, present
  parameters 0x82AA32D8, caps table 0x820F80E0 (GetDeviceCaps), import slot 0x82000830 (D3DResource_Destroy),
  no-large-pages word 0x8295DA24 (XPhysicalAlloc sub_8245BE28, Episode I sub_822252B8 at 1.00), format table
  0x82107E50 (47 votes), HiZ owner 0x82AA70F4 (11 votes). GetCurrentThreadId 0x82459490, the game's XMemFree 0x8245A420.
- Argument lists that differ from Episode I (read from the generated code): DrawVertices and DrawIndexedVertices take
  Direct3D's full form (Episode I's S4 adapters dropped); BeginTiling takes flags r4, count r5, rects r6, colour r7,
  stencil r9.
- Present: the wrapper sub_828770F8 rewritten from Episode I's (all its globals mapped by hand, listed in draw.cpp),
  plus the call Episode I lacks, sub_82860008(0x82AABFA0, 1). Direct3D's Present sub_823FBF58 hooked too. The pacer
  step sub_828604B8 for the speed line.
- Paths: build stamp, Switch folder `/switch/s4ep2-xbla-nx/`, `C:\S4ep2-XBLA-nx`.
- Checked: every app source passes a clang syntax check against Episode I's Linux configure (two errors found and
  fixed: offset comments the band tool had left without `//`, and the delta chunk range). Not yet built or run.

## 2026-11-16 (morning): first codegen, sweeps, Direct3D match against Episode I

- Codegen pass 1: one UnresolvedCall, an 8-byte tail-call thunk at 0x8242D428 (declared). Pass 2: 23,629 functions,
  128 files, 131 MB (Episode I: 12,440). One warning left: "Unresolved b target 0x821F3A10 from 0x821F39BC".
- Pointer sweep (`tools/pointer_fixpoint.py`): pass 1 accepted 88 of 106 (14 no code, 2 fragments, 2 funclets), pass 2
  none. `override_labels_check.py --code 82130000:828DFD3C` flagged 1 of 89 (0x82466DCC, an SEH fall-through):
  removed. `switch_check.py`: truncated 0, wrong targets 0, missed 0; six odd shapes to watch at boot (0x822E6460,
  0x822E77B0, 0x823992A0, 0x823992C8, 0x8264F3B8, 0x826511D8).
- `d3d_match.py` with Episode I as the reference (its 314 hook addresses as names, `docs/d3d/`): of 302 that are
  function starts, 277 match at 0.97 to 1.00, 15 at 0.85 to 0.97, 10 below (Swap, Resolve, BeginTiling, Release,
  Resource_Destroy, UnsetAll and a few render-state accessors: to check by hand). Despite the newer XDK the Direct3D
  code is nearly the same; the device's field offsets still need checking. The frame pacer step (Episode I
  0x82488958) matches 0x828604B8 at 0.94; the Present wrapper (0x8249E2F8) only 0.63 (to find by hand).

## 2026-11-16: kickoff

- Repo made from S4ep1-nx 0.5.0's tree (commit 7fa43d1c, `docs/upstream.md`). Episode I's notes moved to
  `docs/from-s4ep1/`.
- Extracted Mark's package to `assets\extracted\` with `tools/stfs_extract.py` (run on his machine).
- Scanned default.xex (`docs/scan.md`): XDK 2.0.20871, not Episode I's 11626, so the Direct3D map has to be made
  again; same CRI and NN engine family; image twice Episode I's size.
- Episode METAL: the 360 checks the Episode I license ("Check Ep1 License", referenced at 0x823778A8).
