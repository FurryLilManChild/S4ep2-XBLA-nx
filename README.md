# S4ep2-XBLA-nx

A native Nintendo Switch (and PC) port of **Sonic the Hedgehog 4: Episode II** (Xbox 360 XBLA, 2012), made with
[ReXGlue](https://github.com/rexglue/rexglue-sdk) static recompilation, following
[S4ep1-nx](https://github.com/FurryLilManChild/S4ep1-nx) and [rr6-nx](https://github.com/ChanseyIsTheBest/rr6-nx).
Not the mobile version's port: this one recompiles the Xbox 360 `default.xex`.

It is not an emulator: the PowerPC code of the game's `default.xex` is translated to C++ and compiled for the Switch
and the PC, and the game's Direct3D calls are drawn by a native renderer (deko3d on the Switch, D3D12 or Vulkan on the
PC).

## Status

Version **0.5.0**: feature complete, needs a full playthrough.

| | |
| --- | --- |
| Switch | Plays at a steady 60 fps at stock clocks in handheld, drawn at 960x540. Title, zones, acts and special stages tested; not yet played start to finish; docked not tested yet |
| PC | Builds and plays (Vulkan or D3D12, or the emulated GPU); no PC release yet |
| Episode METAL | Unlocked without an Episode I save or license (`s4_episode_metal`, on by default) |
| Port menu | Hold **-** and **+** together: FPS counter, handheld and docked resolution, handheld GPU profile, stick dead zone; Save and close keeps the choices |

Known issues: see [Known issues](#known-issues). Session notes: [docs/progress-log.md](docs/progress-log.md).

## What you need

- A Switch with custom firmware (Atmosphere) and the homebrew menu, launched through title takeover (hold R while
  starting a game), not the Album.
- Your own copy of the Xbox 360 XBLA game, extracted from its package: `default.xex` and `data.cpk`. Supported
  `default.xex` SHA-256: `30583685039e56080bb69d0856a9a938862ea05340a3e0faa9dd96eb424d8c90`.

## Installing (Switch)

1. Download `S4ep2-XBLA-nx-0.5.0-switch.zip` from the Releases page and copy its `switch` folder to the root of the SD card.
   That gives `/switch/s4ep2-xbla-nx/` with `s4ep2.nro`, `renut.toml`, `deko3d_shaders.bin` and `deko3d_variants.txt`.
2. Make the folder `/switch/s4ep2-xbla-nx/game_root/` and put `default.xex` and `data.cpk` from your own copy in it.
3. Start the homebrew menu through title takeover (hold R while opening a game) and run `s4ep2.nro`.

At every start a short "Preparing shaders" screen reads every shader variant listed in `deko3d_variants.txt` from
`deko3d_shaders.bin` (about 2 seconds), so effects do not stutter the first time they appear in play. A variant the
file lacks is compiled on the console once and added to it.

## Port menu

Hold **-** and **+** together (F8 on the PC). The game pauses its input while the menu is open.

| Setting | Choices | Takes effect |
| --- | --- | --- |
| FPS counter | Off (default), On: the frames shown on screen per second | at once |
| Stick dead zone | None (default, the game's own only), 10 %, 20 % for a drifting stick | at once |
| Handheld resolution | 960x540 (default, a steady 60 at stock clocks), 1296x729, 1440x810 (for overclocked consoles) | next start |
| Docked resolution | 1296x729 (default), 1440x810, 1920x1080 | next start |
| Handheld GPU | 460.8 MHz (default, Nintendo's own handheld profile, not an overclock), 384 MHz (saves battery) | next start |

**Save and close** writes the choices into `renut.toml`; **Close** keeps the live ones until the game is restarted.

## Known issues

- Not yet played start to finish. Please report anything that looks or plays wrong, with the logs (below).
- Docked mode has not been tested on this port yet.
- The homebrew menu shows the app's version as 0.1.0; the build is 0.5.0.
- The first time an effect appears that `deko3d_shaders.bin` does not hold, there can be a short stutter.
- Logs are written to `/switch/s4ep2-xbla-nx/logs/` on the SD card (`renut_N.log`, a new numbered file each launch). Send
  the whole folder with a report.

## Building

Windows (PC build and the Switch build): `tools/win/build_pc.ps1` and `tools/win/build_switch.ps1` (Visual Studio 2022
Build Tools, LLVM 20, CMake, Ninja, Python, Git, and devkitPro for the Switch). Code generation and shader packages:
see [docs/progress-log.md](docs/progress-log.md) and the notes in `docs/from-tfu/` and `docs/from-rr6/`.

## Layout

| Folder | Contents |
| --- | --- |
| `app/` | The game project: manifest, fixes (`overrides.toml`), the native renderer (`src/renut_engine`), Sonic 4's code (`src/s4`: frame pacing, port menu), settings (`s4_switch_release.toml`, `s4_switch_test.toml`) |
| `sdk/` | sa2-nx's fork of the ReXGlue SDK (0.10), with the Switch layer |
| `shaders/` | XenosRecomp, the shader translator |
| `tools/` | Package extraction, scanners, shader and variant cache tools; `tools/win/` the Windows build and test scripts |
| `docs/` | Progress log, pitfalls, scan of the game, upstreams |

## Legal

No affiliation with Sega, Sonic Team, Dimps, Microsoft or Nintendo. "Sonic the Hedgehog" is a trademark of Sega.
This repository contains no assets or program code from the original game. You must provide your own legally obtained
copy, and everything made from it (`assets/`, `app/generated/`, shader dumps and packages) is made on your own
computer. Running homebrew requires custom firmware, which violates Nintendo's terms of service.

Source code is provided under the GPL-3.0 License (see [LICENSE](LICENSE)), like the ports it builds on.

## Credits

- **Sega, Dimps and Sonic Team**: creators of Sonic the Hedgehog 4: Episode II. This is an unofficial, fan-made port.
- **[ChanseyIsTheBest](https://github.com/ChanseyIsTheBest/sa2-nx)**: sa2-nx and rr6-nx, the porting guide and tools.
- **[Tom Clay](https://github.com/rexglue/rexglue-sdk)**: the ReXGlue SDK, built on the work of the **[Xenia](https://xenia.jp)** team.
- **masterspike52**: reNut, the Direct3D-level renderer (through banjo-nx and sa2-nx).
- **[hedge-dev](https://github.com/hedge-dev/XenosRecomp)**: XenosRecomp, the shader translator.
- **devkitPro**: devkitA64, libnx and deko3d, and **fincs**' uam shader compiler.
