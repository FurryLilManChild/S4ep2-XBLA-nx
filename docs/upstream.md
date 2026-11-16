# Upstreams

S4ep2-XBLA-nx (2026-11-16): the whole tree copied from FurryLilManChild/S4ep1-nx at `7fa43d1cce06edd094fd192c6304a42e09ea2a73` (v0.5.0), GPL-3.0. The table below is S4ep1-nx's own.

| Upstream | Commit | License | What we take |
|---|---|---|---|
| FurryLilManChild/TFU-nx | `756609ec17a0a792b83931dd7df3204a701455ef` (2026-11-14, build 5) | GPL-3.0 | the whole tree as the base: `sdk/` (ReXGlue fork with the Switch layer), `app/` (Renut, glue, CMake), `shaders/` (XenosRecomp), `tools/`. TFU-nx is itself rr6-nx 1.0.1 (`docs/from-rr6/`). TFU's own notes are in `docs/from-tfu/`. |
| FurryLilManChild/LoS1-nx | `9c70bb5` (v0.5.0) | ours | the XDK 2.0.11626 Direct3D map (`docs/d3d-map.md` there), the same XDK build as Sonic 4. Later: port menu, stock clocks. |
| ChanseyIsTheBest/rr6-nx | `cd5133f5b75b37d6e7ec72990f902eb5cc71303b` (v1.0.1) | GPL-3.0 (own code), others per `THIRD_PARTY_NOTICES.md` | through TFU-nx |

Copied 2026-11-14 as files, not history. TFU-specific files (`app/src/tfu`, `tools/tfu_*`, `app/tfu_switch_test.toml`)
and RR6-specific files stay for now as references and are replaced or removed as Sonic 4's own code is built.

S4ep1-nx is GPL-3.0 as a result (Mark's decision, 2026-11-14).
