# Progress log

Newest first.

## 2026-11-22 (16:40): Mark's run of 1753fa18; independent review; 7e3 targets as the console stores them

- Mark: the special stage pause is fixed (the deko3d resolve shortcuts off). Still wrong: the title and Sylvania's orange
  cast, the waterfalls, Sky Fortress Act 3's flare through geometry. Mark asked for an independent review.
- Review (separate agent, code and logs only): banjoTrim (shader_common.h) applies a target's RB_COLOR_INFO exponent bias
  to alpha only, and nothing clamps a 2_10_10_10_FLOAT (7e3) target to 0..31.875 or drops negatives and NaN; resolves
  copy the host RGBA16F as it is. The console stores export x 2^bias in 7e3 and resolves that. The title's composite is
  a Reinhard tone map plus a god-ray layer from an 8_8_8_8 target: auto-exposure cancels a uniform scale of the scene
  but not its weight against the god rays. The waterfall samples a mid-frame resolve of the 3F scene (BBCB1130,
  ps 1C7E2600F4A518AF: tex * vertex colour), so negative or NaN texels there show as blotches. Flare: no occlusion
  queries in the game code (no EVENT_WRITE_ZPD), no CPU texture reads in the logs ("texture lock" never logged); cause
  not found.
- Change: a resolve of a 7e3 target (RB_COLOR_INFO format 3) is drawn as the console holds it: colour x 2^bias, NaN 0,
  0..31.875, alpha 0..1 (view_bits_ps mode 5, s4_fp10_clamp on). "[s4 views]" counts them with the biases seen.
  Not yet seen running. A per-write clamp (banjoTrim) would also cover blends; left for if this is not enough.

## 2026-11-22 (14:40): Mark's run of c8d3760a; the deko3d resolve shortcuts off

- Mark: Death Egg Mk. II Act 1 starts (the 0x822A8410 fix holds). Special stage start and results screens still
  wrong; the title and Sylvania still too orange; Sky Fortress Act 3's sun flare shows through level geometry.
- read_log of renut_008/009 (c8d3760a35, 13:29-14:29): "resolves from never-drawn surfaces" 0 every 10 s, so the
  never-drawn-surface lead was wrong (BBCD14F0 and the like share the drawn target).
- The traced pause frames (13:52-13:58, 25 draws, 7 resolves): every colour resolve of the pause's blur chain goes
  through the colour hand-over ("3005 resolves not drawn, 1803 drawn after all, 1202 images taken at a whole clear"
  per 10 s, 5 a frame). The traced title frame: the tone-mapping chain's luminance downsample (R32_FLOAT 64x64 targets,
  partial resolves 16x16, 4x4, 1x1) goes through the deko3d-only drawn resolves. Hand-overs, scan-out hand-over and
  drawn resolves exist only in the deko3d renderer; the PC path copies every resolve.
- Change (settings only, test, release and built-in): banjo_deko3d_color_handover, _scanout_handover, _depth_handover
  and _draw_resolves off, so the Switch resolves as the PC does. The depth hand-over is the sun flare's occlusion path
  too. Not yet seen running. If a screen is still wrong, those shortcuts were not the cause.

## 2026-11-22 (12:05): Mark's run of 0ed3e6e0; the pause's resolves read never-drawn surfaces

- read_log of renut_007.1/007 (build 0ed3e6e00a, 11:23-12:00): every view pair carried over, 24<-3F included (up to
  600 per 10 s), none dropped. Mark: special stage pause and results still wrong (paused 30 s+). So the view-pair lead
  is disproven for those screens.
- The traced pause frame (11:47:23): the blur is drawn into the 06 target BBCA02D0, then each resolve reads a different
  surface object, BBCD14F0 and BBCD1670, that no draw in the frame went to; their resolved textures feed the next blur
  pass and the background. Lead (not confirmed): those surfaces' views differ from BBCA02D0's in some key bit, so each
  got its own never-drawn target (uninitialised memory: the green blocks and noise).
- Fix with its own check: a colour resolve from a never-bound surface reads the target most recently drawn at the same
  EDRAM base, pitch and host format (s4_resolve_redirect, on); "[s4 views]" counts them and logs the first 6 with both
  keys. If it counts 0 at the pause, the lead is wrong and the keys show why.

## 2026-11-22 (10:35): Mark's Switch run of 4d08d3da; the fourth view pair; shaders merged

- read_log of renut_006.log (build 4d08d3daf2, 10:08-10:29): "[s4 views]" shows 06<-3F, 06<-24 and 3F<-06 carried over
  every time (up to 600 per 10 s), none dropped; Mark: the title tint, the waterfalls and special stage 5's pause and
  results screens are unchanged. So carrying those three pairs is not what those screens need.
- Not carried: 24<-3F (the 7e3 target read through a 32_FLOAT view), 70-601 per 10 s in bursts (10:16, 10:19, 10:24,
  10:28); it feeds the 06<-24 copies. view_bits_ps mode 4 now carries it (the packed 7e3 word as the float's bits).
  Test: special stage pause and results backgrounds.
- No crash from 4d08d3da (rex_crash.log is 8aff941b's); 38 shaders compiled on the console; one 10 s window at 1.3 fps
  (not checked). Death Egg Mk. II not reached yet.
- Shaders: Mark's SD deko3d_shaders.bin (12.0 MB) as the merge source, lists PC + both console pipeline lists: 2,524
  variants (347 compiled in the cloud, 0 failed), 12,894 KB; deko3d_variants.txt from it.
- s4_switch_test.toml: banjo_renut_diag_resolve_trace on (one frame every 30 s), for the title and waterfall passes.

## 2026-11-22: the cloud Switch build

- Mark: past 0.5.0, NROs come from the cloud. Wine 9 crashes at start on this cloud machine ("free(): invalid
  pointer", even `cmd /c echo`), so LoS1-nx's Wine route fails here. Mark approved the devkitPro Docker image
  (ghcr.io/danfromtico/switch-dev-legacy-vulkan, GCC 15.2) with his own libnx and portlibs/switch mounted over its
  copies (from C:\devkitPro, tar files in FOR DELETION). tools/linux/build_switch.sh: setup (Wine, kept for machines
  where it runs), inputs (his out\renut_switch and the host shaders), configure, build, package (s4ep2.nro).
- Configure matches his (NVK driver /lib/libvulkan.a, deko3d back end). 1078 of 1078 steps, 0 failed, about 50 min on 2
  cores. NRO stamped "S4ep2-XBLA-nx switch build 4d08d3daf2", sha256 b820ae3f...; ELF out/nx/elf/renut_b820ae3fac5e.elf
  (cloud). video.cpp compiled with the new shader's header: the preflight's app-sources failure was its own build
  folder lacking that generated header.

## 2026-11-21 (22:40): issue 1's visual bugs: EDRAM view pairs carried over; diagnostics

- Second tester's logs (8a8a68aa, 23 runs) and screenshots: special stage pause and results backgrounds are noise,
  solid green and black blocks; the waterfall shows the scene behind with purple and green blotches; the title has an
  orange cast against the 360's picture. All three read a copy of the screen. The logs' only lead: "Resolve through a
  view of format 06 of EDRAM last drawn as 3F/24" (and 3F after 06), every frame of the title (capped at 8 per run).
- Fix (the lead, not yet confirmed): SyncColorViewLocked carries these pairs over bit for bit as the console holds the
  pixel (new host shader view_bits_ps: 7e3 to 8_8_8_8, 8_8_8_8 to 7e3, 32_FLOAT to 8_8_8_8), setting s4_view_bits (on).
  Confirmed when the title matches the 360 shot and the pause, results and waterfall look right on the Switch.
- Diagnostics: "[s4 views] 10 s:" counts every such read by pair, carried over or not; plume logs every uam compile
  with its memory figures (the tester's one crash was inside malloc in a shader compile; heap 1.7-1.8 GB free at the
  logged compiles, so not plainly out of memory: cause unknown).
- The same logs: 52-222 shaders compiled on the console per long session (later zones). Merge needs an SD card's
  deko3d_shaders.bin and pipeline_list.bin.
- Checks: deko3d host shaders (28) built in the cloud, 0 failed; Switch syntax of the 4 changed sources, 0 fail;
  preflight --quick PASS.

## 2026-11-21: GitHub issue 1, the Death Egg Mk. II crash

- Reporter's logs (vladimirkasan2-cpu, build 8a8a68aa, six runs; copies in Mark's Issues\issue01): every run stops at
  "[FATAL] Call to invalid or unregistered function at guest address 0x822A8410" (ctr 822E95D8) entering Death Egg
  Mk. II; rex_crash.log's five abort() blocks are that stop.
- 0x822A8410: stored as data at 0x820C4C64 (a field of a 0x3C-byte descriptor in .rdata), no .pdata start, the
  instruction before it an unconditional branch. Declared in overrides.toml. Generated as a clean function: reads r3
  only, tail-calls sub_822E3EC0.
- Sweep: every .rdata/.data word that points into .text and is not registered (164): the rest are SEH scope labels
  (0x8210AExx-0x8210B2xx), values in .reloc and .embsec_, or mid-instruction values; no other descriptor field.
- preflight --codegen --quick: PASS, 92 functions all registered.
- Not fixed yet, need PC captures: Sylvania Castle and title yellow tint, waterfalls, special stage reflections and
  pause/results artifacts, Sky Fortress sun through geometry. The logs warn "Resolve through a view of format 06 of
  EDRAM last drawn as 3F: its pixels are not carried over" (a lead to test, not a diagnosis).

## 2026-11-19 (21:10): Switch test of 8a8a68aa; release 0.5.0

- read_log.py --expect 8a8a68aa: renut_003.log and renut_004.log match (built 21:26 UTC). Handheld at 1020/460.8/1331.2
  MHz; after boot every 10 s sample 56.6-60.0 fps (up to 308,400 draws in 10 s); 0 shaders missing, 0 draws dropped;
  "[s4 shaders] prepared ... 1639 variants of 572 shaders in 1.6 s; 0 compiled on the console"; "[s4 metal]" flag set.
  rex_crash.log is the earlier build's (8aff941b): no crash file from 8a8a68aa. Mark: "things are lookin good". Not in
  the log: which special stages were finished (no line marks a stage's end).
- One 21.3 s stretch at 13 fps in renut_003, between shader preparation and the license check (boot, before the
  menu); renut_004's boot has none. Not in play.
- The console's deko3d_shaders.bin (20:47) is older than the one shipped (20:51, 1,639 variants) and compiled nothing
  new: no merge needed.
- Gate 2 for working sessions (Mark): preflight `--quick` before commands, the full run in the background;
  tools/build_commands.py prints the commands. The cloud copy of app/generated lost renut_init.cpp when the cloud
  machine restarted; the overrides check now says so instead of failing on every entry.
- Release 0.5.0: app/s4_switch_release.toml (test settings with the profiler stacks, screenshots, GPU profile and
  main-thread wait log off), README, docs/release-notes-0.5.0.md. The NRO is the tested build 8a8a68aa, shipped as
  s4ep2.nro (its NACP still says 0.1.0, as S4ep1-nx 0.5.0's did).

## 2026-11-19 (15:50): Switch test of 8aff941b; the special-stage crash

- read_log.py: build 8aff941b matches. "[s4 shaders] prepared before the game: 1435 variants of 569 shaders in 1.6 s; 0
  compiled on the console", DKSH cache 1,555 variants at start. 8.5 min handheld at 1020/460.8/1331.2 MHz: 56-60 fps,
  up to 18,000 draws a second; 131 variants compiled in the background over the run, 0 draws waited for one (the
  first Switch run: 500 compiled, 120 waits, 37.5 s). "[s4 metal]" flag set.
- Crash after a special stage (Mark; logs/rex/rex_crash.log, build 8aff941b): brk #0x3e8 (__builtin_trap) at
  image+0x1002150 = sub_82399280, from sub_82872E68 (ELF renut_e53f08bb1bd9.elf). sub_82399280's bctr (0x823992A0) is
  a call through a table of 28 function pointers (0x820B6FE0, indexed by [r3+4]); the generator read it as a jump
  table switching on r10, which holds the table address: one case, every other index trapped. Fix: indirect_calls =
  [0x823992A0] in overrides.toml (katamari-nx's fix for the same misreading); regenerated: REX_CALL_INDIRECT_FUNC.
  All 28 targets are registered. Preflight: "generated switches" fails on any switch on a register set to a constant
  just before, or with one or two cases (a sweep of all generated code found only this one).
- read_log.py now reads logs/rex/rex_crash.log (it had reported "crash: none" for this run).
- Shaders merged again (this run's console cache and list): 1,804 variants, 0 failed, 10,662 KB; list 1,639 lines.

## 2026-11-19 (14:25): first Switch test (9ca60366)

- read_log.py (now reads Switch build stamps and the SDK profiler's clock lines): build 9ca60366 matches. 6.5 min
  handheld, no crash, no missing shaders. Clocks: CPU 1020 / GPU 460.8 / memory 1331.2 MHz (18 samples), plus 1785
  CPU / 76.8 GPU (4 samples: the boost mode while a draw waited on a compile). "[s4 metal]" flag set; whether METAL
  shows in the menu is Mark's to see. Captures: Sylvania Castle and the pause menu drawn right.
- The stutter: uam compiled 500 variants on the console (151 ms average); 120 draws waited, 37.5 s in all. Outside
  those, 60 fps with the render thread at 0.9 ms and the GPU at 2.6 ms a frame.
- The console's cache: 311 of its 500 variants were not in the PC-built cache (the PC runs' list covered 38%).
  Merged: PC list + console list (4,689 pipelines), the PC and console caches kept: 1,555 variants, 0 failed,
  9,866 KB (sha256 fe3d9caa8a4bb1d0), a superset of both; deko3d_variants.txt 1,435 lines. In out\renut_switch.

## 2026-11-19 (14:00): shaders compiled up front

- Mark's Switch test: in game; the shader-compile stutter much worse than Episode I's. Cause: Episode I's Switch build
  shipped deko3d_shaders.bin (variants compiled in the cloud from the PC's pipeline list); Episode II's first Switch
  build shipped none, so every variant compiled on the console when first drawn. Mark: compiling shaders up front is
  best practice (recomp-nx/fnxf_xiii compiles them at first launch, 30-40 min); both parts approved.
- Part 1: tools/deko3d_variant_cache.py --generic --list (Mark's out\pc\pipeline_list.bin: 2,148 pipelines) --glsl
  (the package's glsl_opt): 923 variants, 0 failed, 7,846 KB (sha256 db11b8fcd0d70c41). Put in out\renut_switch and
  the SD folder out\sd\switch\s4ep2-xbla-nx; deko3d_variants.txt (--export-variants, every key checked) in
  out\renut_switch.
- Part 2: banjo::renut_gpu::deko3d::PrepareListedVariants (s4_prepare_shaders, on), from S4_CreateDevice_hook before
  the game draws: every variant in deko3d_variants.txt through plume::Deko3DPrepareVariant (new: loadVariant now, from
  the cache or compiled), with a full-screen "Preparing shaders: N of M" SDK dialog (s4/s4_shader_prep.cpp) and an
  "[s4 shaders]" log line. build_switch.ps1 copies deko3d_shaders.bin and deko3d_variants.txt into the SD folder.
  Not compile-checked for the Switch in the cloud (no devkitPro here; access to C:\devkitPro was refused): the
  Switch-only code was read by hand.

## 2026-11-19 (afternoon): the Switch build and Episode METAL

- Mark: colours looked right on screen (so the swap is in the scan-out capture's readback, not the display; left).
  Next: the Switch build and the Episode METAL unlock.
- Switch shaders (cloud, the tools Mark OK'd for Episode I: XenosRecomp from the base tree, SPIRV-Cross, uam): Mark's
  renut_shader_dump (590) -> early-alpha pass, SPIR-V 590, uam 590 of 590 (vs 389, ps 201), every optimisation pass,
  package 1,320 KB (out/renut_switch/deko3d_glsl.bin, sha256 01c0b4d8747d1bd6). Renut's 27 host shaders for deko3d
  are the base tree's (shaders folder identical). Both put in Mark's out\renut_switch and out\renut_switch_host.
  NRO title "Sonic the Hedgehog 4: Episode II".
- Episode METAL: the "Check Ep1 License" task (string 0x821059B8, constructor sub_823777A8) runs sub_823779F8, which
  enumerates marketplace content (type 0x000D0000) for title 0x58410A07 (Episode I), mounts it as "epo" and, with
  the license bit, writes 1 to [0x82AABC38]. sub_82343CB8 starts the check only while that word is 0; menus read it.
  s4/s4_metal.cpp (s4_episode_metal, on) sets it before sub_82343CB8 and after the worker; "[s4 metal]" logs it.

## 2026-11-19 (12:40): in game

- Run of 11adf4fb (read_log.py: matches). Mark: into the first level and played. No crash; the window was closed
  ("Window closing, shutting down", exit 0; boot_test calls any early exit a crash). 56 to 60 fps, up to 15,000 draws
  a second in play, no missing shaders, no dropped draws.
- All 49 D3DXCreateTexture calls returned 0 with a texture (the caps fix); the port made none.
- Still open: the captures keep red and blue swapped between the back buffer (Present 1900 average 242/177/132) and
  the scan-out (129/176/241). One warning: a resolve through a format 06 view of EDRAM last drawn as 24.

## 2026-11-19 (midday): a picture, wrong; the caps address; the level-load crash

- Run of c3e99be0 (read_log.py: matches). Things on screen, not right (Mark's screenshot: desert ruins in bands, a
  smaller copy in a square, blue). Crash at a level load: "Call to invalid or unregistered function at guest address
  0x822AA168".
- [s4 rt]: every D3DXCreateTexture returned 8007000E (E_OUTOFMEMORY) with texture 0, and the port made each one
  ("made by the port"); "colour targets ... with no texture 0" afterwards; 6,600 of 7,800 ends resolved per 10 s.
- Cause of the E_OUTOFMEMORY: kDeviceCapsVa was 0x820F80E0 (game strings "Boss4_01"...), written from
  "lis 0x8211, addi -0x7F20", which is 0x821080E0 (the D3DCAPS block, checked in the image). D3DX's checks read the
  strings as caps. Fixed; tools/s4ep2_addr_refs.py (in preflight) recomputes tagged addresses against the functions
  they name and flags the old value.
- Captures: the back buffer is orange (average 139/89/70 at Present 1000) and the scan-out blue (67/86/138): red and
  blue swap between the back buffer and the screen. Not changed in this build (the caps fix changes the textures;
  one run first).
- Crash: tools/built_ptr_scan.py finds two built code addresses inside .pdata functions, 0x822AA168 (the crash) and
  0x82596B48, both callbacks: added to overrides.toml. First appended after the [[switch_tables]] block, where they
  became keys of that table and the generator ignored them; caught by checking renut_init.cpp, moved into
  [functions]; preflight now checks every [functions] entry is registered.

## 2026-11-19 (late morning, 11:05): build 5c61c45b failed to link

- pc_build.log: lld-link undefined symbol renut::gpu::(anonymous)::g_banjo_game_frames, from render/draw.cpp:7619
  and 7623: the four-phase test of bc904028 (dropped after gate 5, never built) declared the extern inside the
  anonymous namespace. Removed the test entirely (its cvar, the extern and the block). preflight.py now checks for
  externs inside anonymous namespaces (it reports the old line as a FAIL and the fixed tree as a PASS).

## 2026-11-19 (early afternoon): the resolve trace; every off-screen resolve has no texture

- No-rebuild run of 850eb991 with banjo_renut_diag_resolve_trace and banjo_renut_fast_draw off (read_log.py: matches;
  70 s, 141 "[trace]" lines, three traced frames). From 30 s on (about 2,100 draws a second) each traced frame: the
  scene (14 draws) into a 1280x720 target BBCA41B0 (guest format 3F, Renut host format 10, RGBA16 float), then a chain of targets 480x256, 512x256, 512x512,
  256x256, 64x64 down to 1x1, then the back buffer (one full-screen draw and 8 small quads). Every resolve of the
  scene and the chain has destination texture 00000000; only the Present's resolve of the back buffer (to BBCA0250)
  has one. The resolves happen (through the hook); the textures they name do not exist.
- The descriptors' textures come from D3DXCreateTexture sub_82469398 (four calls, all in the descriptor creator
  sub_828616E0), which calls Direct3D's CreateTexture sub_823F26E8 and stores the texture it returns.
- Next build: s4/s4_rt_diag.cpp reworked. The forced resolve of dd60adbf is gone (it could not fire: the game's own
  resolves happen). It logs each D3DXCreateTexture call (arguments, result, texture), and when one leaves its texture
  pointer 0, makes the texture with Direct3D's CreateTexture (s4_make_missing_textures, on). What confirms it: "made by
  the port" lines, "colour targets ... with no texture 0" in the 10 s line, and a picture. If the textures are made
  and it is still black, the next suspect is what the chain's passes do with them. The texture the screen samples
  (0x82996C4C) is a view of descriptor 0x82996BF0's depth texture (+0x38), also made by D3DXCreateTexture (calls at
  0x82861980 and 0x82861B30); the trace shows no depth resolve (flags 4) at all, consistent with that texture being 0.
- Gate 5 asked whether the new hook is reached: the generated tree calls __imp__sub_82469398 directly (direct_calls).
  build_pc.ps1 step 3d reruns post_codegen.py (direct_calls --redo) when the hooked set differs from
  out\post_codegen.hooks; Mark's copy lacks 82469398, so his build reruns it. Checked in the cloud: after
  post_codegen.py, 0 direct __imp__ calls to sub_82469398, sub_828620A8 or sub_828755A8 remain.

## 2026-11-19 (midday): the texture dump; the game's render-target descriptors; gate 5 stops dd60adbf

- Run of build 850eb991 with the texture dump (read_log.py: matches; no rebuild, scripts pulled). In windows 1 and 2
  (window 0 queued none) the only texture Renut knew of in the draws' pixel slots was 0x82996C4C, 1280x720, plume
  format 20; both dumps are solid black (every texel 000000FF).
- "0 resolves": the swap line that says so is printed for the first swap only (device.cpp, swap_ordinal 1), so it
  covers boot, not the run. No whole-run resolve count exists yet.
- Correction of the entry below: sub_828755A8 is not the frame function and [0x82AA3428] does not make it skip.
  sub_828755A8 ends one of the engine's render-target descriptors; [0x82AA3428] picks its tiled or untiled path (and
  is the tile list). The untiled path resolves depth (flag 0x10, texture +0x38, flags 4) and each colour target whose
  bit is in the flags (texture +0x28 + 4i), unless [0x82AABFCC] (set by the begin sub_828758F8) is 0 or flag 0x20000
  is set.
- 0x82996C4C is a header sub_828616E0 builds at descriptor 0x82996BF0 + 0x5C (format word 0x18280186) over the memory
  of the descriptor's DEPTH texture (+0x38), and sets flag 0x10: a colour view of the depth resolve's words. Found by
  gate 5 after I had called it the colour resolve target. Renut has a path for sampling a depth resolve's words
  (banjo_gpu_depth_words_view); the 850eb991 log counts 0 binds through it.
- Build dd60adbf (s4/s4_rt_diag.cpp: descriptor logging, and s4_force_descriptor_resolve forcing COLOUR resolves) was
  pushed but not given to Mark: gate 5 found its fix aimed at the wrong texture, its "from" addresses are not set by
  the generated code (ctx.lr), and it forces even when the game skipped on purpose. To revise before any build.
- Instead, a no-rebuild run with banjo_renut_diag_resolve_trace (one traced frame at start and every 30 s: each
  resolve with source and destination, target change with its draws, first sample of a resolved texture) and
  banjo_renut_fast_draw off (every draw listed). What it settles: whether depth resolves into 0x82996BF0's +0x38
  happen, and whether 0x82996C4C's sample finds them.

## 2026-11-19 (late morning): colour ruled out; four-phase state test

- Build 850eb991 (read_log.py: matches). Still black. No "[s4 inputs]" line: the draws' declarations all carry COLOR
  (BBCB3AF0: position, COLOR 0 and 1, texcoord; 28 of 38 logged draws), so the colour suspect is out. The stream dump
  did not run: `powershell -File` passed "-Extra a,b" as one string, so only the first flag reached the game
  (boot_test.ps1 now splits it and prints the game's arguments).
- The game's frame function sub_828755A8 skips its resolves when the own-flip flag [0x82AA3428] is 0 and draws into
  the auto back buffer, which Present resolves: 0 resolves a frame is expected on this path.
- Missed by my read, found by gate 5: the 850eb991 run's Present 700 capture is not black (239,900 pixels, average 13):
  a flat grey panel with a black rectangle cut out, a dialog's shape. Geometry lands on screen; no texture detail
  anywhere. Suspect (not proven): textures sample black. read_log.py now prints every capture. The phase test below
  was judged unsound by the same check (content changes over time, so a phase cannot be credited) and is not run.
- Planned before that: s4_diag_phases cycles four 300-frame phases (as set; depth off; plus blending off and all channels; plus
  culling off) with "[s4 phase]" lines, and TFU_PRESENT_DIAG captures land in different phases; the stream dump with
  the corrected flags.

## 2026-11-19 (later): the shader build, still black; a correction and the declaration question

- Build b4000586 (read_log.py: matches; 469 shaders from data.cpk, 590 in the dump, none missing at run time). Still
  black; render target 0 all black.
- Correction: "vfetch 0 bytes" does not mean no vertex data. XenosRecomp's vertexFetchMemoryMask (and the bytes) cover
  only fetches the input layout does not provide (shader_recompiler.cpp ~322); ordinary vertex elements come through
  the input layout. The new line shows the main shaders read position, COLOR (usage 10) and texcoord from fetch 95,
  fetch 95 holds a buffer, device stream 0 stride 7 dwords, no bind applied.
- Gate 5 (independent check) found: 6 of 38 logged draws have colour mask 0; the declarations logged so far have no
  COLOR element (only position and texcoords), while the shaders read COLOR. The draw log now prints the device's
  declaration at each draw with its elements, to settle whether the draws' declaration lacks it.
- Fix attempt in the same build (Mark: every build carries a fix for the leading suspect): s4_merge_shader_inputs (on)
  adds to the device's declaration, per vertex shader, an element for each input it lacks, from the shader's own
  vfetch (stream, offset, format); "[s4 inputs]" lines prove it in force and list what was added.

## 2026-11-19: black screen, draws with no vertex data

- Build f0548210 (before the shader commit: Mark's copy had not taken it). 21,000 draws in 10 s reach the GPU at 60
  fps, no shader missing. Black: with TFU_PRESENT_DIAG the auto back buffer (render target 0) is all black at Presents
  60 to 1300 and the scan-out matches it; no resolves in the captured frame (the game draws straight into it).
- Draw log (banjo_renut_diag_draw_log_s=8): targets 1280x720, viewport and scissor whole, colour mask F, normal blend,
  but "vfetch 0 bytes" on every draw: no vertex data fetched. Episode II binds its shaders at run time
  (D3DVertexShader_Bind), which Episode I never did. The draw log now adds the shader entry's fetch mask and inputs,
  the bind applied to the shader and the device's stream fetch constants, to show which part is empty.

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
