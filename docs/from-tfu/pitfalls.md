# Pitfalls to avoid (TFU-nx session list)

Mark's rule: every mistake or wrong conclusion in a port session goes on this running list. At wrap-up we review it
together, and the lessons move into the port skills. Newest first.

## 2026-11-13

22. **A command block that dropped a flag the build needs.** Build 4's PowerShell block had `build_pc.ps1` without
    `-Renut`, so the PC build left Renut out, and the 805 copies' hooks (which read Renut's A/B word `g_ab_off`) failed
    at the link after the whole game code had compiled. Fixed: the right command, and `banjo_hot_guest.cpp` defines
    the word when Renut is not built in. Lesson: copy the build commands from the last block that worked, not from
    memory, and say what each flag is for.
21. **"I'll read it while you get the files", then not reading it.** Same failure as 20, a day later: I said I would
    read test 6's CPU profile while Mark fetched the SD card's files, and the turn ended with only the files staged.
    Mark caught it and made it a hard rule (saved to project memory): if I say I am doing something, I do it. Lesson:
    a sentence that promises work is written only after the work's first tool call has run (or the background job
    is started), and the turn does not end with promised work not running.

## 2026-11-11 / 12

20. **"I'll start on it now", then not starting.** I told Mark the shader cache work would start while build 3 built,
    then ended the turn without starting it; he had to point it out (this has happened before in this project). Lesson:
    when a message says work is starting, the work is already running in the background before the message is sent,
    or the message says it will start after his reply.
19. **Leftover files in Mark's folders.** Working around the file copy problem left `renut_4core.toml` and
    `renut_new.toml` beside `renut.toml` in the SD folder (all three the same), and the shader package's parts,
    a tar and a test script in `out\`. Mark had to ask which file to move. Lesson: the SD folder holds exactly what goes
    on the card; delete every helper file in the same step that made it (deletion was granted on 2026-11-12).
18. **A command block without `git pull` after a new commit.** The 540p settings were committed after Mark's PC build
    had pulled, and the Switch build's command block had no `git pull`, so the SD folder got the old `renut.toml`.
    Mark noticed. Lesson: every command block starts with `git pull` when anything was pushed since his last pull.
    (Fixed by writing the file into his SD folder directly; `device_commit_files` again left the old file in place when
    overwriting, so it went in under a new name and was copied over.)
17. **A header change that would have recompiled the whole game.** To log a settings parse failure I declared a new
    function in `rex/cvar.h`, which every generated game file includes (through `rex/chrono/clock.h` in the pch).
    Caught before Mark built it: reverted, the declaration moved into `rex_app.cpp`. Lesson: before changing an SDK
    header, check whether `app/generated/renut_pch.h` reaches it.
16. **Edits that changed line endings.** Editing files with Python's text mode turned 13 CRLF files into LF, so the
    commits showed whole files rewritten (thousands of lines) for few-line changes. Restored. Lesson: edit with
    `newline=''` (or the Edit tool) and check `git diff --stat` for line counts out of proportion before committing.
15. **A settings file that was never parsed before it shipped.** The first Switch test's `renut.toml` merged rr6-nx's
    two files and kept a key twice (`renut_gpu_profile`). TOML rejects that, the SDK loads the file before its log
    opens, and so the whole file was silently ignored: no CPU sampler, no captures, rr6's Switch settings off. Found
    only from the log's own settings summary. Fixed: deduplicated, the build script parses the file before copying
    it, and a parse failure now goes to `rex_stderr.log` and to the main log as an error. Lesson: parse every
    generated config before it ships, and make "which settings ran" a log line.
14. **A skill rule not carried into the tree.** The x360 port skill says to compile the generated `<game>_register.cpp`
    at -O0 on the Switch. The tree (from rr6-nx) did not, and GCC sat on that one file at -O3 for 40+ minutes at step
    1171 of 1172. Lesson: before the first Switch build, check the skill's Switch build flags against the CMake files.
13. **A new build configuration handed over without a configure run.** The Switch build without the NVK driver had
    never been configured from this tree: plume still required `libvulkan.a`. Mark spent a second round finding it.
    Lesson: before the first build of a new configuration, run its configure in the cloud (a stand-in devkitPro with
    the host GCC is enough to reach every CMake check), so only compile errors are left for Mark's build.
12. **A Windows build script that trusted an inherited environment variable.** devkitPro's installer sets
    `DEVKITPRO=/opt/devkitpro` (its MSYS2 form). The toolchain file preferred it over `-DDEVKITPRO`, because CMake's
    compiler checks run in fresh projects that only see the environment, so the first Switch configure failed. Lesson:
    a build script sets every environment variable its toolchain reads, and does not rely on what the machine has.
11. **A long job left in a time-limited background slot.** The SPIR-V pass ran single-threaded in a background task
    that is stopped after 30 minutes, and needed about an hour; I had also misread its rate from the first minutes.
    Caught in time: the remaining shaders were split into two halves with a 2-hour limit. Lesson: measure a long job's
    rate over a few minutes first, then start it with a time limit that covers it, using every core.
1. **The list itself started late.** This rule was in the project preferences, and the skills should have been read in
   full before starting. This list should have existed from the first mistake.
2. **A build trigger that was too broad cost a full game-code rebuild.** `build_pc.ps1` reran the post-codegen
   passes whenever any source file was newer than their last run. The passes rewrite all 227 generated files, so even
   a two-file renderer change recompiled the whole game (20+ minutes). I also told Mark that build would be "quick"
   without checking what the script would trigger. Fixed: they rerun only when the set of hooked functions changes.
   Lesson: before calling a build quick, trace what it rebuilds. A step that rewrites generated code must be keyed to
   the input that actually changes its output.
3. **Hooks moved from a reference game kept that game's argument forms.** XDK 6534 keeps the first release's extra
   arguments: `SetGammaRamp(device, Flags, ramp)` / `SetPWLGamma` (black window: a zero gamma ramp), and
   `D3DVertexShader_Bind(shader, Flags, decl, strides, pixel shader)` (crash). Lesson: when porting hooks, check every
   hooked function's argument registers in the new game's code, not just its address and shape. Hooks whose comments
   say "TU3 dropped an argument" are the first to check.
4. **D3D functions the game calls that the reference never did.** `D3DVertexShader_GetFunction` read Renut's shader
   records as guest objects (crash). Lesson: before the first Renut boot, list the D3D library functions the game calls
   that are not hooked and that read resource objects at fixed offsets.
5. **One question per test round.** Several boot tests each answered one thing (crash report, swap, back buffer, scan
   out). Mark's rule is to check several suspects per build. The combined present test found the gamma bug in one run.
6. **Diagnoses stated as known.** "Tiling is my top suspect", then a different theory after Mark's screenshot. Say
   "I think it might be X; this test checks X, Y and Z", and keep to that.
7. **A compile check that skipped files.** The first Linux syntax check missed `command_buffers.cpp`, whose static
   assert then failed on Mark's PC. Lesson: check every file of the changed target (stub the generated headers) before
   handing over a build.
8. **Files the build needs that git does not carry.** The DXC binaries (git-ignored upstream) were missing on the
   first `-Renut` build. Lesson: before the first build of a new configuration, list the ignored files and directories
   it reads.
9. **A post-codegen pass that ran before the hooks existed.** `direct_calls.py` made calls to newly hooked functions
   bypass their hooks (black screen, the game's own CreateDevice ran). Lesson: any pass keyed to the hook set must
   rerun when that set changes (see 2).
10. **Command format.** Commands went out as numbered steps, then as a block with an `if`. Mark's rule: one clean code
    block, one command per line, no chaining.
