# Shallow GeoMod zero and negative depth handoff — 2026-09-15

`tools/verify_geomod_shallow_depth_handoff.py` executes eight full original45cff0 calls, followed by original4dbfb3..4dc03f normalization. Actual arithmetic helpers run unmodified; region container accessors are supplied. JSON retains authored depths, selected signed vectors, effective depths, normalized vectors and the enable byte. Same RF.exe SHA-256 as prior-cut report.

- Zero-depth regions still participate in original region selection/count rejection. Depth is not checked during selection; vector output becomes zero after multiplication by negative authored depth.
- **First limit zero disables all deformation**, even if second limit is nonzero:4dbfcc/4dbfd7 skip to4dc03f. Depths were cleared4dbfba/4dbfc4 and normalized scratch vectors were initialized to zero earlier.
- With first limit nonzero, a zero second limit skips only the second branch:4dc021->4dc03f leaves its normal zero, so the subsequent dot-positive gate cannot activate. It does not flatten points onto that plane.
- Negative authored depth is accepted by45cff0. The signed vector reverses, then4fabd0 at4dc004/4dc036 returns positive length and normalizes the reversed direction. Rejecting all negative depths is an additional port restriction, not original semantics.
- Example authored first depth -2 with region Up(0,1,0) yields vector(0,2,0), unit direction(0,1,0), effective depth2. Selection still used the original region Up for angular tests before depth multiplication.

Prior-cut alignment operates on signed depth-vectors before this handoff. Preserve their exact equivalent for history preparation; do not normalize away sign prematurely. The fixtures prove zero/negative values reaching runtime, not that the level editor normally emits them or all authored assets contain them.

Primary integration: rf_geomod_shallow_normalize converts signed selected vectors into effective cutter limits, preserving zero-first suppression and zero-second omission. All8 retained original/shared C comparisons pass; invalid-input rollback and in-place normalization guards pass. This conversion must run after prior-crater placement and is not yet connected to live authored regions.
