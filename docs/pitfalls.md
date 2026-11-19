# Pitfalls to avoid (S4ep1-nx session list)

Mistakes and wrong conclusions from this port, newest first. Reviewed with Mark at wrap-up, then moved into the skills.

- 2026-11-16 (S4ep2-XBLA-nx): Mark's first PC build failed five times in a row, each a separate cause, and the first
  two were the 2026-11-14 copy pitfall below, repeated: the kickoff copy dropped the force-tracked files (rexglue.cmake,
  FFmpeg's three headers) and all 125 README/LICENSE files (fmt's CMake lists its README as a source). Then a source
  ordering race on a fresh tree (the hot-copy files before the code generator), a codegen warning left open since the
  first codegen (an unresolved `b` that became a call with no name), and a jump table with no bounds check that the
  generator read to 513 entries. Before the FIRST PC build of a new game, all of these: `comm` of both repos'
  `git ls-files` (must be empty outside docs/); resolve every codegen warning; run post_codegen.py and a
  `-fsyntax-only` pass over every generated file and every app source in the cloud (about 15 minutes on 2 cores);
  read this file first.
- 2026-11-19 (S4ep2-XBLA-nx): a change to the build script itself (gate 1's self-update) reaches Mark's copy only by a
  pull: tell him to pull once whenever a build or test script changed. And `powershell -File script -Extra 'a','b'`
  passes "a,b" as one string: test every new command form (boot_test.ps1 now splits and prints the arguments).
- 2026-11-18 (S4ep2-XBLA-nx): I planned Renut's capture-and-rebuild loop for shaders, the method that left 185 of
  S4ep1-nx 0.5.0's shaders out, though S4ep1-nx had just replaced it. Every Renut port takes its shaders from the
  game's own files before the first boot (default.xex and every data archive) and checks a run's captures against
  them; capture is only a check that nothing comes from elsewhere. And a "one time" build step that runs before its
  input exists (the code image) silently does nothing: run such steps whenever the input is there.
- 2026-11-18 (S4ep2-XBLA-nx): a fix for one crash is not a working boot: say what the fix cures and that the next
  run shows what comes after it. For a crash, symbolize the host PC first (`llvm-symbolizer --obj=renut.exe
  0x140000000+offset` with renut.pdb beside it, both staged from out\pc): it names the generated function at once,
  no diagnostic build needed. When the base game's alignment leaves a library function unpaired, the base game never
  called it: hook or vet every unpaired function the new game calls before the first boot.



- 2026-11-14: copying TFU-nx's tree with tar and committing with `git add -A` silently dropped four files TFU tracks
  past ignore rules (`app/generated/rexglue.cmake`, three FFmpeg config headers), and Mark's first PC configure failed.
  After copying a base tree, compare `git ls-files` of both repos before the first commit and force-add what is missing.
- 2026-11-14: the cloud boot tests ran a Linux build with Renut off; nothing exercised Windows' `build_pc.ps1 -Renut`
  path. Mark's first -Renut configure failed: the script checked and installed xxhash with the `python` on PATH while
  CMake's find_package(Python3) chose another python.exe. Fixed by passing -DPython3_EXECUTABLE (the PATH python) and
  checking the module in that one. Lesson: a cloud test only covers the build path it ran; say which path was tested,
  and when a Windows path has an interpreter or tool lookup, make the script pin it instead of trusting two lookups to agree.
- 2026-11-14: after porting Renut I compile-checked only the files I had changed; a layout-dependent static_assert in
  an unchanged file (command_buffers.cpp's last snapshot chunk: 24 bytes for Sonic 4) and a missing tool
  (dump_shaders.py, never in this tree; TFU always had its dump folder) would have failed Mark's build one at a time.
  Check every source of the target (`ninja -t commands` plus `-fsyntax-only`) and walk the build script's steps for
  tools or folders the base project had on disk but never committed.
- 2026-11-14: the cloud boot tests ran the Linux build, where I had gated the Windows path wizard off; on Mark's PC the
  wizard waited for a click on its first run and both boot tests sat at a black window. And boot_test.ps1's "emulated
  GPU" run was really Renut in a -Renut build (banjo_renderer defaults to "native", which falls back to Renut). A cloud
  test only stands in for Mark's run when both take the same code path: list what differs (OS-gated code, defaults,
  the renderer chosen) before saying a cloud result carries over.
- 2026-11-14: the pointer sweep missed a function whose address is built in code, because the generator had already
  merged it into the previous function (it ran past that function's .pdata end). Check every lis/addi code address that
  lands on a .pdata end without being a function start, at kickoff.
- 2026-11-15: Renut's hooks were moved to Sonic 4 by matching code shape and call sites, without checking each
  function's arguments. Sonic 4's D3D is LTCG-linked (TFU's and LoS1's are not, or less so): CreateDevice and Present
  take no arguments, draw calls drop constant ones, SetViewport takes floats. The first Renut run read garbage. For
  every hooked function, list the argument registers it reads in the generated code and compare with the hook's
  signature before the first Renut build.
- 2026-11-15: banjo_late_input (on by default since banjo-nx) delays the first controller read after each Swap. It
  assumes the game reads input on the thread that swaps. Sonic 4 reads on a separate frame thread: the delay put the
  game in slow motion (a quarter speed) on Renut only. Check where a new game reads its controller before keeping it.
- 2026-11-15: a resolution setting copied from TFU was "fixed" by changing the toml without checking that it took
  effect. The cvar's range rejected the value and the scale only applied with a known composite shader. Any setting
  change must have a log line that proves the new value is in force, and the first log after it must be checked.
- 2026-11-15: a feature carried over from another game's port (sasrt-nx's EDRAM depth restore) broke a picture in this
  one (the title's white sky). When a picture is wrong and shaders, textures and constants check out, A/B the inherited
  renderer features (depth restore, skips, hand-overs) before writing new code.
- 2026-11-19 (S4ep2-XBLA-nx): I called "0 resolves a frame" expected from one misread branch (a flag that only picks a
  tiled or untiled path read as "skip"), and it stood for two builds. A texture dump then showed the screen sampling a
  never-resolved black texture. When a count that should be busy is 0, treat the 0 as a lead, not as expected, unless
  a log line proves the path; and dump what the final draws sample early in any black-screen hunt.
- 2026-11-19 (S4ep2-XBLA-nx): preflight's compile check took its file list from the base port's build, so a source
  this port added (s4/s4_rt_diag.cpp) was never compiled before Mark's build. It now also checks every source
  app/CMakeLists.txt lists that the base build lacks (Switch-only files excepted).
- 2026-11-19 (S4ep2-XBLA-nx): the phase test (bc904028, never built until 5c61c45b) declared
  `extern std::atomic<uint64_t> g_banjo_game_frames;` inside render/draw.cpp's anonymous namespace: it compiled and
  failed at the link on Mark's PC. Preflight's compile check is syntax only and cannot see that; it now also fails on
  any extern declared inside an anonymous namespace. Code committed but never built (a test judged unsound and not
  sent) still rides along in the next build: remove it when the test is dropped.
- 2026-11-19 (S4ep2-XBLA-nx): kDeviceCapsVa was written as 0x820F80E0 from "lis 0x8211, addi -0x7F20" (that is
  0x821080E0): GetDeviceCaps handed the game a block of strings as its caps, and every D3DXCreateTexture failed with
  E_OUTOFMEMORY, so the render targets' textures did not exist (the black screen, then a wrong picture once the port
  made them itself). The tag comment showed the derivation and nobody recomputed it. tools/s4ep2_addr_refs.py (in
  preflight) now checks that each tagged address naming a function is built by that function's code.
- 2026-11-19 (S4ep2-XBLA-nx): override entries appended at the end of app/overrides.toml land in its last table
  ([[switch_tables]]), not [functions]; the generator ignores unknown keys silently. Preflight now checks every
  [functions] entry is registered in the generated renut_init.cpp.
