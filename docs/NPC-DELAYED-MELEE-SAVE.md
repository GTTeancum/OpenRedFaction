# NPC delayed-melee save continuation

## Status and scope

Source-written on October 12, 2026, against a094722da8bd6a29dba487927021f459e13cebdf after RFAP11 HEAP. Uncompiled and runtime-unverified. The parent owns the hourly Xbox compile/package and short startup smoke; functional coverage belongs to testers. No helper builds, tests, fixtures, emulator sessions, HDD operations, image work or publication were performed.

RFAP12 kind26 continues the existing Drone Smash, Reeper Claw, Baby Reeper Claw, Mutant Attack 1 and Mutant Attack 2 runtime. It preserves armed phase2 strikes and ordinary selected-controller cadence after a strike ends. It adds no weapon, damage policy, target acquisition, runtime attack bank, animation start or visual resource.

## Existing behavior retained

- RF.exe42606f–4260ce starts the melee action and schedules each authored impact separately. The implementation's metadata loaders already validate Drone Smash 400 ms, Reeper 400/800 ms, Baby Reeper 300/600 ms, Mutant Attack 1 150/500 ms and Mutant Attack 2 150 ms.
- Original4fa360 and4fa3f0/4fa420 supply the implemented modulo deadline/remaining-time semantics. The save uses signed remaining milliseconds, including an already-due negative offset. The second strike remains relative to the original accepted attack, never to the first strike's service time.
- Runtime phase1 is presentation ownership, phase2 is armed, and phase3 is impact-callback ownership. The service consumes a due bit before any geometry, rubble, shield, damage or death callback. These rules and all ticket/full-handle lifetime guards are unchanged.
- The existing reach/cone/contact policy remains a first-playable approximation. Capture never queries reach, LOS, cover, posed hit geometry or shield contact. A target that moved outside reach can still be a saved pending miss and is checked at the real due time.
- RFAP is this port's format. These receipts do not claim to reconstruct the original game's save bytes or exact original AI scheduler.

## Wire ownership

The RFAP header remains 16 bytes and each row remains 112 bytes. Kind26 requires version12. Writers retain the earlier smallest required version when there is no delayed-melee controller. RFAP1–11 remain readable.

A source receipt contains these little-endian words:

| Offset | Meaning |
| --- | --- |
| 0 | Kind26 |
| 4 | Dense controller receipt ordinal,0–63 |
| 8 | Positive authored source UID |
| 12 | Family0 Drone Smash,1 Reeper,2 Baby Reeper,3 Mutant Attack 1,4 Mutant Attack 2 |
| 16 | Original pending-bank slot, or UINT32_MAX when only cadence remains |
| 20 | Remaining impact mask |
| 24,28 | Signed remaining milliseconds for each still-owned impact |
| 32,36 | Target role and authored target UID |
| 40 | Exact source combat order mode |
| 44,48 | Exact accepted order-target role and authored UID |
| 52 | Source AI action |
| 56,60 | Remaining fire and navigation frames |
| 64 | Retained burst count |
| 68 | Shared spread RNG |
| 72–111 | Reserved zero |

Target roles are0 absent,1 the local-player role with UID0, and2 an authored NPC with a positive UID. Runtime handles, tickets and pointers are never serialized. A cadence-only row has no pending slot, mask, deadline or paired-target payload; its existing RFNC combat row owns any scripted/reactive target. Every living, registered, alerted source holding one of the five selected families contributes one controller row, including a source with no pending impact. This avoids RFNC's legacy ordinary30-frame awareness reconstruction overwriting a paid cooldown.

A one-impact family admits mask1. A two-impact family admits mask3 or mask2. Mask2 means impact1 was already consumed and only impact2 survives. With mask3 the two remaining offsets must differ by the exact authored separation. Unowned deadline words are zero on wire and become inactive-1 in the restored pending slot. Due0 remains an armed, due strike; it is never treated as absence.

## Capture and candidate joins

Capture is read-only apart from existing diagnostic counters. It validates every live pending slot, including orphaned slots absent from the actor iteration, duplicate source reservations, nonzero monotonic tickets and duplicate live tickets. Callback phases1/3, unknown phases, servicing and presentation all refuse. An invalid armed source is never silently omitted or cancelled by saving.

Both endpoints require the exact full registry/entity/physics identity plus authored seed/history provenance. Source receipts join the same pending RFNC source by UID, resolved handle and body slot. Family, current primary ownership, AI action, living state, alert, absence of a conflicting reload/retreat, order, and RFNC-owned cadence must agree. RFAP12 requires all eligible selected controllers, so a missing row cannot fall back to the legacy ordinary cadence.

Armed source and NPC-target candidates additionally require living visible on-foot state, with the actual RFNS candidate seat association checked rather than the outgoing link. The source retains holster, pending-single, scripted-animation, AI-mode, generated-base and order eligibility checks. The exact authored/reactive target must agree with RFNC's independently resolved attack target. The ordinary player's nullable order_target is retained separately from the strike's player target. No replacement actor or generated/turret/vehicle approximation can satisfy a saved paired NPC target.

The player role joins the saved player health and incoming seat state. The retained player death/hidden/link flags, and retained living-NPC810 death bit, are explicitly refused when incompatible; no life flag is cleared to make a strike fit. NPC and player positions, geometry, shields and actual due-time movement remain owned by their existing restored runtime.

The shared spread RNG must agree across all kind26 rows, delayed-projectile RFAP8 controllers, RFNC pending shots/combat/guard-retreat lanes, RFEN's already-decoded spread value and an active player-turret receipt. No RFEN schema or monitor code is changed.

## Publication and failure behavior

The private RFAP stage receives only resolved candidate receipts. Preparation runs no attack, presentation, cancellation, target acquisition or deadline consumption. Before live publication it checks dense row order, unique source UID/pending slot, ticket headroom and frame-rebase overflow.

Only after world publication and final successful storage close does the existing outgoing delayed-melee reset run. Then assignment-only publication restores the original pending-bank slots with fresh process-monotonic tickets, exact remaining bits, independently rebased millisecond deadlines and paid fire/navigation cadence. It restores the shared RNG without drawing it. No launch, inventory debit, animation, sound, melee contact or damage is replayed.

Save rejection and load preparation/storage failure publish no new melee receipts. Existing whole-world failure semantics remain unchanged; this slice does not introduce general rollback of already-published NPC/world state. Direct composed loading additionally refuses an outgoing active melee callback, while an ordinary armed phase2 timeline can be replaced only through successful final publication. Settled-only standalone NPC/player exports continue to refuse delayed-melee controller state rather than lose it.

## Stock64 and limits

- Maximum64 controller receipts. This is checkpoint admission, not an expansion of the existing64 live pending slots or authored NPC pool.
- Each staging owner is21 fixed32-bit words:84 bytes. The appended private stage is64×84 +16 =5,392 bytes, with no pointers, retained allocations or new runtime bank.
- At most64 additional112-byte rows:7,168 bytes. RFAP's structural maximum becomes741 rows,83,008 bytes including its16-byte header.
- The110,524-byte whole-checkpoint cap remains unchanged. Other components can exhaust the remaining envelope; ordinary save returns the existing capacity error instead of truncating or enlarging it.
- The2 MiB composed restore budget and existing byte accounting remain unchanged; the added private stage is charged through the existing sizeof-based RFAP stage admission.
- No new resource-demand bit, model, mesh, texture, particle bank or gameplay fixture is introduced.

## Remaining implementation boundaries

Callback-stack serialization, generated/non-authored melee targets, historical removed-source remapping, full transactional world rollback and original swept-limb collision fidelity remain outside this bounded slice. The existing guards continue to refuse unsupported ownership. Functional save/load coverage remains with testers.
