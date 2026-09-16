# Authored persistence integration checkpoint

Live source capture retains a 9,344-byte trusted resource manifest, including the original generated rock02 substrate from ui.vpp. No original pixel pointers survive capture. The aggregate source SHA stays unchanged. Archive borrowing ends before gameplay.

`artifacts/xemu/render-20260916-125251/report.json` passes 57 checks over 840 frames of reset/re-cut on stock64MiB, with4186 free pages at endpoint. Additional identity capture peak is1,890,119 bytes, below2MiB. Native framebuffer inspection confirms the post environment, floor, water, launcher and HUD. This is not full visual parity or authored checkpoint restart acceptance. Earlier failed runs124742 (missing UI scope) and125139 (NXDK empty-body warning) are retained; both causes were fixed before the passing run.

Twelve focused PC tests pass: authored_checkpoint_layout, geomod_publication_digest, geomod_collision_digest, geomod_retained_material_digest, composed_checkpoint_codec, scene_terrain_lighting_stage, authored_identity_manifest, scene_publication_candidate, scene_terrain_empty_stage, scene_authored_journal_import, scene_authored_digest_capture, and scene_authored_admission_import.

The RFDS2 parser bounds all spans before RFCP accepts them. Candidate publication can consume a privately decoded core with an independent saved revision while preserving live ownership. Lighting supports both nonempty journal reconstruction and empty reset staging; admission import preserves requested centers and the next-cut RNG. The three-digest adapter validates actual full-room collision, publication, and all retained material history including unused maps. These scene adapters remain prerequisites: the live RFDS2 writer/restore dispatch and complete player/clutter scope are not yet wired.

Unchanged compiled collision materials are indexed within the trusted complete compiled source identity; their animated pixels are not collision geometry. Published/source/authored resource identities use canonical content digests. Capture source tables remain separate from runtime renderer slots.

`rf_geomod_uv_lineage_probe` is deliberately not registered as a passing CTest. Running actual shared splitting and final mapping reproduces12 altered inherited corners. The exact original correction requires chronological reconstruction with transient new-face lineage, not indiscriminate reprojection. No UV production fix is included here.
