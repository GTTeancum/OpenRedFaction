# Executed source/generated face draw-state comparison — 2026-09-15

`tools/verify_geomod_crater_draw_state.py` executes original4f1432..4f15a3 in16 cases: flags0,0x100,0x108,0x300; valid or absent lightmap mapping; ordinary single/multitexture branch selection. Mapping container lookup is supplied; original mode initializer515730 and mode constructor411e00 execute. JSON records mode, diffuse RGBA and texture handles. Same RF.exe hash as earlier reports.

This extends the previously static4f0c00 inspection with executable evidence. It does not repeat or reinterpret the known noise32..95/MODULATE2X findings.

For all tested source/generated/detail flag variants, draw-state RGBA is **0xffffffff**, base texture is the face's material handle, and lightmap texture is mapping+0c/image+10 when mapping exists. A missing mapping selects handle-1. For a given branch, all flags yield the same render mode. No generated-face brightness multiplier, alpha reduction, alternate material, or density-class-dependent draw state appears in this tested block.

The distinguishing special branch is flag0x20000, which follows a separate material/object chain. Actual Holey01 template flags0x100 do not enter that branch. It was excluded rather than supplied with an invented material owner. Debug/alternate visualization globals are zero in these fixtures; their separate policies are not evidence of ordinary crater behavior.

Conclusion: this bounded draw setup does **not** establish a missing crater-specific gain to fix the dark appearance. It supports retaining common ordinary-face material/lightmap state while investigating actual map values, mapping identity and later dynamic-light updates. A valid mapping handle alone does not prove the correct texture contents or brightness. Full batching, vertex upload, graphics-driver state and original visual parity were not executed or inspected here.

## Primary live atlas audit

The retained verifier reruns all16 original draw-state cases successfully. `python tools/verify_geomod_base_seeds.py artifacts/destruction/debris/close-900.bin --name live-lightmap-review` also verifies every packed texel in121 retained maps (7407 texels) against the original CRT noise sequence, including continuous seed ownership. Packed RGB channels match each other and span levels4..11 of31, with mean7.543. No unexpected zero-filled rectangle was found. This includes retained maps, not only currently visible ones.

The audit rules out missing base-noise fill in this endpoint; it does not prove the original leaves those maps unchanged throughout gameplay. The RE agent is tracing the additional4f9d30 runtime relighting wrapper and caller dirty transitions. No brightness multiplier or visual parity correction is claimed. Current live density4 also agrees with Holey01's proven class1 scaling; remaining map grouping and later update ownership are still separate questions.

Artifacts: artifacts/destruction/live-lightmap-review.csv, .json and live-lightmap-review-values.json. This audit introduced no runtime behavior change.
