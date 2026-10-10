# Authored waypoint continuity: RFNC23 / RFCH9

Source-written, independently reviewed staging; no compilation, syntax check, test, fixture, or runtime was performed by the codec worker. Baseline is source134 / d1cf63798490e212096fae67a5fb7ab973827ee8. Integrate with the matching authored-waypoint runtime/parser patch before the parent-owned Xbox batch.

## Ownership and source contract

The five actual L3S1 guards (3, 407, 892, 897, 929) bind their own named finite Loop/Ping Pong paths. Runtime qualification checks guard1/guard2, raw AI byte2, actual named path and plain finite navigation nodes. Original constructor46483c..464866 binds the actor path/mode; default action4 derives from raw byte2 through59c014. The default-return path407ee0→4270f0 resets cursor0 but preserves shared direction6b0. Set_AI_Mode4bc6c0 changes current action only, not default407eb0.

Direction applies before route mode at407b12..407b26. Loop underflow407c33..407c44 wraps to count−1 without clearing direction; Ping Pong407c5c..407c66 clears direction and selects1. New qualified Loop reverse is therefore retained. Generic legacy reverse admission is unchanged. Qualified One way event replacement remains unsupported and is rejected by the runtime before mutation and by candidate source admission.

No new event UID, actor UID, source hash, catalog hash, or authored default-action field is invented. Current event routes and immutable actor-default provenance are distinct. Current3 is admitted only with qualified patrol metadata and the existing supported RFNC combat owner. Current4 requires an actual active path with either exact actor-default provenance or a real linked event. Current10 requires the exact linked Goto_Player, retaining active or inactive override ownership. Existing seat13 admission still requires the existing seat component, including nonpatrol actors whose generic history lane is already present.

## Exact 36-byte lane

Nine little-endian words: flags, signed action, byte offset, node count, cursor, waypoint-section hash, origin, event UID, patrol bits.

- flags bit0: present; bit1: path; bits2–3: path mode; bit4: current direction; bit5: current authored route active. Other bits are invalid.
- origin0: none, including combat pursuit; origin1: real authored movement event; origin2: this actor's immutable authored default. Origin2 always has event0; origin1 requires a real nonzero event UID.
- patrol bit0: enabled, source-qualified default; bit1: independently retained shared direction. Shared direction survives no-path combat/Goto and terminal metadata. Whenever a current path is retained, its direction must equal this shared bit.
- Absent lane is bitwise zero. Present path validates count/cursor, exact borrowed byte offset/count, full waypoint section hash and actual node bounds. Byte offsets need not be C-word aligned.
- Action matches raw RFNC AI, except the existing section policy maps constructor-unset0 to -1 in this lane only. The raw RFNC record is never rewritten.

RFNC23 appends36 bytes after the RFNC22 medic tail on every row, only when any row has a present lane. Earlier formats decode canonical absence. The existing active move168 retains exact destination, cursor/mode/direction/count, current navigation window, retry, fall speed and start/goal points. Overlapping lane facts must match it exactly. Inactive retained path and event ownership live in the new lane. Path targets are checked against the exact actual node. Candidate paths are rebound privately.

RFCH9 actor rows append the same36 bytes at offset104, growing104→140. RFCH1–8 decode absence. The old32-byte static campaign_ai_saved_modes array is removed and replaced by actors.routes at the existing level/UID key. Every supported in-memory sidecar entry is represented; malformed lanes retain a hard omission/admission failure. Unrelated shield/player/weapon-mode omissions are unchanged.

## Capture, history, and legacy semantics

Untouched ordinary current rows can retain an absent lane. Qualified patrols, retained generic histories, and inactive/event path ownership use it. Section capture uses the lane for all supported prior sidecar modes and proves every affected actor before any goals/switches/triggers/events/shield/actor history publication. Living section combat3 and seat13 are explicitly unsupported; no target is silently discarded and no invented default-after-combat kind is encoded. Patrol LookAt/ShootAt intersections remain unsupported. Active patrol script animation is a section capture guard; same-level RFNC keeps its existing animation payload/admission.

Same-level saves preserve current combat3 exactly using the existing RFNC target/alert payload. Only current-level living3/13 can appear in RFCH, and each must have its exact RFNC counterpart; remote living3/13 is rejected during target-level candidate preparation. Remote lane source/path/hash validation occurs when its actual level is loaded, before applying it. Structural QuickLoad preflight intentionally does not classify history against the old live level, which may differ from the saved target.

Section revisit retains the current cursor, path/mode/direction, current action and event/default ownership, rebuilding navigation from the authored revisit placement. It does not claim to persist section-exit world pose, retained navigation window, combat target, animation or every generic NPC subsystem. Those remain existing bounded section policies. Same-level RFNC is the exact pose/window continuation.

Legacy same-level absence disables the new constructor patrol only at final successful publication, then restores event provenance from the already assigned legacy movement. Saved AI and movement are preserved. Legacy section history with saved vitals/retirement but absent route likewise does not start a newly discovered patrol: only an exact fresh enabled actor-default/action4 constructor owner is cancelled. Its four initializer-written action/clock/argument fields and view action revert to the pre-feature constructor zero, while independently restored AI suppression flags remain intact. A first registration with no prior history keeps the new constructor initialization.

A restoring-only current identity capture can fingerprint an existing qualified pending medic14 after a generic section lane exists. It retains raw14 and the complete old medic owner snapshot; such a row cannot encode into RFNC. Save-only pending/residual43 guards and candidate admission remain separate.

## Transaction ordering

Current actor-route capture and RFCH snapshot write private rows/store copies only. Rebind preparation validates actual current keys and source lanes; RFNC/RFCH join compares every36-byte current lane exactly at its rebound slot. Both current and candidate source identities are checked before assignment. Nonfresh route-relevant candidates retain the complete owner/pose before-image plus an exact36-byte history-before lane and reject intervening changes.

Active existing RFNC movement follows the existing assignment phase. New patrol/default/provenance, inactive retained route state and history keys publish only from scene_npc_checkpoint_restore_finish after the composition's final successful storage close and history rebinding. No default-return/restart callback is called. Failed candidate preparation never changes these lanes. The existing sole scene_world_load call is on fresh frame0; ordinary QuickLoad validates/closes before replacing the old scene. This does not claim whole-world rollback for unrelated preexisting fallible vehicle publication.

## Stock-64-MiB accounting and limits

- Shared steady actor history gains36×2048=73,728 bytes; removing the old32×2048 static sidecar saves65,536 bytes. Net steady history increase8,192 bytes.
- Runtime patrol/origin fields add12 bytes per NPC. Each existing full owner-before snapshot consequently adds12 bytes too.
- Each RFNC in-memory record gains36 bytes. Restore saved+before rows gain72 bytes per entry; new route-before lane and snapshot flag add40 bytes. Together with the owner-before growth, restore entry growth is124 bytes on the target32-bit layout, before existing alignment. Existing sizeof-based allocation/cost accounting includes it.
- Every full private RFCH snapshot/candidate actor store gains73,728 bytes. No new automatic full-history stack copy is introduced; source has the global actor store and heap-backed RFCH stage.
- Maximum RFNC wire growth is36×2048=73,728 bytes. Row maximum1732→1768; theoretical component bound becomes64+2048×1768=3,620,928 bytes.
- RFCH actor stride104→140; theoretical maximum330,600→404,328 bytes, exactly+73,728.
- The stock profile0 RF_CHECKPOINT_FILE_MAX=110,524-byte transport cap and the composed2-MiB load staging cap are unchanged. Makefile262,144 is conditional on expanded profile1 and is not the queued stock runner cap. Oversized saves/stages reject through the existing capacity checks. Growing a theoretical component bound does not promise that every maximal row set fits the transport.

Changed cap/accounting sites: include/rf/npc_checkpoint.h; src/core/npc_checkpoint.c encode/span/decode; scene_campaign_history_checkpoint.inc max/stride/heap stage. Existing scene_world_load.inc, scene_world_restore.inc and scene_npc_checkpoint_restore.inc charge sizeof-based stages and record arrays; no budget raise was made. Existing fixed-format old test scripts (for example frozen RFNC14 or single-driver RFNC10 fixtures) remain fixed fixtures, not general new-format parsers; no new fixture or tool execution was added.

## Recovery and integration

The executor filesystem was replaced at16:23 UTC. Parent recovered exact source134; the codec worker reconstructed its recorded edit scripts against that baseline and durably archived the stage before continuing source review. Runtime/parser changes are separate, including body fields/constants, the early scene_patrol_* helpers and command provenance. This patch relocates the unchanged exact event movement-binding helper into the earlier AI persistence include so history can use it. It owns the route codecs/capture/history/restore and the bounded actor-history/section preflight wiring in scene.c.
