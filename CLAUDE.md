# S4ep2-XBLA-nx

Native Nintendo Switch (and PC) port of Sonic the Hedgehog 4: Episode II, Xbox 360 XBLA version, by static
recompilation (ReXGlue, PowerPC to C++) and Renut's Direct3D-level renderer on deko3d. Named for the XBLA version
because a port of the mobile version (boraeskicioglu's s4ep2-nx) already exists.

## Who does what

- Claude writes all code, scripts and docs, reads logs, makes cloud Switch builds and cloud boot tests.
- Mark (FurryLilManChild) runs PC builds in PowerShell, tests on his V1 Switch and PC, and sends logs.
- Mark's folder: `C:\S4ep2-XBLA-nx`. The XBLA package is in `assets\`, extracted to `assets\extracted\`
  (`default.xex`, `data.cpk`). Logs go to `C:\S4ep2-XBLA-nx\out\sd\switch\switchlogs`.

## Hard rules

- No game data in git, ever (see `.gitignore`). Run `git status` before every commit.
- No em dashes anywhere. No decorative emoji.
- Commit as `FurryLilManChild <346934328+FurryLilManChild@users.noreply.github.com>`.
- Running an upstream project's scripts or building third-party code needs Mark's OK first.
- Never ship a Switch build without the renderer, sound, input and game code all in it.
- Present test findings and the next build's plan to Mark and wait for his go-ahead (cloud boot work before the title
  screen is the exception: build and fix without asking, with an update at least every 10 minutes).
- Before Mark's first boot test of any build that adds or remaps hooks: run the whole-program sweeps (switch-port-base
  "Before the first boot test"): tree check, clean codegen, full compile check, `tools/s4ep2_d3d_sweep.py` with every
  unpaired reachable function hooked or explained, and every hard-coded address checked.
- Gates (docs/reporting.md), every time: build_pc.ps1 builds only GitHub's latest (gate 1); `tools/preflight.py`
  passes before Mark gets build commands (gate 2). While Mark is at his PC: `--quick` (seconds) before the commands, and
  the full run (`--codegen --full` when hooks or codegen changed) in the background while he builds, any failure told
  to him at once; the full run first only when he is away or before a release. Build commands are pasted from
  `tools/build_commands.py` (preflight prints them), never written from memory;
  `tools/read_log.py --expect <commit>` before any statement about a test, WRONG BUILD stops the report (gate 3);
  reports name the build, the evidence for each fact, and what is not checked (gate 4); an independent agent checks
  the claims before a first boot, a release, or the test after two failed ones (gate 5).
- Never state a theory as a diagnosis: say what a test checks and what result confirms it.
- Visual issues (Mark, 2026-11-22, every port): the first diagnostic is a full frame capture (render/draw.cpp
  "full frame captures", read with `tools/s4_capture_read.py`), before any theory or fix build.
- Any setting change must be proven in force by a log line, checked first in the next log.
- Anything to get rid of goes to `C:\Games\04 MY PORTS\FOR DELETION` (Mark empties it in bulk). Temporary transfer
  files are written there directly and joined from there.

## Goals (Mark)

- Same bar as Episode I: a steady 60 fps at stock clocks on the Switch, steady game speed.
- Episode METAL playable without an Episode I save or license (the 360 unlocks it by checking the Episode I license:
  string "Check Ep1 License", referenced at 0x823778A8).

## Layout

- `docs/scan.md`: what the package and default.xex hold.
- `docs/progress-log.md`: newest first, one entry per session, with evidence.
- `docs/pitfalls.md`: the running list of mistakes to avoid (carried over from S4ep1-nx).
- `docs/upstream.md`: each upstream, its pinned commit, license and what we take.
- `docs/from-s4ep1/`, `docs/from-tfu/`, `docs/from-rr6/`: the base trees' notes, for reference.
- `sdk/`, `app/`, `shaders/`, `tools/`: copied from S4ep1-nx 0.5.0 (itself from TFU-nx and rr6-nx 1.0.1).
