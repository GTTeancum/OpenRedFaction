# Ordinary NPC reload: exact joint source review

Review date: 2026-10-11 UTC.

## Disposition

The independent complete review accepts the complete bounded `guard1` / ordinary `12mm handgun` runtime and transaction implementation composed with the separately accepted RFNC27 codec. It reports no blocking source defect in that exact union. Earlier codec and interface reports describe their then-pending whole-feature review; the later complete review closes that source-review gate.

This records source acceptance only. Compilation, runtime behavior, target ABI/stack/stock-memory fit and protected current-save/consumer compatibility remain unverified. Source acceptance is not approval to activate the format, replace a protected consumer or integrate only part of the feature. See [the feature contract](NPC-ORDINARY-RELOAD.md) for supported behavior and limits.

## Frozen identity

The accepted source is the exact 22-file `guard-reload-joint-next` union, against gameplay baseline `0674ed0491d5fca2ead42672f5dabbe69683d709`:

- Joint `SOURCE-SHA256SUMS`: `41fc5bec976e2cd628f6c82c50c5e4a3095b463b53323acb1ca7bcc8b22b607a`.
- Joint `joint.patch`: `4083ab2b47a148a1839aa85fc36aaede5c51e12fb857b665d709de68134ced52`.
- Complete independent `guard-reload-runtime-review-next/REVIEW.md`: `27780e59303b93f9a6ee0592b7d018820e29405126b99b47d8c13df00ed683cb`.

The complete review verified all 22 source hashes, direct byte equality with the reviewed 20 runtime files and two codec files, and exact file-set equality with no additional source file. The source manifest and patch hashes were also rechecked while preparing these integration documents. Integrity checks identify the reviewed bytes; they are not compilation or behavioral tests.

The durable final review packet is Library item `libfile_f68716ce38f88191bdefe245d97da197`, version 0, with archive SHA256 `34fe393ec02d9b9df72e9c9120d618ecbc8f8e2240fcf113c728376bd6cb74f9`. These are the preserved packet identifiers supplied for integration provenance; this documentation pass did not re-download or re-hash that archive.

Supporting source-review records:

- Codec review, `guard-reload-codec-review-next/CODEC-REVIEW.md`: `e81999a96ff0dfa9f97771a62f493ce600ed4146c696dd48956ddd5111830aae`.
- Runtime interface/budget review, `guard-reload-codec-review-next/RUNTIME-INTERFACE-REVIEW.md`: `f179e84d6c7cf43af2f4daf3cafd05ceabde1a662da346ec15068df99651fcf5`.
- Runtime implementation record, `guard-reload-runtime-implementation-next/IMPLEMENTATION.md`: `3c9683315c34f85d85e3313b4ea97c141e339009f23a19a1d6bd245d8f042a46`.
- Runtime budget ledger, `guard-reload-runtime-implementation-next/BUDGET.md`: `025e0e9aeb7d2a23a4dfd59868d941ddacae50d9020d32deba65fd75648ec09e`.

## Exact 22-file union

The two codec files are:

- `include/rf/npc_checkpoint.h`
- `src/core/npc_checkpoint.c`

The 20 runtime files are under `src/diagnostic/`:

- `scene.c`
- `scene_ai_ammo_fallback.inc`
- `scene_ai_gameplay.inc`
- `scene_ai_reload.inc`
- `scene_ai_reload_completion.inc`
- `scene_npc_checkpoint_capture.inc`
- `scene_npc_checkpoint_resources.inc`
- `scene_npc_checkpoint_restore.inc`
- `scene_npc_reload_checkpoint.inc`
- `scene_npc_reload_player.inc` (new)
- `scene_npc_reload_player_decl.inc` (new)
- `scene_npc_reload_profile.inc` (new)
- `scene_npc_reload_publication.inc` (new)
- `scene_npc_retirement_orders.inc`
- `scene_riot_shield_gameplay.inc`
- `scene_riot_shield_history.inc`
- `scene_scripted_disarm.inc`
- `scene_turret_operator.inc`
- `scene_world_load.inc`
- `scene_world_snapshot.inc`

Documentation additions are outside that 22-file source manifest. A subsequent source change needs an updated identity and review of the changed joins; the listed acceptance does not automatically extend to different bytes.

## Complete-review coverage

The accepted review covers dynamic full-instance admission without a UID 109 gate; original authored-duration conversion; target-independent initiation; finite-ammunition exactly-once completion; pending/due-zero and legacy separation; reset/busy joins; absent-slot capture; canonical replacement and retirement; independent script, pain and RFAP ownership; RFNC27 interface; and full source budget accounting.

It also covers the incoming/outgoing union before demand, zero-reference metadata-only qualification, retention of both sides' requested resources, two-sided on-foot admission, all-row lifetime/reference-prefix/medic-child sealing, and one native close attempt before coupled NPC/RFAP publication. The direct post-close chain and cleanup retain their established ordering. Unsupported occupied/player-gunner/turret transactions reject as a whole; earlier world side effects are not represented as rollback-safe.

The review inspected relevant unchanged constructors, motion/cache, registry/list/body/medic close and pre/post-close call paths. It independently regenerated the runtime source delta; equivalent hunk placement did not change the accepted source identities. Source review establishes neither successful execution nor a broader supported actor, weapon or occupied-player profile.

## Remaining gates and integration discipline

1. Preserve the exact complete codec/runtime union. Do not split timing, pending ownership, RFNC27 or publication protection into separate activated features.
2. Resolve protected current-save and saved-consumer compatibility explicitly. Older RFNC26-only binaries reject newly written RFNC27; retained RFNC1-26 parser semantics do not waive that boundary.
3. Obtain focused parent-controlled Xbox compilation and runtime validation for this exact composition, including stack and peak stock-64-MiB fit under unchanged constructor, world-stage, cache and file caps.
4. Validate due-zero, current-inventory zero transfer, clip completion/eviction, old-absent/terminal replacement, native close failures and occupied rejection before activation.

The source review and this documentation pass did not apply source to the repository, execute or import project code, run tests, create fixtures, compile, use XEMU, change HDD state, manipulate locks/queues or activate gameplay. This document does not claim any later integration or execution result.
