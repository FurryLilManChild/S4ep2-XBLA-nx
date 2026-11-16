# Scan: Sonic the Hedgehog 4: Episode II (XBLA)

Scanned 2026-11-16 from Mark's dump.

## Package

- LIVE package, content type 0x000D0000 (Arcade title), title id 584111F2, "Sonic 4 Episode II", 105,414 blocks.
- 18 files, 411.7 MiB: `default.xex` (4,116,480 bytes), `data.cpk` (426,836,000 bytes, all game data), `ArcadeInfo.xml`
  (projectVersion 1.0.111.0), `AvatarAwards`, 12 achievement PNGs, `GameTile.png`, `MarketplaceBanner.png`.
- The second folder ("Addon", content type 0x00009000) is avatar items, as for Episode I: not needed.

## default.xex

- SHA-1 `2fa6e222ac51716ca460efa754032d9366d822c6`, SHA-256
  `30583685039e56080bb69d0856a9a938862ea05340a3e0faa9dd96eb424d8c90`.
- XEX2, encrypted (retail key), normal (LZX) compression, window 0x8000. Image 0xB10000 bytes (about 11 MB, twice
  Episode I's) at 0x82000000, entry 0x8245A030. Unpacked with `tools/xex_unpack.py`.
- Static libraries, **XDK 2.0.20871** (Episode I: 2.0.11626): XAPILIB, D3DX9, XGRAPHC, XBOXKRNL, XNET, XAUDIO2,
  XAPOBA, XMCORE, XONLINE, XMP, QNETXA2, XRNSS, XRNMS, XHV2 (voice), XCAM, XMEDIA2, XPARTY, **D3D9LTCG** (Direct3D
  linked in with link-time code generation, as in Episode I: calls take game-specific argument lists). LIBCPMT, LINK,
  C1/C2 2.0.11886 / 16.0.11886 (VS 2010).
- **The biggest difference from Episode I**: a different XDK build, so Episode I's Direct3D addresses and the device
  layout do not carry over. Mirror of Fate HD (mof-nx) is XDK 2.0.21256, the nearest mapped build.

## Middleware and engine (from the image's strings)

- **CRI**, all built 2011-09-29 for XTL20871: File System 2.28.00 (`data.cpk`), Audio 3.57.12, Sound Renderer 1.05.00,
  ADX and AHX decoders, CPK Analyzer, Stream Joint, LLDECOMP3. The same family as Episode I, a year newer.
- **Sega's NN library** `.AMB` archives as in Episode I: `DEMO/...`, `G_ZONE1..4`, `G_ZONEF`, and `G_EP1ZONE1..4`
  (Episode METAL's stages, Episode I's zones replayed as Metal Sonic), `CUTSCENE/SCENE01..`.
- Online and multiplayer strings (`DEMO/MULTIPLAY`, XONLINE, QNETXA2, XHV2): co-op over Xbox Live, not expected to
  work offline.

## Episode METAL

- Strings: "Episode METAL", "EpisodeMetal2..4", `DEMO/UNLOCK/D_UNLOCK_*.AMB`, "DmEmUnlockMessage", "EvEmUnlockMessage",
  and **"Check Ep1 License"** (at 0x821059B8, referenced once, at 0x823778A8).
- On the 360 Episode METAL unlocks when Episode I is owned. The string points at a license check: the port's fix is to
  make that check answer "owned" (or set the unlock flag), to be found from the code around 0x823778A8 after codegen.
