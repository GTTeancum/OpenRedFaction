# Final independent review: split producer and durable-gated consumer

2026-10-11 03:25 UTC. This supersedes the runner revision in IMPLEMENTATION-REVIEW.md. Static inspection only; no tests, imports, syntax execution, builds, runtime, images or cleanup.

APPROVE FOR PARENT'S SCHEDULED BOUNDED VALIDATION.

Final inspected runner: /workspace/shared/natural-save-validation-next/tools/xemu_natural_save_gameplay.py
SHA256 326c6f371e57e6567b8635e48f0401e8bbee7e418ab729d04235f79644171932

Unchanged helper SHA256 585626a0d3c3502e666c04b6641e659ae703353511589ace1e80fd617941626e.
Unchanged helper patch SHA256 209d3900e35496ff251b3481bd00a92c106a259ceed861fb1803d918444e4dcf.
RUNBOOK.md documents the exact invocations and parent receipt contract.

## Changes verified

- Exact baseline SHA256 b5df1ad52a7814e14dbcf5d516c4267f654eb5bacd64970642c0ba03a75b7485 is asserted before disc staging in either phase. Producer copy must match this exact hash. Base remains read-only input, distinct from missing old SHA129855.
- --phase is mandatory. Producer accepts no consumer-resume arguments. Successful producer ends with PASS_NEW_NATIVE_SAVE_AWAITING_DURABLE_BACKUP and consumer blocker PARENT_VERIFIED_DURABLE_BACKUP_REQUIRED. It creates two read-only local copies only; metadata explicitly says durable_off_workspace=false. No consumer is launched in this phase.
- Producer final restoration and disk-protection checks still control status. Same bounded180-frame neutral save request and strict clean-shutdown/native-flush evidence remain.
- Separate consumer admission checks producer final status, no restoration errors, nonempty successful producer checks, matching clean current commit/XBE/map, original archive equality, NEW_L3S1 provenance, expected absolute producer/backup paths, both read-only disk sizes/hashes, exact stored strict-shutdown receipt, actual payload decode/hash and generation/slot/length consistency.
- Consumer additionally requires parent-verified durable receipt kind RF_NATURAL_SAVE_DURABLE_BACKUP_V1, verified=true, matching raw HDD hash/size and producer-immutable manifest hash, plus actual Library-ID/version/hash artifact metadata. The runner explicitly cannot authenticate remote objects; parent must verify those actual remote uploads/readback before setting the receipt boolean. A local duplicate cannot satisfy that operational requirement.
- Consumer operates a distinct byte-identical copied disk with snapshot=True. An exclusive consumer-attempt.json in producer evidence reserves the sole consumer launch across different output folders. It remains on failure and is never removed automatically. Frozen producer and backup bytes/modes are verified afterward.
- Parent can preserve chunked remote backup, but must ensure actual artifact coverage, ordering, reconstruction instructions and raw-disk reconstruction hash; receipt schema alone does not prove these. Do not claim durable protection merely from a parseable JSON receipt.
- Pickup checks now use collected/rounds lanes3/4 and status7, requiring zero. This matches scene.c rf_scene_pickups declaration. Dynamic checks/blocked/draw lanes are no longer incorrectly compared over simulation time.
- PLAN wording no longer attributes helper implementation to reviewer or claims saved pose/health equality from inventory-only checks.

## Scope and outcome limits

Still one genuine producer and one separately gated consumer, with parent exclusively owning runtime. Save refusal, missing observer window, ambiguous close, failed durable upload or failed consumer are honest blockers, not permission for frame-search retries, grants, forced events, fabricated saves or broad campaign exploration. Load, finite observed movement and actual single-shot/ammo outcomes are separately evidenced. The missing historical L11S3 regression is not recovered by this new L3S1 save.

No static blocker remains in the reviewed final hashes. Runtime success and durable recovery remain unverified until parent performs the actual operations.

## Final movement-evidence correction (03:27 UTC)

Final approved runner SHA256 is now 3641c3d8cae7f53920cb97c23a49c6aa3ff12460d489355f4a8218823765b934, superseding326c6f37 above. Inspected the requested correction: movement acceptance requires squared horizontal X/Z displacement>.0001 plus matching tagged move inputs. Pure vertical gravity settling can no longer satisfy movement_observed. Full XYZ positions/full displacement remain reported, with horizontal_displacement_squared added. Strict helper/patch hashes are unchanged. Static approval stands; no tests/runtime executed.

## Final attempt-count correction (03:27 UTC)

Frozen approved runner SHA256 6e89c5599f354e52db7675213889dd9be6cd128aaee33fe999e87fcdb4eaa14c supersedes3641c3d8. Read-only inspection confirms each selected phase now sets both its phase attempt count and shared attempts count to1 before launch. This removes misleading inherited attempts=0 metadata without adding any retry or runtime action. Static approval stands; unchanged helper/patch hashes.
