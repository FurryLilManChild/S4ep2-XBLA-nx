# S4ep1-nx 0.5.0

The first release of the native Switch port of Sonic the Hedgehog 4: Episode I (Xbox 360 XBLA). Feature complete,
needs a full playthrough.

## What's in it

- The whole game recompiled from the Xbox 360 `default.xex` and drawn by a native deko3d renderer: no emulation.
- A steady 60 fps at stock clocks: drawn at 960x540 in handheld and 1296x729 docked, with Nintendo's own 460.8 MHz
  handheld GPU profile (not an overclock). The game keeps its own speed: no slow motion and no speed-ups in heavy
  moments.
- A precompiled shader file (`deko3d_shaders.bin`) with the shader variants seen in testing, so effects do not stutter
  the first time they appear.
- A port menu: hold - and + together. FPS counter, handheld and docked resolution, handheld GPU profile, stick dead
  zone; Save and close keeps the choices in `renut.toml`.
- The title screen's sky, clouds and islands, special stages, and the game's save prompt drawn large enough to read.

## Installing

1. Copy the zip's `switch` folder to the root of the SD card: `/switch/s4ep1-nx/` gets `s4ep1.nro`, `renut.toml` and
   `deko3d_shaders.bin`.
2. Make `/switch/s4ep1-nx/game_root/` and put `default.xex` and `data.cpk` from your own copy of the XBLA game in it
   (extracted from its package; `default.xex` SHA-256
   `0a2525018d23e908cdcefd779e350d49f632f5773baf248108435cc8dac00a7b`).
3. Start the homebrew menu through title takeover (hold R while opening a game), not the Album, and run `s4ep1.nro`.

No game files are included.

## Known issues

- Not yet played start to finish: please report anything that looks or plays wrong.
- Docked mode has had less testing than handheld.
- An effect the shader file lacks can stutter briefly the first time it appears.
- Logs: `/switch/s4ep1-nx/logs/`. Send the whole folder with a report.
