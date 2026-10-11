# Fresh-controls integration handoff

Source-only implementation. No import, syntax/AST check, test, build, emulator run, HDD change, image/audio/FPS capture or cleanup has been performed by the implementation worker. Independent static implementation review approved helper22ba54ee; syntax, import and runtime remain unverified. Final parent-runner approval is separate.

## Exact identities

- Approved design: DESIGN.md, SHA256 427db3a4e241ae804618ad5a31464336b39b3f14990ccfa8e7553280a1d417f7.
- Clean dependency baseline: b777b55d863600498c3fb27d2fd66ce5f389444f.
- Candidate helper: tools/xemu_original_fresh_controls.py, SHA256 22ba54eeac19d9f393f987ca4375b1f013180a434dd7f429910ada3908b1a02d.
- Natural-save observation/common helper: tools/xemu_natural_save_gameplay.py, SHA256 cde478827c8eb1df47f1845dc1b1dcab93270e1641544d1355c8d94ff00b362a. This includes the independently reviewed additive128-word NPC velocity observation.
- Strict native lifecycle helper: tools/xemu_native_world_save.py, SHA256 585626a0d3c3502e666c04b6641e659ae703353511589ace1e80fd617941626e.
- Original startup recipe/symbols: tools/xemu_original_startup_admission.py, SHA256 79be1db965d4779f1c4f36778a0b38ec6f1a86012b728ed2dedf7228039c24ad.
- Borrowed lock/hash/report helpers: tools/xemu_fighter_grounded_exit.py, SHA256 50947e2c87f8f43808994f0487892911cbe35f7a092fcf332a766b8a8fbb904b.
- Exact symbol resolver: tools/xemu_original_switch_sound.py, SHA256 153dc297a16cafb8c38b0cb114d8b0fc3353620ea3f61e066a964bc3e415f952.

Integrate only the new tools/xemu_original_fresh_controls.py after independent implementation approval. No C/C++ or persistence-helper changes are proposed. Parent owns documentation/TO-DO updates, committing and the final hourly runner.

## Suggested parent invocation, never executed here

After the one scheduled06:00-or-later Xbox build, with a clean committed tree and its exact build manifest:

    python3 tools/xemu_original_fresh_controls.py --parent-hourly-batch \
      --root /workspace/shared/OpenRedFaction-attack \
      --consumer-build /workspace/shared/rf-20261011-0600-build.json \
      --out /workspace/shared/rf-fresh-controls-20261011-0600 --seconds 600

Output must not exist. Parent must retain its serial batch lock and fail-stop ordering. The helper never builds; it repacks the pinned existing XBE once and invokes the native guest exactly once. It never copies or opens a producer HDD, checks a backup receipt or calls a saved-load admission path.

## Parent runner integration contract

Use one build, then fresh controls, then the separately named original normal-save producer. If fresh controls exits nonzero, misses a sample, lacks an actual move/shot, has dirty shutdown or fails restoration/integrity, stop the batch immediately without producer or retry. Preserve each phase's independent evidence directory. The existing producer still performs its normal save120; it must independently pass, be durably backed up and pass its existing receipt gate before any saved-load consumer. Fresh controls is never substitute save/load evidence.

Expected successful fresh status: PASS_ORIGINAL_FRESH_START_BOUNDED_CONTROLS. Require status plus every checks value and restoration_verified=true with restoration_errors=[] before advancing. The single exclusive fresh-controls-attempt.json is never deleted/reused after failure. A different output directory does not authorize retry; parent owns the one-attempt schedule.

Proof paths to retain:
- verification.json, recipe.json, build-manifest.json and fresh-controls-attempt.json
- fresh-controls-input.bin and pack-only.log
- fresh-live-before-input.json and fresh-live-after-move.json
- fresh-controls/result.json, fresh-controls/shutdown.json and native stdout/stderr logs
- tested-default.xbe, tested-main.map and disc-restore.json

Original ISO is temporarily moved to original-disc.iso only while the helper runs; successful restoration moves it back to the original path. Do not expect it in the final proof packet. The baseline HDD is never copied or promoted. Parent separately owns preservation, output packaging and permitted cleanup.

## Interpretation boundaries

The two live samples surround the15-frame forward segment; displacement is not isolated exactly to those frames and no direction/causal/contact conclusion is warranted beyond the reported tagged inputs and finite horizontal displacement. One shot plus16-to15 loaded ammo establishes the observed ordinary fire/consumption path; terminal hits/kills are raw observations, not a hit/damage validation. The actual fresh pistol inventory is the port constructor's observed inventory, not an authored mission inventory guarantee. Existing observers may show normal NPC movement/event activity without requiring checkpoint readiness. Full-zero world/storage diagnostics are specifically justified by the approved exact fresh selector/source flow, not a general persistence predicate.

## Independent static approval

The final helper22ba54eeac19d9f393f987ca4375b1f013180a434dd7f429910ada3908b1a02d received APPROVED_STATIC_IMPLEMENTATION. Full review and machine-readable receipt are at /workspace/shared/fresh-controls-review-next/IMPLEMENTATION-APPROVAL.md and IMPLEMENTATION-APPROVAL.json. The final one-line allowlist hardening includes every native known selector name, including world-fixture.0/.1, as well as broad mutable controls. Parent was notified directly by the reviewer. This is not build, syntax, import, game-action or runtime approval.
