# S4ep2-XBLA-nx 0.5.0

The first release of the native Switch port of Sonic the Hedgehog 4: Episode II (Xbox 360 XBLA). Feature complete,
needs a full playthrough.

## What's in it

- The whole game recompiled from the Xbox 360 `default.xex` and drawn by a native deko3d renderer: no emulation.
- A steady 60 fps at stock clocks in handheld: drawn at 960x540 and scaled to the screen, with Nintendo's own 460.8 MHz
  handheld GPU profile (not an overclock). The game keeps its own speed.
- Shaders prepared before the game starts: a short "Preparing shaders" screen at launch reads every shader variant
  seen in testing from `deko3d_shaders.bin`, so effects do not stutter the first time they appear in play.
- Episode METAL without Episode I: the 360 version unlocks it by checking for an Episode I license; the port answers
  that check itself (`s4_episode_metal`, on by default).
- A port menu: hold - and + together. FPS counter, handheld and docked resolution, handheld GPU profile, stick dead
  zone; Save and close keeps the choices in `renut.toml`.

## Installing

1. Copy the zip's `switch` folder to the root of the SD card: `/switch/s4ep2-xbla-nx/` gets `s4ep2.nro`, `renut.toml`,
   `deko3d_shaders.bin` and `deko3d_variants.txt`.
2. Make `/switch/s4ep2-xbla-nx/game_root/` and put `default.xex` and `data.cpk` from your own copy of the XBLA game in
   it (extracted from its package; `default.xex` SHA-256
   `30583685039e56080bb69d0856a9a938862ea05340a3e0faa9dd96eb424d8c90`).
3. Start the homebrew menu through title takeover (hold R while opening a game), not the Album, and run `s4ep2.nro`.

No game files are included.

## Known issues

- Not yet played start to finish: please report anything that looks or plays wrong.
- Docked mode has not been tested on this port yet.
- An effect the shader file lacks compiles the first time it appears and can stutter briefly.
- The homebrew menu shows the app's version as 0.1.0; the build is 0.5.0.
- Logs: `/switch/s4ep2-xbla-nx/logs/`. Send the whole folder with a report.
