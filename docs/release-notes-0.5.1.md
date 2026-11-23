# S4ep2-XBLA-nx 0.5.1

Fixes for the problems reported on 0.5.0 (GitHub issue 1). Still needs a full playthrough.

## Fixed

- **Death Egg Mk. II no longer crashes** when the zone loads.
- **Special stages:** the pause screen and the results screen no longer show garbage over the picture.
- **The sun:** the title screen and Sylvania Castle no longer have a yellow-orange haze over everything, and in Sky
  Fortress (Act 3 and the Metal Sonic fight) the sun and its flare no longer shine through the level. The game hides its
  sun glow behind the scene with a trick the port now handles the way the Xbox 360 does.
- **Port menu resolution** (hold - and +): the choices now name the resolution actually drawn, and a change applies at
  once, with no restart. Handheld: 640x360 (default), 960x540, 1280x720. Docked: 960x540, 1280x720 (default),
  1920x1080. Correction to the 0.5.0 notes: 0.5.0 drew handheld at 640x360, not 960x540.
- **More shaders prepared up front:** `deko3d_shaders.bin` now holds 2,860 shader variants (2,524 in 0.5.0), so fewer
  effects stutter the first time they appear.
- The homebrew menu shows the right version.

## New

- **Capture this frame** in the port menu: 3 seconds after the menu closes, the port saves everything about one frame
  (every drawing step's pictures and settings) to `/switch/s4ep2-xbla-nx/captures/`. If something looks wrong on screen,
  a capture of it with your logs is the most useful bug report there is. Wait about a minute after a capture before
  quitting, so it finishes writing. Each one takes a few hundred MB on the SD card.

## Installing

1. Copy the zip's `switch` folder to the root of the SD card: `/switch/s4ep2-xbla-nx/` gets `s4ep2.nro`, `renut.toml`,
   `deko3d_shaders.bin` and `deko3d_variants.txt`. Replace all four if you had 0.5.0 (the new `renut.toml` resets the
   port menu's choices to the defaults).
2. If this is a new install: make `/switch/s4ep2-xbla-nx/game_root/` and put `default.xex` and `data.cpk` from your own
   copy of the XBLA game in it (extracted from its package; `default.xex` SHA-256
   `30583685039e56080bb69d0856a9a938862ea05340a3e0faa9dd96eb424d8c90`).
3. Start the homebrew menu through title takeover (hold R while opening a game), not the Album, and run `s4ep2.nro`.

No game files are included.

## Known issues

- **Waterfalls** in Sylvania Castle still look blotchy. Next on the list.
- Special stage reflections were reported as sometimes wrong on 0.5.0 and have not been checked since.
- An effect the shader file lacks compiles the first time it appears and can stutter briefly.
- Logs: `/switch/s4ep2-xbla-nx/logs/`. Send the whole folder with a report.
