# Scan: Sonic the Hedgehog 4: Episode I (XBLA)

Scanned 2026-11-14 from Mark's dump.

## Package

- LIVE package, content type 0x000D0000 (Arcade title), title id 58410A07, version 1, display name "Sonic 4 Episode I".
- 19 files, 189.3 MiB: `default.xex` (2,236,416 bytes), `data.cpk` (195,555,960 bytes, all game data), `ArcadeInfo.xml`,
  `AvatarAwards`, dashboard images (12 achievement PNGs, `TitleIcon.png`, `MarketplaceBanner.png`, `kmrb_91.PNG`).
- Mark's second folder ("Addon", content type 0x00009000) is 8 avatar items (two tees, a hat, Chaos Emeralds, a Sonic
  plushie, Chopper and Moto Bug toys, a Sonic 4 cartridge). Not a title update; not needed.

## default.xex

- SHA-1 `5aab60d774ceb828fca656b5b8457c6d010909cd`, SHA-256 `0a2525018d23e908cdcefd779e350d49f632f5773baf248108435cc8dac00a7b`.
- XEX2, encrypted (retail key), normal (LZX) compression, window 0x8000. Image 0x5D0000 bytes (about 6 MB) at 0x82000000,
  entry 0x82223690. Unpacked with `tools/xex_unpack.py` (libmspack from the SDK).
- Imports: xam.xex and xboxkrnl.exe 2.0.11626 (86 and 289 records).
- Static libraries, all **XDK 2.0.11626**: XAPILIB, D3DX9, XGRAPHC, XBOXKRNL, XNET, LIBCPMT, XAUDIO2, XAPOBA, XMCORE,
  XONLINE, XMP, **D3D9LTCG**. Compiler: MSC 16.0.10224 (VS 2010 era).
- 11626 is the same XDK build as Castlevania: Lords of Shadow (LoS1-nx), whose Direct3D function addresses and device
  layout are fully mapped.

## Middleware and engine (from the image's strings)

- **CRI** (all built 2010-08-05 for xdk11626): CRI File System 2.21.17 (CriFs2, reads `d:/data.cpk`, CRILAYLA-compressed
  entries, 427 of them), CRI Audio 3.52.03 (the sound engine: cue sheets `SOUND/SND_FX.CSB`, `SOUND/SONICDL_SNG01.CSB`
  with a music `SONICDL_SNG01.CPK`; HCA, ADX and AHX decoders; its Sound Renderer outputs through XAudio2), CRI Heap,
  Thread, Timer, Critical Section, LLDECOMP3. No Sofdec/USM movie strings found.
- **Sega's NN library** assets: about 430 `.AMB` archives named in the code (`DEMO/...`, `G_ZONE1..F/...`), including
  `NNSTDSHADER/SHADER.AMB`.
- **The HLSL shader compiler is linked in** (D3DX9's compiler and the Xenos microcode compiler's strings: `xvs_3_0`,
  `xps_3_0`, `main_VS`, `main_PS`, "Microcode validation failed"). Theory, not yet confirmed: the game compiles its
  shaders at run time from HLSL in `NNSTDSHADER/SHADER.AMB` instead of shipping microcode. To check: unpack that file from
  data.cpk and look for HLSL source, then confirm in the first PC run (the compiler's functions are called at boot).
- Online: XOnline marketplace and leaderboard strings (leaderboards and achievements will be local or stubbed).
