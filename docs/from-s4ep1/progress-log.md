# Progress log

Newest first.

## 2026-11-14: kickoff

- Repo made from TFU-nx's tree (`docs/upstream.md`). GPL-3.0 (Mark's OK).
- Extracted Mark's XBLA package (version 1) to `assets\extracted\` on his PC; the "Addon" folder is avatar items, not
  a title update.
- Scanned default.xex (`docs/scan.md`): XDK 2.0.11626 (same as LoS1), CRI File System and Audio, Sega NN assets, and the
  HLSL compiler linked in (theory: run-time shader compiles, to confirm).
- Added `tools/xex_unpack.py` and `tools/xexunpack/` (LZX unpack with the SDK's libmspack).
- robzilla10001/SwitchPorts X360 list checked: no Sonic 4 port exists; ChanseyIsTheBest's XBLA ports and sa2-nx are the
  nearest references.

## 2026-11-14 (evening): first codegen, sweeps, Direct3D match

- Generator (`sdk/out/linux-amd64/Release/rexglue`) built with clang 20 (Ubuntu's clang-20 package) after the SDL3
  X11/XCB development packages were installed.
- Codegen options: the core speed set (x360 CPU step 1) on from the start; non_argument_as_local and its keep list
  (TFU addresses) and TFU's setjmp/longjmp removed (`app/renut_config.toml`). TFU's lists emptied: overrides,
  `config/renut_funcs.toml`, `src/cpu/hot/localized_bulk.txt` (copies in `docs/from-tfu/app-config/`).
- Codegen pass 1: two UnresolvedCalls, both leaves with no .pdata: 0x82186490 (size 0x170, a leaf whose two halves
  branch into each other) and 0x821F42D8 (an 8-byte thunk). Declared in `overrides.toml`. Pass 2: 12,440 functions,
  129 files, 64 MB, 8 s. Code range 0x820C0000-0x824A05DC.
- Pointer sweep: pass 1 accepted 69 of 74, pass 2 none (fixpoint). Gap sweep: 54 of 113, pass 2 none.
  `override_labels_check.py --code 820C0000:824A05DC` flagged 10 (1 SEH scope, 9 epilogue tails): removed.
- `switch_check.py`: 107 switches, 169 indirect; truncated 0, wrong targets 0, missed 0. Four tables with no bound in
  the code (34, 50, 17, 16 cases generated); five odd shapes generated as indirect jumps (0x821545A0, 0x8215B960,
  0x8215C6E0, 0x82417158, 0x82418EC0): watch for "invalid or unregistered function" at those addresses at boot.
- `d3d_match.py` against LoS1's default.xex (Mark's copy, SHA-256 0287bbd8..., the same XDK 11626 but plain D3D9; Sonic 4
  links D3D9LTCG): 22 of 41 named functions at 0.97 to 1.00 (draws, BeginVertices, shader and declaration creation,
  render targets, ring buffer, CBlocker::Check, SynchronizeToPresentationInterval); 19 to check by hand (0.26 to 0.94),
  as LTCG predicts. Full list: to be written into `docs/d3d-map.md` with the Renut work.
- PC build adapted: `tools/win/build_pc.ps1` and `boot_test.ps1` use `assets\extracted` (extracting the XBLA package
  with `tools/stfs_extract.py` if missing); TFU's `tfu_job_idle.cpp` and its `tfu_spin_idle.py` step left out.

## 2026-11-14 (night): Linux build, first cloud boot reaches the title screen

- Mark's first PC configure failed: `app/generated/rexglue.cmake` and three FFmpeg config headers were missing from the
  repo (the tree copy plus `git add -A` skipped files TFU force-tracks). Added (`docs/pitfalls.md`).
- `tools/post_codegen.py` still ran TFU's `tfu_spin_idle.py` (stops at "sub_826B1398 not found"): removed. Mark's PC
  build would have hit it too.
- Linux build (cloud boot tests; Ubuntu 24.04, clang 20, Xvfb, lavapipe): `-march=x86-64-v2` (SSSE3 in the SDK's
  memory.cpp), host glslangValidator (`-DBANJO_GLSLANG_VALIDATOR=/usr/bin/glslangValidator`), `REXGLUE_DUMP_IMAGE`
  set for post_codegen, and the Windows desktop UI (path wizard, keyboard and mouse, PC profiler) gated to `_WIN32` in
  `renut_app.h` (`RENUT_DESKTOP_UI`), with the Switch's no-op keyboard hooks on Linux (`app/CMakeLists.txt`).
- Boot 1 and 2 (emulated GPU, 130-150 s each, no crash): SEGA logo, Sonic Team logo, then the title screen with
  "Press the START button" by about 35 s; the attract loop pans the scene. Audio runs (CRI's render driver client
  registered, SDL frames with data). Screenshots: `out/boot/` (not in git).
- Checked: the inherited hand natives and localized copies (RR6/TFU's) show "ON" in the enhancements line but hook
  nothing: `hand.txt` and both localized lists are empty, `generated/hot` holds no functions.
- Leftover names in the log to rename later: "[tfu] TFU-nx pc build" (build stamp), "rr6_dlc_unlock_keys".

## 2026-11-14 (late): Renut moved to Sonic 4

- TFU's default.xex (Mark's, read-only folder access) translated as the reference; hooks and device layout mapped TFU to
  Sonic 4 (`docs/renut-map.md`): 299 hooks registered, 151 offset lines, 10 guest data constants, the full viewport
  passed by value, GetFunction imports dropped (not linked in Sonic 4), queued.cpp's chunk count 42.
- Tools: `tools/s4_d3d_propagate.py`, `tools/s4_renut_retarget.py`, `tools/s4_device_offsets.py`.
- Compile check with Renut on (Linux): the hook files, d3d_native_state.cpp and queued.cpp clean.
- Renut build check in the cloud (Mark's OK for DXC, 2026-11-14): with the pinned dxc-bin (`12395e36`), the shader dump
  from default.xex (8 containers), XenosRecomp and every generated header built (855 of 855); all 79 app and Renut
  sources pass `-fsyntax-only` with the build's own commands. Windows-only code (D3D12 backend, path wizard) is TFU's,
  unchanged.
- Sweep of other games' code still in the tree: N&B's keyboard/mouse pad mask, N&B's photo hook, banjo's native PM4
  renderer (masseffect_renderer_native) and RR6's DLC unlock are off by default or never registered; widescreen.cpp's
  N&B hooks are compiled out. Inert, to be removed in a cleanup.

## 2026-11-14 (late): first PC play, emulated GPU

- After the path wizard fix: title screen on Mark's PC. New Game crashed: "Call to invalid or unregistered function at
  0x82149FF8": the generator ran 0x82149E30 past its .pdata end and swallowed a function with no .pdata entry (its
  address built by lis/addi at 0x82149C18, 0x82149D30). A scan for every lis/addi address that lands on a .pdata end
  without being a function start found one more (0x8235E920); both declared in overrides.toml (0x82130004 is padding).
- The menu offered "Unlock Full Game": the SDK's license_mask defaulted to 0 (trial). Sonic 4 tests bit 0 of
  XamContentGetLicenseMask's mask (sub_82163E00) and clears its trial flag when set: license_mask now defaults to 1.
  Mark's "Addon" package is avatar items, not a title update; nothing else is needed for the full game.
- Renut run: "Direct3D_CreateDevice: InitializePresentationParameters failed"; 6 shaders captured. Next to look at.

## 2026-11-15: emulated GPU plays; Renut's hooks fitted to Sonic 4's LTCG signatures

- Mark's PC, emulated GPU: title screen, New Game, play. One note: a small save-file prompt at the title screen (an SDK
  dialog, not the 360's look); its scale is to fix later.
- Mark's PC, Renut: black screen. Log: "InitializePresentationParameters: 1920x1080 back 0x00000000 ... depth
  CreateSurface(... 0x00000780) failed": the hook read garbage arguments. Sonic 4's D3D is linked with LTCG, which
  drops arguments that never vary and reorders the rest, so TFU's signatures do not hold even where the code matched.
- Checked each paired hook's argument use in Sonic 4's generated code (which registers it reads, where each goes):
  - CreateDevice is inlined into the NN setup, sub_82494160 takes nothing (the parameters at 0x82505BC8, the device
    holder 0x82586168, behaviour flags 0x0C000001); InitializePresentationParameters (sub_821CF318) takes the device.
  - DrawVertices (device, primitive, count; start 0); DrawIndexedVertices (device, start, count; triangle strip, base 0).
  - Resolve (device, flags, rect, dest, point, colour r8, stencil r10, Z f1; level, slice, parameters 0).
  - SetViewport takes six floats (D3D_SetViewport's form), not a D3DVIEWPORT9 pointer.
  - Present takes nothing: the NN wrapper around the inlined D3D Present; written as a raw hook that keeps the game's
    own parts (a once-only init, its own front-buffer flip mode, the frame-time call after).
  - DrawVerticesUP, BeginVertices, UnlockResource, XGRegisterPixelShader: same as Renut's.
- Adapters in device.cpp, draw.cpp, state.cpp; Linux compile with Renut on: clean.

## 2026-11-15 (morning): Renut boots, runs at 60 fps, black picture

- Mark's PC, Renut, after the LTCG adapters: device created (1920x1080, back and depth surfaces made), title music,
  59.9 fps, no crash. Picture black.
- Log: "game gamma ramp applied ... [0]=0/0/0 [255]=0/0/0": an all-zero ramp, the scan-out multiplies every frame to
  black (TFU's 2026-11-11 symptom, opposite cause). Sonic 4's SetGammaRamp (sub_821B6978) and SetPWLGamma
  (sub_821B6A48) take two arguments, the ramp in r4 (sub_821CF318 calls them with r4 = its local ramp); TFU's
  three-argument hooks read r5. Registrations moved to the two-argument hooks.
- Log: "DRAWS NOT DRAWN: 6367 (no host shader 6367)" in 10 s: Sonic 4 makes its shaders at run time (51 captured to
  out\pc\shader_capture, 45 in this run), none in the build's cache. The next build collects them
  (collect_captured_shaders.cmake, the build folder is out\pc).

## 2026-11-15 (late morning): first picture; tiling hooked; slow motion on both renderers

- Mark's PC, Renut: picture and gameplay, the level loads. The bottom of the screen from row 544 down stays black
  (his screenshot: the HUD, the sky and clouds above, black below). Sonic 4 draws 1920x1080 with predicated tiling;
  its BeginTiling (sub_821C2D30) and EndTiling (sub_821C30D0) were never mapped, so Renut saw only the first tile.
  Found through TileCount (+0x327C). Hooked with their LTCG arguments; the first calls are logged ("[s4] BeginTiling",
  "[s4] EndTiling") so the rects confirm the split.
- The earlier "crash" at the level start was boot_test's 60 s limit ("Still running after 60 s: closed it").
- The game flips its own front buffers (0 D3D Presents between swaps in the diag lines), so TFU's present capture
  (TFU_PRESENT_DIAG) never fired: no pictures from that run.
- Slow motion (about half speed by eye) on the emulated GPU too, so not Renut's. 60 game frames a second, audio at
  full speed. The frame pacer (sub_82488738) sets a 16,667 us deadline from 1.0 * 16666.67 + 0.5 (constants read from
  the image), as on a 360. A log line every 600 Presents compares real time with the guest clock.

## 2026-11-15 (midday): slow motion is not CPU; counting the game's frame pieces on both renderers

- Mark's PC, Renut, 120 s with the PC profiler (banjo_pc_profile_s=30): every thread mostly idle. Main XThread busy
  22-26 % (88 % of that in NtWaitForSingleObject), the busiest game worker 11-14 %, Renut's render thread 8-11 %.
  The guest clock matched real time; 60 Presents a second. So the game waits most of each frame and still moves at
  about half speed: a pacing or timing difference, not throughput. The emulated GPU runs at full speed.
- The main loop (sub_821644F8) calls sub_821647A0 twice a pass, then the Present wrapper. The pacer step
  (sub_82488958) picks its wait from the display refresh (0x824F1390, set at run time) against 60 fps.
- src/s4/s4_speed_diag.cpp: every 10 s on either renderer, the counts of those pieces, D3D's GPU interrupt handler
  (sub_821C65E0) and swap-done callback (sub_821C0770), and the refresh and pacer periods. One run on each renderer
  shows where they differ.

## 2026-11-15 (afternoon): first Switch build prepared (TFU's route)

- Mark: Switch build next. TFU's route, tested on this tree: the NRO built on Mark's PC (tools/win/build_switch.ps1),
  the deko3d shaders made in the cloud.
- Cloud (Mark's OK for SPIRV-Cross and uam): SPIRV-Cross vulkan-sdk-1.4.304.0 and app/thirdparty/uam built with clang
  20 into out/deko3d_tools; zstd's CLI from XenosRecomp's copy (out/zstd_build). Shaders: Mark's renut_shader_dump (80)
  plus his shader_capture (82), 84 in all; early-alpha pass (little 1, unsupported 22), SPIR-V 84, GLSL 84 of 84,
  package out/renut_switch/deko3d_glsl.bin 107 KB (sha256 8f5bb467ea85f7b9). Renut's 27 host shaders for deko3d
  (dxc-linux), 0 failed. Both copied to Mark's out\renut_switch and out\renut_switch_host.
- build_switch.ps1: TFU's tfu_spin_idle step removed, the test folder out\sd\switch\s4ep1-nx, the settings
  app/s4_switch_test.toml (TFU's Switch settings without TFU's forwarder id). NRO title "Sonic the Hedgehog 4:
  Episode I"; the SDK's fallback path /switch/s4ep1-nx/renut.nro.

## 2026-11-15 (afternoon): first Switch NRO; the frame thread is the slow part on Renut

- run_all.cmd on Mark's PC (started by Claude through File Explorer while Mark was away): PC build, two 90 s boot
  tests, Switch build OK in 15 min (renut.nro 32 MB, sha256 91e72a403569b1d1, out\sd\switch\s4ep1-nx).
- Speed counters, 10 s windows at the title: the main loop's pieces 1,200 on both renderers. The frame thread's loop
  (sub_82488B10: pacer step sub_82488958, then sub_8249BAB0, sub_824891C0, sub_82219A00, sub_82496AF0, sub_824960B0,
  sub_8249ECA8) 867-889 steps on the emulated GPU, 152-284 on Renut. D3D's GPU interrupts 3,954-4,808 and swap-done
  callbacks 600 on the emulated GPU, 0 and 0 on Renut. Display refresh 60, pacer periods 833,350 ticks on both.
- Next: wall time per piece of that loop, both renderers.
- Wall time per piece (10 s windows): sub_824891C0 (the controller read, XamInputGetState through sub_82223928) took
  7 ms on the emulated GPU and 7,900-8,500 ms on Renut; every other piece the same on both. Renut's banjo_late_input
  delays the first controller read after each Swap (for games that read input and swap on one thread); Sonic 4 reads
  on its frame thread, so the delay held the game's simulation back.
- Test: Renut, --banjo_late_input=false, no rebuild: frame thread 845-866 steps in 10 s (emulated 875-889), the read
  8.4-8.6 ms. Default now off (banjo_renut_tuning.cpp, app/s4_switch_test.toml). The per-piece timers removed again;
  the per-10 s counts line stays.

## 2026-11-15 (evening): Switch test 1, and the first shipped variant cache

- Switch test 1 (NRO bea12b74f3acc3fe, late input off), handheld, about 2 minutes (Mark closed it): full speed. First
  60 s 87 game frames (boot compiles), then 40.6-59.3 fps, GPU 12.7-17.4 ms a frame in the level. uam compiled 173
  variants on the console (90 ms each); 43 times a draw waited for one, 7.7 s in all: Mark's stutter. The console's
  DKSH cache started empty.
- tools/deko3d_variant_cache.py --generic --list out/pc/pipeline_list.bin (the PC runs' 375 pipelines) --runtime-log
  renut_001.log (109 run-time variants, 52 new): 257 variants, 0 failed, 718 KB, sha256 60e1b7df79d70c88, in 3 s.
  Put in Mark's out\sd\switch\s4ep1-nx\deko3d_shaders.bin. The NRO unchanged.

## 2026-11-15 (evening): Switch test 2: speed-ups in the GPU dips; the pacer rule and the FPS counter

- Test 2 (variant cache 60e1b7df): much less stutter; the display showed 45.6-60.1 new images a second (GPU 11-21 ms a
  frame); Mark saw the game run up to about 1.5x. Frame-thread steps: about 600 in 10 s at 60 fps, 770-874 at 45-50 fps.
- The pacer (sub_82488958, object 0x824F0FA8): it polls the event, then spins until the event (set by the Present
  wrapper's tail, sub_82223E98) or its deadline (+24; the tail's sub_82488738 sets it to now + 16,667 us; the pacer
  moves it on by the period, +32, once passed). So a frame later than 16.7 ms gives a deadline step and then a Present
  step. On a 360 Presents land on refreshes (16.7 / 33.3 ms), which gives 60 steps a second; Renut's land anywhere
  (20 ms: two steps in 20 ms, 100 a second).
- s4_pacer_fix (on): the Present hook counts pacer entries since the last Present; if a deadline step ran since (more
  entries than the Present's own wake gives), it neither sets the event nor moves the deadline. The [s4 speed] line
  counts Presents that woke the frame thread and those held back.
- FPS counter: no game hook ticked a counter for Sonic 4 (TFU's and Katamari's were address hooks), so the overlay was
  empty. Now ticked by the display thread at each refresh with a new image ("Shown"); cpu/gpu lines only if set.
- The GPU dips: Renut's internal heights are a scale of height / 720 (render_scale.h, surfaces.cpp BuildTarget: host
  size = scale.Size(guest size)), made for 1280x720 games. Sonic 4's targets are 1920x1080, so TFU's handheld 540 drew
  them at 1440x810 and docked 720 at the full 1920x1080. s4_switch_test.toml: handheld 360 (960x540), docked 486
  (1296x729). The "Depth target" log line now gives the drawn size too, to confirm.

## 2026-11-15 (night): Switch test 3 read; shaders from the console for the next build

- Test 3 was build a950e242 (pacer fix, before the resolution change): frame-thread steps 600-602 in every 10 s window
  (the fix holds; 0-599 Presents held back a window). Display 48.9-59.8 new images a second. GPU profile in a heavy
  window, mostly at 460.8 MHz: 17.9 ms a frame = 9.8 draws (1920x1080 colour+depth) + 3.2 depth resolves + 2.5
  resolves + 1.6 present and overlays + 0.8 other. CPU total 34 % of 400.
- Special stage: 19,663-26,503 draws in 10 s not drawn, "no host shader": the console met 28 shaders the PC runs never
  captured (8 of them at the title screen). Mark copied them and the console's deko3d_shaders.bin to out\sd_back.
- Rebuilt in the cloud: 112 shaders (early alpha: little 1, unsupported 32), GLSL package 124 KB (772c70372b97181f);
  variant cache from the PC list, renut_001-004's run-time variants and the console's file merged: 367 variants,
  1,040 KB, 0 failed (81ca2d573e44597b). Mark's out\pc\shader_capture now holds the same 112 (two from the cloud dump
  added), so the PC build's shader table matches.

## 2026-11-15 (night): Switch test 4: the resolution change never took effect

- Test 4 (build 0f8625ae, 112 shaders, cache 367 variants): special stage drawn (0 shaders missing, 0 draws dropped);
  no draw waited for a shader compile (24 compiled in the background); frame-thread steps 600-601 in every window.
  Display 48.9-60.1 new images a second, GPU 4.7-20.1 ms a frame at 460.8 MHz, CPU 68-86 % of 400. Title sky still
  white (also on the PC, so not a Switch-only problem).
- The log: "Internal heights: handheld 1280x720" and "Depth target ... drawn at 1920x1080". Two causes: (1) the cvar's
  range is 480-1080, so 360 fell back to the default 720; (2) the internal resolution only applies to frames with the
  game's final composite (banjo_renut_smaa_after_ps, a TFU / N&B hash); Sonic 4 never has one, so every frame drew at
  the display scale, 1x of 1920x1080. TFU's 540 never applied to Sonic 4 either: every Switch test drew 1920x1080.
- Fix: range 360-1080 for both heights; s4_scale_without_composite (on): every frame at the internal resolution, the
  HUD with it. Variant cache merged with the console's file (391 variants, ce2ea77feb11e647).
- Captures (1920x1080 PNGs): a level, the loading screen, the special stage's pause, the zone title card, a black
  frame.

## 2026-11-15 (late night): 0.5.0 work: port menu, cleanup, the title's white sky found

- Switch test 5 (Mark): steady 60 at stock clocks, drawn at 960x540, 583-601 shown frames per 10 s. Plan for 0.5.0
  agreed: cleanup, port menu, the white sky, the save prompt's size, docked check, then release.
- Port menu (`app/src/s4/s4_menu.cpp`, from LoS1-nx): - and + together (F8 on the PC): FPS counter, handheld and docked
  resolution, handheld GPU profile, stick dead zone (`input_stick_deadzone`, new in the Switch input driver, 0 by
  default). Save and close rewrites only its keys in renut.toml. The SDK got LoS1's `AppOverlayOpen` and
  `config_file_path`. XAM message boxes (the save prompt) drawn at 2x.
- Cleanup: S4ep1-nx build stamp and log tags, TFU's job-idle code and tool removed, RR6's DLC hiding off, the speed
  diagnostics trimmed to one `[s4 speed]` line every 10 s.
- White sky, PC runs on Mark's machine (most of them unattended, at his request):
  - Draw log: fog constants sane (colour 0.7/0.95/1, end 500); not the cause.
  - Texture dump and screenshot: the reflection image is right; the main pass loses everything above the horizon.
  - Tile predication: no draw is cut to a tile (`s4_tile_predication_off` made no difference).
  - Frame capture before each resolve (`s4_diag_capture_present`) and resolve trace: the main pass's sky and island
    draws never reach the target. Four of Renut's skip shortcuts off: no change. No scissor: no change. Depth test off:
    the sky appears. So the main pass fails its depth test above the horizon.
  - Cause: between the passes the game draws the reflection's resolved depth back as colour (ps EFB11876797D1652,
    columns swapped by 40, the EDRAM depth layout) into EDRAM its depth target shares. sasrt-nx's EDRAM depth restore
    (`sasrt_renut_depth_restore`) then takes those words as the main pass's depth, and the sky fails against them.
    With it off the title matches Mark's photos of the 360 (`out\logs\round\norestore.png`). Default now off.
  - To check in the next Switch test: levels and special stages still look right with the restore off.

## 2026-11-16: Switch test 6, release 0.5.0

- Test 6 (build a9fd2f3b, Mark): menu opened with - and + (`menu shortcut: F8`), the
  `[menu] in force` line read the toml, resolution drawn at 960x540, `[s4 speed]` 600 pacer steps per 10 s throughout.
  Mark happy with it; release asked for.
- Release: `app/s4_switch_release.toml` (the test settings with the profiler stacks, screenshots, GPU profile and
  main-thread wait log off), the console's `deko3d_shaders.bin` from test 5 (434 variants, a superset of the 391
  shipped), README, `docs/release-notes-0.5.0.md`. GitHub release v0.5.0 with `S4ep1-nx-0.5.0-switch.zip`.
