# Independent fresh-controls implementation review

APPROVED_STATIC_IMPLEMENTATION

Reviewed 2026-10-11 at05:22 UTC. No blocking findings remain for this exact candidate.

Candidate: /workspace/shared/fresh-controls-next/tools/xemu_original_fresh_controls.py
SHA256: 22ba54eeac19d9f393f987ca4375b1f013180a434dd7f429910ada3908b1a02d
Lines reviewed: all350.
Approved design SHA256: 427db3a4e241ae804618ad5a31464336b39b3f14990ccfa8e7553280a1d417f7
Clean dependency source: b777b55d863600498c3fb27d2fd66ce5f389444f
Natural-save helper: cde478827c8eb1df47f1845dc1b1dcab93270e1641544d1355c8d94ff00b362a
Strict native helper: 585626a0d3c3502e666c04b6641e659ae703353511589ace1e80fd617941626e
Startup recipe helper: 79be1db965d4779f1c4f36778a0b38ec6f1a86012b728ed2dedf7228039c24ad
Borrowed-lock/hash/report helper: 50947e2c87f8f43808994f0487892911cbe35f7a092fcf332a766b8a8fbb904b
Exact-symbol helper: 153dc297a16cafb8c38b0cb114d8b0fc3353620ea3f61e066a964bc3e415f952
Integration metadata was inspected and agrees with these identities.

## Verified by static inspection

- Lines23,39..43 generate exactly240 RFI6 size48 records. The rf_scene_input layout is move[3],look[2],crouch,jump,use,fire,reload,cycle,alt. Only forward=1 for120..134 and primary=1 for180 are nonzero. No adaptive input, route, look/aim, further shot or retry exists.
- Lines46..101 define fresh-only predicates and retain actual inventory/body/ring/shot/ammo observations. Both probes require selected owned ID3/slot0/loaded16 and identical448-byte live inventory. The terminal16-to15 debit, unchanged125 reserve, no reload/denial/error, shot0/0/1 and no pickup confirm the intended limited ordinary-fire observation. Hits/kills remain raw and hit_damage_validated stays false.
- The borrowed live_reader enforces phase2 and windows[30,120),[140,180), retains all raw owners, and rejects nonfinite position/velocity before acceptance. Final ring tags120..134 must all match actual movement triplets. Horizontal displacement is correctly described between surrounding samples, not isolated to those15 frames or claimed to validate exact direction/collision/controller behavior.
- Zero world/storage arrays at pre/post/terminal are the approved source-qualified fresh isolation check. The caller has no load/save selectors or quick-action schedule. Startup storage reset and excluded storage entry points justify the predicate. Ordinary AI and events continue; there is no RFEC, restore-reject, NPC-capture-readiness or event-queue acceptance condition.
- Imports reuse only common checks, actual live observation, source recipe/reader, locking, exact symbol resolution and the normal native runner. There is no call to producer_checks, consumer_checks, admit_producer, decode_actual_payload or any persistence entry point. The sole run_guest at249 uses snapshot=True, capture_world=False, measure_fps=False and strict clean shutdown. The new128-word symbol is inherited only as terminal read-only telemetry; the reviewed natural-helper diff adds that one entry only.
- Lines115..183 enforce a new output outside the repository, current clean committed source, exact PASS_XBOX_BUILD manifest/XBE/map, original L3S1 recipe, exact map tokens, no existing XEMU, SessionLock and verified standalone B5DF base. No base copy or producer disk is created or consumed.
- Lines184..239 preserve original archives, immutable data, selector bytes/names, XBE/map and old ISO, then stage and byte-verify exactly five allowed controls. Established native FLAGS plus the broad campaign/flag/bin/txt scan remove save/load, fixture, grant/event/setup and audio selectors. Only a single pack-only command appears; the helper does not build.
- Lines240..255 reserve one exclusive attempt and call the native runner once. The shared native runner keeps stock64, disabled host autobinding, no audio output and no image capture. Probe failure, guest failure, shutdown failure or failed predicates remain failures without retry.
- Lines257..268 record distinct fresh-start/movement/fire observations. Strict shutdown additionally requires acknowledged QMP quit, completed normal wait, zero exit and no forced cleanup. PASS requires every runtime predicate.
- Lines272..344 restore selectors and exact ISO and check names, bytes, XBE/map, original archives/fixed data, source and base hash. Unexpected XBE/map changes stay errors even if repaired from preserved copies. Restoration errors downgrade status, set fresh_start_verified false and cause nonzero exit. Raw native result and shutdown paths are retained even when run_guest raises. There is no broad cleanup.

## Parent integration boundary

Only the exact new helper is approved for parent integration against the listed dependencies. The final committed source and final hourly runner need their own exact review before execution. Required order remains one build, this distinct fresh-controls check, then unchanged natural-save producer only after full fresh PASS. Any failure stops the batch; a new output directory is not permission to retry. The actual producer must still independently succeed and be durably backed up before the saved-load consumer can be admitted. Fresh PASS is never persistence or historical-regression evidence.

No helper import, syntax/AST check, test, build, runtime, HDD action, original-asset change, source edit or cleanup was performed during this review. No gameplay success is claimed by static approval.

## Final one-line selector hardening

The initial fully inspected candidate had SHA256bd4c13f5afeef18e9a6a454a57a30995f8a745b980da3df5c396e07003a10519. Final candidate22ba54ee changes only line225 by adding `path.name in names or` to the actual-controls selection. This explicitly includes every preserved known selector, including world-fixture.0/.1, in the exact allowlist. Those files were already removed and restored; this strengthens admission without enabling anything. A read-only inverse text substitution of this sole addition reproduces the exact initial candidate hash, confirming no other code difference. Approval above applies to the final22ba54ee candidate. Updated integration metadata names the same final hash.
