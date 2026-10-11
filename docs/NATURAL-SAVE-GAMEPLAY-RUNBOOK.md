# Parent integration and execution

Source-only stage. No test, build, AST/import check, emulator or HDD operation has been executed. The new files require independent source review and the parent's scheduled 04:00 UTC batch.

Integrate only:
- strict-clean-shutdown.patch (existing tools/xemu_native_world_save.py; default old behavior unchanged)
- tools/xemu_natural_save_gameplay.py (new parent-only runner)

Commit before building; the runner requires a clean current source tree and PASS_XBOX_BUILD manifest matching exact commit/XBE/map hashes. It imports the installed repository helper, so do not invoke the new runner against an unpatched repository.

## Producer phase

Inside the normal serial parent batch after the one Xbox build, use:

    python3 tools/xemu_natural_save_gameplay.py --parent-hourly-batch --phase producer --root /workspace/shared/OpenRedFaction-attack --consumer-build /workspace/shared/rf-20261011-0400-build.json --out /workspace/shared/rf-natural-save-producer-20261011-0400 --seconds 600

The output folder must not exist. The normal parent batch lock plus the harness SessionLock prohibit concurrent project XEMU. One producer attempt only. Expected success status: PASS_NEW_NATIVE_SAVE_AWAITING_DURABLE_BACKUP. This means a genuine new save has been created, not that load or controls passed. Guard/observer/flush/clean-exit failure stops without consumer admission. Preserve failure evidence. The two local copies are read-only and hashed; neither survives a workspace reset by itself.

## Durable gate, owned by parent

After producer final verification succeeds, preserve new-l3s1-producer.qcow2 or the identical new-l3s1-immutable-backup.qcow2 through the supported Library workflow. Chunked archives are allowed when necessary, but preserve exact reconstructable bytes, chunk ordering and checksums in the uploaded metadata. Keep the source build, tested XBE/map, recipe/input, native result/payload, shutdown receipt, owner and producer immutable manifest with the backup. Verify actual returned Library IDs/versions and upload/readback evidence. Never invent a successful receipt or call a local copy durable.

Then atomically publish a complete receipt outside the producer folder (write a temporary file and rename after closing), containing:
- kind: RF_NATURAL_SAVE_DURABLE_BACKUP_V1
- remote_upload_verified_by_parent: true only after actual verification
- producer_hdd_sha256: exact successful producer disk SHA256
- producer_hdd_bytes: exact disk byte count
- producer_manifest_sha256: SHA256 of producer-immutable.json
- artifacts: nonempty list of each actual uploaded file, with library_file_id (returned libfile_ ID), version (returned integer), and sha256 (that artifact's file hash)

For chunked backup the artifact SHA256s differ from the raw disk hash; the raw disk hash/size remain separate mandatory fields. Preserve extraction/reassembly instructions with the remote artifacts. The harness checks the receipt matches the local producer, but cannot independently authenticate remote artifacts. Parent owns this verification.

If backup is blocked, retain the verified producer, report save-only progress and hold the consumer; no synthetic fixture or bypass.

## Separate consumer phase

Only with the exact same clean source/XBE/map and original assets:

    python3 tools/xemu_natural_save_gameplay.py --parent-hourly-batch --phase consumer --root /workspace/shared/OpenRedFaction-attack --consumer-build /workspace/shared/rf-20261011-0400-build.json --producer-dir /workspace/shared/rf-natural-save-producer-20261011-0400 --durable-backup-receipt /workspace/shared/rf-natural-save-durable-backup-receipt.json --out /workspace/shared/rf-natural-save-consumer-20261011-0400 --seconds 600

Consumer uses one hash-matched independent snapshot HDD. An exclusive consumer-attempt.json on the producer evidence reserves the sole attempt immediately before launch; no automatic retries or deleting that marker. Final status PASS_NEW_NATIVE_SAVE_LOAD_AND_BOUNDED_CONTROLS requires fresh native load plus actual movement and single-shot/ammo checks. A blocked move or missing shot is reported separately from native load success. Movement requires horizontal X/Z displacement plus exact tagged forward inputs; vertical-only gravity displacement cannot pass, while full XYZ remains recorded. The consumer deliberately contains no save request.

Timing: neutral frames0..119, forward input120..134, neutral135..179, primary180, neutral181..239. Pre-input observation is requested at30 and must be sampled before120; post-move observation is requested140 and must be sampled before180. Missing a window fails its evidence; no substituted terminal owner or invented sample.

Actual pickup counters3/4 and status7 establish no collection. Do not require pickup check/blocked counters1/2 or draw count6 to stay equal; those evolve during ordinary simulation.

No image/audio/performance/old-save-recovery claim. Parent retains normal repository artifact cleanup and durable evidence delivery responsibilities.

## Reviewed integration

Integrated after final split-runner review4547cd89. Runner SHA2566e89c5599f354e52db7675213889dd9be6cd128aaee33fe999e87fcdb4eaa14c; native helper585626a0d3c3502e666c04b6641e659ae703353511589ace1e80fd617941626e. Durable source/review packet libfile_5ddda2d8adf48191a81adf7c580a59d1 version0. Source-only review; actual native save/load and movement/fire remain unexecuted.

## Additive terminal observations

Final runner SHA256 c73e90d04351c0df4dd98ff41a6a2853f449e4aa28acf48869ac2e8c51048fd0 supersedes the initial runner above. It retains the established startup terminal observations through an immutable union with the natural-save symbols. Exact-address preflight and both phase captures cover the union. Inputs, acceptance, timing, save/restore and live probes are unchanged. Independent review is NATURAL-SAVE-OBSERVER-REVIEW.md; durable packet libfile_3555c9f1840881918e859378ff50bff9 version0. No execution yet.
