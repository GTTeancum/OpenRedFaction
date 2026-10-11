# Existing NPC hit bank: additive terminal capture

Source-only observation change. Add one existing symbol, rf_scene_enemy_actor_hits:113, to tools/xemu_natural_save_gameplay.py SYMBOLS. The shared terminal union also carries it into the independent fresh-controls helper. No C/C++, live probe, input, acceptance, save/load admission, run duration, retry, memory-write or runtime behavior changes are proposed. No import, test, build or emulator execution has been performed for this stage; no actual hit is claimed.

## Exact source contract

The existing array in src/diagnostic/scene_ai_actor_hit_probe.inc is 113 uint32 words: one count followed by the first 16 rows of seven words. It is cleared at scene.c:16523 during frame-zero combat initialization. The count increments before the capacity check; this is not a ring. A count above16 means later observations were omitted. Only rows0..min(count,16)-1 are recorded; unused zero rows are not events.

For row i, words1+7*i through7+7*i are:
- Source UID
- Intended UID
- Actual UID
- Frame
- Returned damage amount, raw float32 bits
- Actual target health at probe time, raw float32 bits
- Intended target health at probe time, raw float32 bits

UID0 denotes the player. UINT32_MAX denotes an unresolved handle, a point target or another unmapped target; its returned health defaults to0. That zero must never be classified as death. NPC UIDs are mapped from current registration handles by this bounded observer. The row does not retain full handles, generations, an epoch or a portable owner binding.

The probe returns for amount<=0. Normal successful callers validate finiteness upstream, but consumers must still require a finite positive decoded amount before making any damage observation. Its amount is the damage API's returned scaled amount after applicable class/difficulty handling and before armor absorption, not measured health loss. No health-before value, armor-before value, selected factor or unique lethal cause is recorded. Actual/intended health is sampled after damage and any preceding death/impact callbacks at that callsite.

## Covered callsites and limitations

- Ordinary NPC bullet path: scene.c:12817, after successful damage and existing identity/callback checks; the enclosing condition excludes melee.
- Shared actor penetration: scene_ai_actor_penetration.inc:238, after accepted actor damage and the existing source owner/handle guard.
- NPC shotgun: scene_ai_shotgun.inc:105-107, per qualifying pellet after its damage path.

Rows are damage observations, not shot counts, a complete hit history or a unique death ledger. Melee, other projectile/radial/event paths, healing and player-fired damage are not generally covered. An absent, zero or saturated bank does not prove no combat. Callback-invalidated identities can prevent observation even after an earlier operation.

A valid mapped NPC row with finite nonpositive post-damage health may help identify a contemporaneous ordinary death candidate, but does not establish a unique killing shot, death-animation completion or stable lifetime ownership. A row's frame belongs to its probe; terminal collection does not make all rows contemporaneous with save120 or with the velocity-reject tuple. Do not infer a save fix, selected damage factor, health delta, complete combat functionality, hit-location branch, weapon identity, visuals, audio or FPS from this bank.

The addition is terminal read-only evidence in the already scheduled runs. It creates no extra scenario or test, and no pass/fail predicate depends on its values. Missing exact symbol preflight still fails normally; the native result preserves the raw113 words under extra.rf_scene_enemy_actor_hits.

## Exact staged change

Clean source baseline: e4f12f3c951aea4b007c9a0d8db9328c5c2877e1.
Existing helper SHA256: cde478827c8eb1df47f1845dc1b1dcab93270e1641544d1355c8d94ff00b362a.
Candidate helper SHA256: 2377a5863646185966115cd73ceccc618ac7a5dfbca2857706868619c9006157.
Patch SHA256: feab970e8539a6ddc54511bdc37fb9aa3b191b16a647c23d3112bb9df5106daa.

Apply only after independent review and parent integration approval. Parent owns any documentation/TO-DO edits, exact committed-source build manifest, scheduled serial execution and preservation of actual evidence. This stage changes no historical result or existing durable-backup requirement.

## Additive dependency impact

The fresh-controls helper SHA25622ba54eeac19d9f393f987ca4375b1f013180a434dd7f429910ada3908b1a02d imports SYMBOLS from this helper into its exact-address terminal union. It therefore gains the same113-word terminal read without code changes, new live probes or changed acceptance. None of common_checks, fresh_checks, producer_checks or consumer_checks consumes this bank.

Decoder SHA2561fc72f645fb998d52ddf5aa0e60fd7e6f32f5a749f7a3e2eafa7a0a76cca4376 reads only its named128-word velocity bank and six-word reject tuple from direct extra. It ignores this additional hit bank; its code and interpretation do not change. Earlier exact dependency approvals and the decoder source packet remain historical snapshots. The new independent review must bind natural helper2377a586 to these unchanged consumers rather than silently rewrite earlier receipts.
