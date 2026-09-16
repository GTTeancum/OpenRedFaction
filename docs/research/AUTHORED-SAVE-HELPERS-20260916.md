# Authored persistence integration checkpoint

Live source capture retains a 9,344-byte trusted resource manifest, including the original generated rock02 substrate from ui.vpp. No original pixel pointers survive capture. The aggregate source SHA stays unchanged. Archive borrowing ends before gameplay.

`artifacts/xemu/render-20260916-125251/report.json` passes 57 checks over 840 frames of reset/re-cut on stock64MiB, with4186 free pages at endpoint. Additional identity capture peak is1,890,119 bytes, below2MiB. Native framebuffer inspection confirms the post environment, floor, water, launcher and HUD. This is not full visual parity or authored checkpoint restart acceptance. Earlier failed runs124742 (missing UI scope) and125139 (NXDK empty-body warning) are retained; both causes were fixed before the passing run.

Twelve focused PC tests pass: authored_checkpoint_layout, geomod_publication_digest, geomod_collision_digest, geomod_retained_material_digest, composed_checkpoint_codec, scene_terrain_lighting_stage, authored_identity_manifest, scene_publication_candidate, scene_terrain_empty_stage, scene_authored_journal_import, scene_authored_digest_capture, and scene_authored_admission_import.

The RFDS2 parser bounds all spans before RFCP accepts them. Candidate publication can consume a privately decoded core with an independent saved revision while preserving live ownership. Lighting supports both nonempty journal reconstruction and empty reset staging; admission import preserves requested centers and the next-cut RNG. The three-digest adapter validates actual full-room collision, publication, and all retained material history including unused maps. These scene adapters remain prerequisites: the live RFDS2 writer/restore dispatch and complete player/clutter scope are not yet wired.

Unchanged compiled collision materials are indexed within the trusted complete compiled source identity; their animated pixels are not collision geometry. Published/source/authored resource identities use canonical content digests. Capture source tables remain separate from runtime renderer slots.

`rf_geomod_uv_lineage_probe` is deliberately not registered as a passing CTest. Running actual shared splitting and final mapping reproduces12 altered inherited corners. The exact original correction requires chronological reconstruction with transient new-face lineage, not indiscriminate reprojection. No UV production fix is included here.

## Solo integration verification

The PC executable builds with the pending authored scope, source identity, digest,
admission, journal, private restore staging and writer includes. Five focused
CTest targets pass: composed_checkpoint_codec, scene_authored_journal_import,
scene_authored_digest_capture, scene_authored_admission_import and
scene_authored_clutter_scope. Existing compiler warnings remain.

The installed-assets digest fixture now invokes the real RFDS2 writer and private
restore stage after one cut, two cuts (including an unused material-history map),
and an empty reset retaining RNG1. It compares all three reconstructed digests,
checks candidate disposal, rejects a corrupted publication digest without replacing
the live core/serial or leaving a pending publication, and verifies undersized
writer output remains unchanged. The fixture now supplies the actual published
generation as the scene serial; direct core cuts start at generation2, unlike
assuming the cut count is the publication serial.

The clutter scope test uses506 actual authored records but synthetic resident
resources; it validates unchanged bookkeeping and rejects semantic mutations or
stale registry/list/model state. This is not a live clutter-loader acceptance.
No live player save dispatch, final restore publication, process restart, or new
Xbox acceptance is claimed. Native baseline remains render-20260916-125251.

## Live PC authored restart

The player dispatch now selects RFCP profile2 for the authored test room, checks
its captured clutter baseline, tests player placement against full composed room
collision, and uses private RFDS2 reconstruction before final terrain publication.
The commit transfers core, admissions, RNG, serial and atlas reservation only after
publication succeeds. The installed CPU fixture commits and re-saves identical
bytes after one/two cuts, rejects a second commit, and commits a790-row empty reset.

artifacts/authored-post-live/solo-live-save.log records the840-frame reset-recut
process producing a2518-byte RFCP (1942-byte RFDS2). A separate32-frame idle
process in solo-live-restored.log loads and re-saves byte-identical output:
solo-live-save.rfcp equals solo-live-restored.rfcp. Both processes exit0.
This proves this settled player's pose/inventory and destruction checkpoint
round-trip; no visual inspection, next-blast continuation or Xbox restart is
claimed. Profile telemetry was corrected from the legacy1 to dynamic2 afterward;
that final telemetry-only edit remains to rebuild. The previous five-test pass
predates live dispatch; the expanded digest/commit test was rerun and passes.

## PC/Xbox next-blast continuation acceptance

Run tools/check_authored_restart.py to split the existing two-shot recording at
frame350: save after the first cut, restart for the final200frames, and compare
against550 uninterrupted frames. The report at
artifacts/authored-post-live/solo-continuation/report.json passes: full3884-byte
RFCP,2508-byte RGCH and1376-byte publication are byte-identical. Explicit cutter
counts prove one saved cut and two final cuts. This does not claim full saves.

Native render-20260916-184610 loads that PC-authored checkpoint and executes the
same200-frame second-blast continuation. All58 checks pass on64MiB;4147 free pages
(16.19921875MiB) remain. Exported Xbox checkpoint equals restarted PC and
uninterrupted PC exactly (SHA256
5248b926a1955da39eb1d2ca840840e8403b59603f3b8f38773cfd1c8263a8f0).
PC and native endpoint images were inspected: damaged post, room structure,
floor/water, launcher and HUD present. No audio or full visual-parity claim.
Disc restoration is confirmed; the runner completed and closed its owned emulator.
Final profile telemetry2 is included in this build. Five focused tests were rebuilt
and rerun successfully. Live reset/malformed and broader authored cases remain.

## Reset continuation acceptance

`python tools/check_authored_restart.py --case reset-zero` saves at frame640
of reset-recut.bin (zero cuts, revision3), then restarts for the last200 frames.
PC checkpoint, cutter history and publication exactly match uninterrupted840frames.
Native render-20260916-184934 passes58 checks with16.51171875MiB free on64MiB.
Xbox, restarted PC and uninterrupted PC checkpoints are identical2518bytes.
Native framebuffer inspected: room, recut post, floor/water, launcher and HUD
present. Audio unverified; no visual-parity claim. Disc restoration confirmed
and owned emulator exited. No code changes were needed for the reset path.
