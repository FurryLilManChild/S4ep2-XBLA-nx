# Progress log

Newest first.

## 2026-11-13 night (build 5 prepared: semaphore wake-one, main-thread wait diagnostics, test settings)

- `thread_semaphore_wake_one` (`sdk/src/core/threading_posix.cpp`): a release wakes one blocked thread per count
  released, and a thread that takes a count wakes the next only while counts are left (was notify_all on both).
  Test 8: 11 % of the main thread inside NtReleaseSemaphore (the host mutex and notify_all).
- `thread_main_wait_diag` (`xboxkrnl_threading.cpp`, `xevent.cpp`, `xsemaphore.cpp`): every 10 s, the objects the main
  thread waited on (type, handle, waits, time blocked) and the threads that signalled them, its sleeps by the time
  asked, its semaphore releases and the time inside them. Only `.cpp` files: the game code does not recompile.
- Test toml: `banjo_show_fps = true` (Mark's request for a live reading), `banjo_deko3d_background_compile_share =
  40` (was 10). All four SDK files compile with devkitA64 (a planted error is caught by the same check).
## 2026-11-13 evening (test 8: build 4 again, first level's areas from test 7, stock)

- 4.5 min at stock. Still 78 uam compiles (1.8 s average): the SD cache had grown to 13,708 (test 7's 183 saved),
  yet none of the 64 variants this log names is one of test 7's 64. The variant space is wider than one visit
  covers (the specialisation constants differ between visits). Mark: the big open areas still slow down.
- Windows with no compiles (21:59:46-22:00:36): 23.8 to 29.2 fps, GPU 19-21 ms, render thread 9-12 ms, CPU total
  270-290 %, core 3 18-33 % idle, main thread 64-70 % busy. Nothing saturated: theory, the frame is bound by the main
  thread waiting on other threads, not by any one core.
- Main thread in those windows, by its stack samples: 58 % running; 21 % waiting on an object (NtWaitForSingleObjectEx
  from sub_821CF0C8); 11 % inside NtReleaseSemaphore from sub_822F7A38 (the host semaphore's mutex and notify_all,
  SDK machinery); 9 % sleeping (KeDelayExecutionThread from sub_822F9D88, the game's SleepEx wrapper).

## 2026-11-13 (test 7, build 4: a new area again; its slow stretch lines up with shader compiles)

- Build 4 (`776a8f46`, stamp c337d116), 5.5 min, no crash. Copies: 686 of 805 reached, all direct; core 3 added to
  all 4 job workers (system core 3 idle 1.5 %, was about 50 %). Audio: 0 requests without data in every window but one
  (12). XctdCompression errors as before.
- Stock (1020 MHz, GPU 460.8): 30 fps 48:00-48:40; 18.6 to 24.5 fps 48:50-51:10 with render thread 11-17 ms and GPU
  23-26 ms a frame; overclocked from 51:10: 29 to 38 fps, GPU 14-15 ms. CPU total in the slow stretch 310-323 % with
  core 3 full, so cores 0-2 had spare time: not CPU-bound on cores 0-2 there (theory).
- 183 uam compiles (average 1.4 s), blocking ones ("prebuild waits", no draw waited) in every 10 s window of the slow
  stretch and none after it. The 52 shaders the log names are all new (none of test 6's 34): a new area, not a cache
  miss. While a variant compiles its draws use the generic variant (plume's asyncVariants), which costs GPU time.
  Theory: test 6's and test 7's slow stock stretches are first visits with compiles, not steady play. The stock
  slow stretch and the compiles overlap exactly, so this test cannot separate them from build 4's changes.

## 2026-11-13 (build 4 prepared: job workers on core 3, 805 localized copies, render-thread line tables)

- Test 6's stock windows (1020 MHz): the heavy part 19.6 to 25 fps with CPU 311 to 326 % of 400 (cores 0-2 full, core
  3 about half idle), the main thread only about 50 % busy while the job workers ran 45 to 55 % each and the render
  thread 30 to 50 %. Theory: bound by the total CPU on cores 0-2, not by one thread. Later part: 27 to 31.5 fps.
- Shader cache for test 7: the console's file (11,378 variants) merged with the Switch's pipeline list, the PC's list
  and `renut_009.log`'s 36 run-time variants: 13,525 variants (2,147 compiled, 0 failed), 165 MB, `739041471764647f`.
  In Mark's SD folder (parts joined, hash checked, parts deleted).
- `tfu_job_core3` (`app/src/tfu/tfu_job_idle.cpp`): each job worker adds core 3 to its own cores (libnx's call, past
  the SDK's mask wrap), checked again every 4,096 empty-queue looks; main and render stay on cores 0-2, the audio
  threads on core 3 keep their higher priority (0x2B against 0x3B). Logged in the "[tfu] job workers" line.
- `localized_bulk.txt`: 805 functions from test 6's profile (`tools/switch_hot_bulk.py`, at least 3 samples; 75 left out:
  atomics 40, time-base reads 20, hooks 4, tiny leaves 4, other forms 7). The whole post-codegen chain run in the cloud
  on build 3's generated code: 805 copies in 12 shards, none refused. PC check before the Switch build:
  `tools/win/verify_test.ps1` (`banjo_cpu_verify`, every call compared).
- `BANJO_SWITCH_RENDERER_LINES=ON` in the Switch configure: `FastDrawLocked` and the other draw functions had no line
  tables in build 3's ELF (`draw.cpp:0`), so the render-thread trim waits for test 7's profile to say where the time is.

## 2026-11-13 (Switch test 6: build 3, the register diet, and the shipped variant cache)

- NRO `2a68d6919d15ee33` (stamp `b68e2c98f7`), 155 MB (162 before the diet). PC boot test first: 30 fps, no crash.
  On the console: no crash in a long session (74 windows of 10 s), 4 cores, the 146 MB cache read (11,132 variants).
- Overclocked (1785 / 768 / 1600): 30 fps held (62 windows, average 30.8; test 4: 23 to 30). Stock: gameplay windows
  17 to 26 fps, then 29 to 30 (test 4: 19 to 22); main thread 53 %, two workers 46 to 54 %, the render thread 36 %,
  XMA 27 % on core 3 (21 % idle).
- 242 variants still compiled on the console (about 1.8 s each, in the background: 7,045 stages drew with the generic
  variant meanwhile), 322 read from its own additions.

## 2026-11-12 (a shipped deko3d variant cache)

- `tools/deko3d_variant_cache.py --generic --list out/pc/pipeline_list.bin --merge <console's deko3d_shaders.bin>`
  (Linux uam, 2 jobs, 33 min): every game shader's generic variant, the 11,601 pipelines of the PC runs' list, and the
  console's 647 entries: 11,132 variants, 0 failed, 146 MB (`2a3ba41b6d5dab7e`). The console's own run-time variants
  (its logs' "run-time variant" lines, 384) were all in its file already. 437 of the console's 647 have keys the PC
  list does not give (kept by the merge). Copied to Mark's SD folder (nine 18 MB parts, joined, hash checked).

## 2026-11-12 (Switch test 4: four cores through Mark's forwarder)

- `banjo_forwarder_program_id = "05A251C833574000"` (Mark's Sphaira forwarder, its id from test 3's log): the first
  launch wrote the NPDM, the next ones ran with "process cores 0xF, system core 3" (game threads on cores 0-2;
  Audio Worker, the mixer and uam on core 3).
- Stock (1020 / 460.8), shader cache warm (4 to 13 variants compiled per run): 19 to 22 fps (test 3: 13 to 16; one
  window at 14), GPU 24 to 28 ms a frame, CPU 285 % of 400. Overclocked: 23 to 30 fps.
- At stock the main thread 48 %, two game workers 44 to 46 %, the render thread 22 % (core 1), the XMA decoder 21 %
  on cores 0-2 (banjo_system_core_xma = false, rr6's choice); core 3 idle 57 %.

## 2026-11-12 (Switch test 3: build 2, 540p handheld, the job workers' idle wait)

- NRO `f8e2e268df5f7519` (stamp `2a3dc06f9a`), `renut.toml` with 540p handheld (the composite 9050898F454CF13D):
  "Internal heights: handheld 960x540", the HUD drawn at 1280x720 over the scaled image (capture checked).
- GPU at stock 38-45 ms -> 25-29 ms a frame. Job workers: 4,000-12,000 empty-queue looks per 10 s (most only
  yielded); `sub_826B1398` gone from the top of the profile.
- Overclocked: 21-30 fps (test 2: 18-26). Stock: 13-16 fps (test 2: 14-16), CPU 305 % of 300: 142 deko3d variants
  compiled on the console during the run (about 850 ms each; the 540p image needs new variants), uam's compiler at the
  top of the render-side thread's profile (nv50_ir). The stock figures are not a clean measurement; the variants are in
  the console's `deko3d_shaders.bin` now.

## 2026-11-12 (build 2 code; build 3 audits started)

- Build 2 (Mark building on the PC): `tools/tfu_spin_idle.py` + `app/src/tfu/tfu_job_idle.cpp` (the job workers'
  idle wait, no change to the hooked set, so one generated file recompiles), the draw trace with shaders
  (`tools/win/trace_test.ps1`), the running program id in the forwarder line. The XCTD compression query stays as it
  is (the game copes with the answer).
- Build 3 (Mark approved: the register diet, its own build). Cloud codegen with `non_volatile_as_local`,
  `elide_lr_stores`, `non_argument_as_local` into `out/build3/app` (227 s). `read_before_write.py`: 49 functions;
  39 are normal functions with their own frames (infeasible paths, setjmp's register saves, `xor r30,r30,r30`): left
  alone; 10 vector functions read v64-v92 at entry (a codec's routines passing vectors in v64+): share_registers.
  `non_argument_audit.py` on the code without the option: 453 ENTRY r12 (MSVC EH unwind funclets, `addi r31,r12,-N`:
  share_registers), 16 ENTRY v32 and 12 RET v32 (vectors passed in v32-v63), 19 RET r11, 3 RET r12, 1 r0, 1 r11:
  sorted: v32-v63 come from the vector save / restore pair sub_82EE0FB8 / sub_82EE1300 and their callers;
  every ENTRY and RET function (513 with the RET lines' callees; `diet_keep_list.py` now takes every callee a line
  names) goes into `non_argument_as_local_keep`, v64-v127 into `non_volatile_as_local_keep` (rr6-nx's choice). Codegen
  again with them (229 s): `read_before_write.py` 28 normal functions (infeasible paths) and the setjmp env copies;
  `non_argument_audit.py --with-option`: 65,103 of 68,528 functions localize, HANDOVER 1 (a setjmp env copy of r11);
  `--volatile` 0; three generated files compile (clang syntax check); `tfu_spin_idle.py` still finds its branch.
  In `app/renut_config.toml`. Also: the screenshots folder is `captures` now.

## 2026-11-12 (Switch test 2: the settings load, first CPU profile)

- Same NRO, the fixed `renut.toml`: loaded (0 settings left to defaults), stack sampler active. Captures show the first
  level drawn correctly.
- Stock (1020 / 460.8): 14 to 16 fps in game (test 1: 8 to 15), GPU 38 to 45 ms a frame, render thread 15 to 18 ms,
  CPU 290 % of 3 cores. Overclocked (1785 / 768 / 1600): 17 to 25 fps, GPU about 30 ms.
- CPU profile at stock (`tools/rex_profile_names.py` with llvm-nm, 11 windows): `sub_826B1398` 16.7 % of all samples
  (25 % of the unnamed game worker threads). It is a job-queue worker loop: atomic reads of a queue's start and end,
  a compare-and-swap to take an item, `sub_826B0D40` for the item, and when the queue is empty it reads the counters
  again without waiting while a state word stays below 1024. The time is in the function itself, not in the item's
  work. Theory: idle workers spin and take CPU from the main and render threads on 3 cores (the 360 had 6 hardware
  threads). Next: Renut render thread (FastDrawLocked, FillTextures, PushFloatConstants), XMA decoding about 4 %.
- 47 deko3d variants compiled on the console this run (78 read from the cache the first run saved); the console's
  `deko3d_shaders.bin` is 1.8 MB.
- 101 x `NtQueryInformationFile(XFileXctdCompressionInformation) unimplemented`.

## 2026-11-12 (Switch test 1: first boot)

- NRO 162 MB, sha256 `1fce18cc6240d227`, build stamp `8ad7f52a2f`. Mark: menus and sound snappy, FMV played, got in
  game; the level load took a while; playable when overclocked with sys-clk; no audio crackle when the picture slowed.
- The settings file did not load (a duplicate key, pitfall 15): the run used every default. Renderer: Renut on
  deko3d anyway (the SDK falls back to it), 3 cores, no CPU sampler.
- Numbers (10 s windows, `rex_profile.log` and `renut_001.log`): menus 30 fps. In game at stock (CPU 1020, GPU 460.8):
  8 to 15 fps, CPU 240 to 290 % of the 3 cores (many game threads at 20 to 40 % each, main thread 30 to 40 %), GPU
  about 40 ms a frame, render thread 20 to 100 ms a frame. Overclocked (1785 / 768 / 1600): 17 to 27 fps, GPU about
  30 ms. So CPU-bound across all three cores and GPU-bound near 25 fps at stock, both.
- 375 deko3d variants compiled on the console (about 300 ms each, 110 s in all): the long load and stutters. No
  variant cache shipped yet (`deko3d_shaders.bin`: 0 variants).
- No shaders missing (no `shader_capture/`), no crash.

## 2026-11-12 (first Switch build: shaders, logging, build script)

- Mark: the PC Renut build plays correctly ("nothing bad to report, it looked correct"). Next: the Switch build, with
  enough logging from the first NRO.
- Shader chain on Linux in the cloud: XenosRecomp built with `-DXENOS_RECOMP_DXIL=OFF` (the DXIL target is
  Windows-only; with it on, configure fails on `Microsoft::DXIL`). SPIRV-Cross (`adec7acb`) and uam
  (`app/thirdparty/uam`) built with clang 20 into `out/deko3d_tools`; zstd's CLI from XenosRecomp's copy
  (`out/zstd_build`). The deko3d tools find the Linux binaries (no `.exe` off Windows). Early-alpha pass: 11,789 pixel
  shaders. SPIR-V pass: one shader at a time (about 0.3 s each, single-threaded): split in two halves run side by side.
- Renut's 27 host shaders made for deko3d in the cloud (`tools/deko3d_host_shaders_all.py`, all ok) into
  `out/renut_switch_host`; the Switch build copies them (`BANJO_DEKO3D_HOST_SHADERS_PREBUILT`), so Mark's PC needs no
  SPIRV-Cross or uam.
- `renut_switch_shader_cache.py` reads the `#embed` SPIR-V blob of the PC build's `shader_cache.cpp` (the PC build
  already translates with the early-alpha override, so its table is the one the Switch needs).
- Logging on by default (`app/tfu_switch_test.toml`, copied as `renut.toml`): build stamp first in every log and crash
  report (`app/cmake/tfu_build_stamp.cmake`), `rex_stderr` and `rex_profile` keep the launch before as `.prev.log`,
  info-level main log, `rex_profile.log` every 10 s (threads, clocks, temperatures, audio), stack sampling, GPU profile
  every 10 s, a PNG every 60 s, missing shaders to `shader_capture/`, stall dumps after 1 s without a frame. Settings
  are rr6-nx's Switch set without RR6's game-specific keys (those keep their defaults, as on the PC).
- Results: SPIR-V for all 12,942 shaders; SPIRV-Cross failed on one (`ps_E17830DA24CCE4C0`, "Maximum compilation
  loops", a SPIRV-Cross bug), so 12,941 in the package (GLSL pass 26 min, single-threaded). `deko3d_glsl.bin` 52 MB,
  sha256 `c3b17a64ed09e697`, copied to Mark's `out\renut_switch` (three 18 MB parts, joined there, hash matches).
  DKSH compiles (uam) were skipped for time: a GLSL uam rejects shows on the console as a missing shader.
- devkitPro on Mark's PC: devkitA64, libnx, deko3d, switch-zlib; no switch-zstd, which nothing needs without NVK, so
  the SDK links it only when installed.
- `tools/win/build_switch.ps1`: checks every input before building, then configure, build, and the SD folder with the
  SHA-256; the ELF kept in `out\nx\elf` by hash. NRO: "The Force Unleashed", FurryLilManChild, 0.1.0, no icon.

## 2026-11-11 (Renut mapping and port)

- Mirror of Fate (XDK 21256) as a reference: 7 confident pairs of 38, not the draws. Beautiful Katamari (5632, Mark's
  disc): 316 of Renut's 321 hooks paired (222 by state table index, 74 d3d_match, 20 by link order / twins), device
  layout and guest data mapped. Details: `docs/renut-map.md`, `docs/renut_map/`.
- Renut's D3D layer ported (katamari-nx hooks + ultimate improvements, no XDK 2135 layer); compiles.
- 68 shaders dumped from default.xex (65 vertex, 3 pixel) to `out/renut_shader_dump` (also on Mark's PC). The level
  packs' shaders are XMemCompress-packed: Renut captures them at run time.
- `build_pc.ps1 -Renut`: installs Python xxhash, points RENUT_SHADER_DUMP_DIR at `out\renut_shader_dump`, and fetches
  the DXC binaries XenosRecomp links (the first -Renut build failed on the missing `dxcompiler.lib`).
- -Renut build 2 stopped at `command_buffers.cpp`: its replay snapshot ends at TFU's device size 0x31A8 (N&B base's,
  Katamari's 0x31A0), so the last 64-byte chunk is 40 bytes, not 32; `TakeLastChunk` takes either now. All 48 Renut
  files then pass a Linux syntax check (stub host-shader headers).
- Renut boot 1 (120 s): Renut's renderer started (D3D12 on the AMD GPU), then a black screen and silence after the
  schema files: the game's own CreateDevice ran (VdSetGraphicsInterruptCallback, VdInitializeRingBuffer reached the
  kernel; no "Guest D3D device created" line) and it waited on a GPU that was not there. Cause: the post-codegen pass
  `direct_calls.py` ran with the Renut-off build's first codegen, before the hooks had TFU addresses, so the calls to
  every newly hooked function were direct (`__imp__sub_822D8710` in Mark's generated tree) and skipped the hooks. The
  build reruns the passes only with a new codegen; `build_pc.ps1` now reruns `post_codegen.py` when a hook source is
  newer than its last run.
- Renut boot 2: the hooks run. "Guest D3D device (type 1) created", render thread up at 1280x720, 9 vertex shaders
  captured (not in the dump), then 40 ms later a crash: a guest write to `0x83C20000` (the image's read-only `.edata`,
  right after `.data` / `.tls`) inside `sub_822C0450`, the CRT memcpy (renut.pdb + llvm-symbolizer). A copy running
  off the end of `.data`. The SDK's guest crash report (registers, back chain, memory at each register) was
  Switch-only: now on the PC too (`sdk/src/system/xmemory.cpp`, VirtualQuery-guarded reads), to name the caller.
- Renut boot 3 (with the report): the memcpy's caller is `sub_822E5650`, `D3DVertexShader_GetFunction` by its body
  (size from the container copy at +0x36C/+0x370, physical part's pointer at +0x20), called from game code `8270DD34`
  right after CreateVertexShader. The object was a Renut shader record, which has none of that layout: length
  `E007FAAE`. N&B and Katamari never read shaders back, so Renut had no hook. Now hooked, with its pixel twin
  `sub_822E55F8` (+0x28/+0x2C/+0x30, +0x18): the record keeps the container it was made from and GetFunction returns
  its size or copies it; guest-laid-out objects (XGRegister*Shader) still go to the originals.
- Renut boot 4: further (audio device open, the game's gamma ramp applied, render targets made), then a read off the
  end of a guest stack in `sub_822EC050` (lr), called from game code `826FBA98`: XDK 6534's
  `D3DVertexShader_Bind(shader, Flags, decl, strides, pixel shader)`, reading the element count at +28 of a Renut
  declaration record. Hooked onto Renut's bind (N&B's three-argument form; the pixel shader is not needed).
- Renut boot 5: no crash, music and sound, full speed, black window. The log: 30 to 36 game frames a second
  through Renut (340, 365, 300 frames per 10 s), 2,743 then 16,593 draws per 10 s, render thread 3 to 28 ms a frame,
  GPU 1 to 6 ms, every shader in the cache (339 captured last run, none missing now), 180 textures, the front end's
  level packs loading, input read. So the game draws; the picture does not reach the window. Diagnostic build: one
  line a second for 30 s with the swap's front buffer and what Renut knows of it, and the frame's resolves and Present.
- Renut boot 6 (the diagnostic): every swap is the device's auto front buffer `AEFA0250`, a host texture the GPU
  wrote, 1280x720, bindless slot 12; Present runs once a frame (auto back buffer `AEFA02D0`). The game's own resolves:
  8 on the first frame, none for swaps 31 to 631 (about 20 s: the intro), then 17 a frame to its own targets
  (`40F57D40`..., the front end in 3D). So the frame reaches the swap as Renut expects. Next: read the back buffer back
  at Present 60/300/700/1000 (average colour, lit pixels, a PPM in `out\pc\logs`), to tell an empty frame from a
  present-side problem.
- Renut boot 7 (the read-back): **the game draws correctly.** The back buffer at Present 60 is the NaturalMotion
  intro, at 1000 the title screen ("PRESS START"), whole and in place (words come back R G B A on the PC, not FF R G B
  as draw.h says). So the black window is the last step: front buffer, scan-out pass or swap chain. Mark asked for
  fewer rounds: the next build checks several things in one go (`tools/win/present_test.ps1`): back buffer and the
  SDK's scan-out capture at 7 Presents; on D3D12 with the scan-out hand-over (A/B 74) and the game's gamma (A/B 86)
  switched off and on every 10 s; then a second run on Vulkan.
- Present test (D3D12 with A/B, then Vulkan): the back buffer held the game's picture at every capture; the
  scan-out capture was black at every one but Present 1300, which fell in the gamma A/B's off half (Mark saw the
  title screen for a moment in run 1; Vulkan black throughout). The log: "game gamma ramp applied ... [0]=0/0/0
  [64]=0/0/0 [128]=0/0/0 [255]=0/0/0", right after the default ramp. **Cause:** XDK 6534's
  `SetGammaRamp(device, Flags, ramp)` / `SetPWLGamma(device, Flags, pwl)` keep the first release's Flags (both move
  r5 to r4 for their converters); the hooks took the two-argument form, read Flags as the ramp's address and stored
  zeros, and the scan-out multiplied every frame by them. Fixed with three-argument hooks. The read-backs stay, behind
  `TFU_PRESENT_DIAG=1` (present_test.ps1 sets it), so normal runs have no GPU waits.
- Gamma fix boot: menus and the title screen show; in the first level only the radar showed. 863 shaders captured;
  every skipped draw (67,000 to 153,000 per 10 s) was "no host shader". The rebuild with them failed: 20 vertex
  shaders read POSITION6, which the vertex input table lacked (DXC: "partial explicit stage input location
  assignment"). Added at location 29 (`shader_common.h`, the host's input layout reads the same table); a scan of all
  1,416 of TFU's vertex shaders found no other missing usage.
- **Offline shader dump** (Mark's OK): the level packs (`LevelPacks/*.lp`, 107) are a section table plus one
  XMemCompress LZX stream per section (window 2^17, 32 KB frames; some sections start at the next 64 KB).
  `tools/tfu_lp_unpack.c` unpacks them, `tools/tfu_pack_shaders.py` writes every container: 12,874 distinct shaders,
  and all 1,215 the runs had captured are among them (the naming agrees). Not complete yet: 12 packs stop at one
  section that does not decode, and 22 small packs (other header) unpack to nothing; the renderer still captures
  what is missing.
- XenosRecomp for a cache that size: the compressed blobs go to files beside `shader_cache.cpp` and the source takes
  them with `#embed` (the number lists would have made a 400 MB source file), zstd level 19 above 4,000 shaders, and a
  shader it cannot translate is left out of the cache instead of failing the build (`BANJO_RECOMP_FAILURES_FATAL=1`
  for the old way).
- Build with the 12,942-shader cache: OK (about 20 min). In game: still only the radar. The log: no shader missing, no
  draw skipped (about 200,000 draws per 10 s at 24 to 25 fps, GPU 10 ms a frame, render thread 40 ms). So every draw
  is drawn and the picture is lost between the scene's passes and the back buffer. Diagnostic build: with
  `TFU_PRESENT_DIAG=1`, every 1,800th Present captures a whole frame (render target 0 before each resolve and each
  EndTiling, then the back buffer, as PPMs), the first BeginTiling calls' arguments are logged (the 0.69 pairing),
  and the swap line runs every 10 s past the first 30.
- Capture run (Kashyyyk, Present 1800 and 3600): resolve 07 (to `40F57EC0`) holds the whole scene, correct (Vader,
  trees, sky). The target the next pass draws to (read before resolve 08, then resolved to `40F57EC0` again as the
  scene) is black except where the force-push ring's distortion redraws the scene texture. Mark: force effects show
  bits of the level. So the scene's pixels do not carry over to the target the post passes draw on. Renut keys host
  targets by EDRAM view (base, pitch, samples, scale) and host format (`TargetKey`); a second surface over the same
  EDRAM with any of those different is a separate, empty host target. The format pair halo3-nx handles is one case
  only, and only at a resolve. Next: the read-back and the resolve trace (`banjo_renut_diag_resolve_trace`) log each
  target's EDRAM base, pitch, samples and guest format, to see which of these TFU changes.
- Logging run: none of the four. TFU's post passes go through two surface objects (`AF047E70`/`AF047EF0`, `AF048BF0`)
  whose headers it rewrites from EDRAM view to view. The scene: `AF047E70`, base 720 pitch 1280, 7e3 float (3F),
  generation 25321, 516 writes. The next pass: `AF047EF0`, the same view and format, generation 25322, 0 writes: the
  same target slot rebuilt. Renut counts surfaces per view and destroys a view's target when its last surface moves
  off it (`UnuseTarget`), so a header rewrite threw the scene away. On the console EDRAM keeps its pixels. Fix:
  targets with no users are kept (the oldest given up past 96).
- **Renut boot with the target fix: the game shows in play** (Mark, Kashyyyk). Log: 29.3 to 30 fps held, render thread
  4.8 to 7.5 ms a frame (40 ms before: the constant target rebuilds), GPU 11 to 12 ms, about 190,000 to 270,000 draws per
  10 s, no shader missing, nothing skipped. Two warnings at boot only (a resolve through the 8-bit view of EDRAM last
  drawn through the 7e3 float view, and the reverse).

## 2026-11-11 (first PC boots)

- PC build fixes: `build_pc.ps1` builds glslangValidator first (the renderer's EDRAM shaders; rr6-nx built it with a
  separate script) and quotes its arguments (PowerShell split `-D...=3.5`); codegen runs once, in the build. The
  Renut-off link missed 8 Renut pacing symbols called by `rr6_keys.cpp` and the profiler: guarded by `TFU_NO_RENUT`.
- Boot 1 (60 s): game code ran (disc mounted, schema files read), black screen: `gpu_plugin` empty, so no GPU at all.
  ~50 `NtQueryInformationFile(XFileXctdCompressionInformation) unimplemented` on the `.z` schema files; the game goes on.
- Boot 2 (`--gpu_plugin=xenos`, the SDK's emulated GPU, 120 s): **in game**. Mark played the prologue as Darth Vader.
  Shadows show as glitchy black stripes (emulated GPU). Log: 1 error (`ResolvePath(\Device)`), first-run
  `save:\SWTFU.BIN` not found, audio steady (1,877 frames with data per 10 s, no gaps), no crash.

## 2026-11-10 (evening, after Mark's OK to use ChanseyIsTheBest's code and the rr6-nx base)

- Base tree copied from rr6-nx 1.0.1 (`docs/upstream.md`). RR6's game-address code taken out of the build:
  `src/rr6_game_fixes.cpp` removed; hot lists (`hand.txt`, `localized_bulk.txt`) emptied; function lists and overrides
  emptied; the diet keys and RR6's setjmp/longjmp removed from `renut_config.toml`. Files with other games' address
  hooks stay guarded off (`KATAMARI_BANJO_ADDRESS_HOOKS`).
- FFmpeg's three generated headers (rr6-nx kept them out of git) written: `config.h` picks the platform's header, then
  `config_masseffect_wmv3.h` (the fork's WMV3 decoder: TFU's cutscenes are WMV9). `avconfig.h`, `ffversion.h`.
- Generator (`sdk/out/linux-amd64/Release/rexglue`) built with clang 20 (clang 18 lacks `std::expected` with
  libstdc++ 13; ReXGlue needs it).
- setjmp `0x828197A0`, longjmp `0x8281A930` (the CRT's; both Lua copies use them): by shape, as rr6-nx's pair.
- Codegen pass 1: one UnresolvedCall (`b 0x8242E02C` from `0x8242E06C`): a leaf with no .pdata, declared whole
  (`0x8242DFF0`, size 0x80). Pass 2: 461 files, 227 translation units, 68,344 functions, 240 MB, 159 s on 2 cores.
  Code range `0x821C0000 + 0xD3C930` (the .text and the .embsec_ sections).
- `switch_check.py`: 928 switches, 409 indirect; truncated 0, wrong targets 0, missed 0. Four tables with no bound in
  the code (8, 8, 17, 16 cases generated) and two odd shapes generated as indirect calls, as in rr6-nx.
- Disc extracted on Mark's PC into `assets\game_root` with `tools/xdvdfs_list.py --extract-all` (1,376 files).
- Pointer sweep (`pointer_fixpoint.py`): pass 1 accepted 138 of 167 pointer-only candidates; pass 2 found none
  (fixpoint). Gap sweep (`gap_sweep.py`): 109 + 6 accepted over two passes, pass 3 none. Both tools now run the
  generator directly when not on Windows (they called codegen.ps1 only).
- `override_labels_check.py` flagged 52 of the 253 accepted entries as labels inside a function (51 from the gap
  sweep: instructions running into them, mostly epilogue tails after `mtlr r12`, and conditional-branch targets; 1
  from the pointer sweep, an SEH scope). All 52 removed, as rr6-nx did with its flagged entries: 201 stay.
- Codegen warned "Unresolved conditional branch" around 0x82BABC88: the pointer sweep had accepted an inline jump table
  (words after a `bctr`, its address built with lis/addi). A scan of every entry found 11 such tables; all removed.
  Codegen then clean: 68,528 functions, no unresolved branches. 190 sweep entries in overrides.toml, 1 in renut_funcs.toml.
- PC scripts: `tools/win/env.ps1` (LoS1-nx's), `build_pc.ps1` (TFU: extracts, junctions `app\assets`, Renut off for
  the first builds), `boot_test.ps1`.

## 2026-11-10

- Repo and `C:\TFU-nx` set up by Mark, disc image in `assets\`.
- Disc listed and `default.xex` extracted with our own `tools/xdvdfs_list.py` (ChanseyIsTheBest's `extract_iso.py` was
  not run: third-party code waits for Mark's OK).
- `default.xex` header and decrypted image read with `tools/xexscan.py`: XDK 2.0.6534, 13.5 MB of code, Havok 5.5.0,
  Euphoria, DMM, Lua 5.1 and 5.0.2, XMedia WMV cutscenes. Details in `docs/scan.md`.
- Reviewed ChanseyIsTheBest/rr6-nx 1.0.1 (released 2026-11-10) for the newest Xbox 360 port practice. Summary in
  `docs/design.md`.
