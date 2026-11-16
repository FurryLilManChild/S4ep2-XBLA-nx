# Progress log

Newest first.

## 2026-11-16 (morning): first codegen, sweeps, Direct3D match against Episode I

- Codegen pass 1: one UnresolvedCall, an 8-byte tail-call thunk at 0x8242D428 (declared). Pass 2: 23,629 functions,
  128 files, 131 MB (Episode I: 12,440). One warning left: "Unresolved b target 0x821F3A10 from 0x821F39BC".
- Pointer sweep (`tools/pointer_fixpoint.py`): pass 1 accepted 88 of 106 (14 no code, 2 fragments, 2 funclets), pass 2
  none. `override_labels_check.py --code 82130000:828DFD3C` flagged 1 of 89 (0x82466DCC, an SEH fall-through):
  removed. `switch_check.py`: truncated 0, wrong targets 0, missed 0; six odd shapes to watch at boot (0x822E6460,
  0x822E77B0, 0x823992A0, 0x823992C8, 0x8264F3B8, 0x826511D8).
- `d3d_match.py` with Episode I as the reference (its 314 hook addresses as names, `docs/d3d/`): of 302 that are
  function starts, 277 match at 0.97 to 1.00, 15 at 0.85 to 0.97, 10 below (Swap, Resolve, BeginTiling, Release,
  Resource_Destroy, UnsetAll and a few render-state accessors: to check by hand). Despite the newer XDK the Direct3D
  code is nearly the same; the device's field offsets still need checking. The frame pacer step (Episode I
  0x82488958) matches 0x828604B8 at 0.94; the Present wrapper (0x8249E2F8) only 0.63 (to find by hand).

## 2026-11-16: kickoff

- Repo made from S4ep1-nx 0.5.0's tree (commit 7fa43d1c, `docs/upstream.md`). Episode I's notes moved to
  `docs/from-s4ep1/`.
- Extracted Mark's package to `assets\extracted\` with `tools/stfs_extract.py` (run on his machine).
- Scanned default.xex (`docs/scan.md`): XDK 2.0.20871, not Episode I's 11626, so the Direct3D map has to be made
  again; same CRI and NN engine family; image twice Episode I's size.
- Episode METAL: the 360 checks the Episode I license ("Check Ep1 License", referenced at 0x823778A8).
