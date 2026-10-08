# Concurrent authored Fighter motion

The secondary vehicle runtime now has stable per-owner slots, allocated only for actual authored commands. APC graph navigation remains supported. Eligible unoccupied Fighter01 owners use their own fixed-point Goto, rigid state and the existing hover/dry-hull flight solver. A level does not need a selected player vehicle to run these owners. Bounded stock-64-MiB cloud Xbox checks pass independent motion, selective freeze/wake, exact ordinary-save continuation and a genuine RFSV1 compatibility load.

## Ownership and motion

The pointer bank is bounded by the existing passive owner count. Runtime addresses remain stable, with deterministic authored-slot stepping and a separate last-frame guard per owner. Pausing, freezing or retiring one entry does not stop traversal of the others. Existing registry handles, damage, affiliation, chassis rendering and borrowed resources remain authoritative; no player vehicle, duplicate entity view, cockpit or ammunition owner is created.

APCs keep suspension and graph-prefix replanning. Fighters use the existing `rf_vehicle_submersible_step` first-pass hover/drag policy with their actual class mass, speed, acceleration, rotation and hull. They have no springs. Each candidate copy rebinds collision/query/liquid pointers before stepping, then publishes complete poses and accepted contact velocity after success. Authoritative rigid velocity stays separate. Other managed chassis contribute angular point velocity from their own inertia and momentum to the ordinary owner-qualified query.

This remains sequential collision against current committed poses. Seats/riders, moving promotion, autonomous weapons, formation avoidance and coupled impulses/crush are unsupported. Existing rider ownership prevents admission or pauses an already-owned chassis. The fixed destination is determined by event type5 and its authored position, never its display name.

## Authored event dependency

Original L20S2 UnHide18456 links Fighters4801 and18353 plus Goto18457. The old type50 scene dispatcher queued visibility but returned before its outgoing-event action, so both Fighters became visible without receiving the movement order.

The narrow shared fix forwards resolved kind6 event links through the existing dispatcher during action2. Entity visibility still belongs exclusively to deferred `rf_unhide_tick`, with its existing ON-before-OFF order and500ms cooldown. Trigger/mover/entity links are not activated by the new propagation branch. Source/actor/OFF mapping and the depth64 recursion guard remain in the common dispatcher.

The original activation-prefix evidence in `tools/verify_event_activation.py` covers4b8b70/4b8c40 and includes type50 propagation; that verifier intercepts effects and does not by itself prove recursive runtime wiring. The new bounded native audit supplies that wiring evidence using a private heap registry and logging callbacks. It covers mixed event/non-event links, immediate and delayed ON, Invert OFF, deferred visibility without duplicate Goto, cooldown and a self-cycle returning RF_RANGE. Exact downstream original-game parity remains outside this practical binding.

## Persistence

One APC still writes byte-compatible RFSV1 and reads the genuine prior format. RFSV2 uses the same16-byte envelope and explicit256-byte row for multiple owners or any Fighter. Rows carry profile/UID, rigid values, order, lifecycle, accepted velocity and copied RFVA flags/vitals. APC rows retain semantic graph state; Fighter rows require canonical unused navigation fields. No handles or pointers enter the wire.

All rows and the owner-indexed pointer bank are staged before publication. Validation checks unique owners/slots, exact original Goto targets, class/profile rules, finite state, frozen state, and agreement with RFVA/RFPV. Every visible candidate passes solid-world, actor and other-candidate hull checks. Fighters also pass a complete dry-hull test against candidate geometry and immutable liquid metadata. Hidden candidates retain typed identity and no-rider checks before hull omission. Only after the entire collection passes does assignment exchange the prepared pool and restore projected flags/velocity. Missing RFSV clears the old pool without replay.

Passive-only levels keep RFVA host_bytes0. RFPV can be absent; selective scripted freeze produces RFPV1 when there is no selected-host visibility state. The fixture does not invent an active-host record.

## Static-edge collision dependency

The separated source phase passed, but its first fresh load correctly rejected a saved Fighter hull that had entered an original thin ramp edge. Independent decoding found sphere0 of owner4801 penetrating face919 by0.290302 m; its initial pose was clear. Both the face and sphere are within room16 and its primary room6 bounds. The checkpoint clearance remains strict.

The recovered face sweep runs its edge tests only after a forward plane hit. A focused calculation reproduces the missing case: a radius1 sphere moving from(-2,0.5,2) by(3,0,0), parallel to a solid square atY0, receives no face hit while the existing sphere-edge routine reports contact at0.377991527. Independent Fighters now complement their ordinary queries with nearby static edges, preserving solid-face filters and primary/child room traversal. Only a nearer approaching contact can replace an existing result. This is a practical vehicle binding; the recovered core face routine is unchanged.

The complement is limited to independently simulated Fighters and uses the same actual hulls and candidate-world checkpoint admission. Other vehicle profiles, selected-player flight and moving geometry retain their existing query paths. Source and native regression evidence is recorded below.

The first complement run passed its native checks and produced11 real edge contacts, but still failed strict restore. A retained per-frame pose trace located the first overlap at frame92 (0.104 mm), growing beyond the existing2 mm checkpoint margin at frame94. At this grazing contact, the solver's1 mm retreat along travel gives only about2.5 micrometers of normal clearance, below the30.52-micrometer float spacing near worldX416. The rotation query's actual sequential float transform starts5.22 micrometers inside the edge and shifts the entering root to−0.06285, outside the recovered edge primitive's−0.05 clamp. A direct C replay confirms the miss.

The scoped complement therefore checks the closest point on a finite edge before that primitive. An already-touching or overlapping sphere receives a zero-time contact only when it moves farther into the actual radial normal. Retreat and tangent motion remain free. Clear starts still use the existing sweep primitive. The shared solver, original geometry, replay timing and checkpoint tolerance are unchanged.

## Bounded evidence

`tools/xemu_secondary_fighters.py` preserves both original Fighter records and transforms, original UnHide18456/Goto18457, geometry/navigation and the fixed destination(370.542358,13.248748,-400.271698). Both owners have900 health/0 armor and start hidden and unoccupied. The original two-sphere hulls are separated by at least0.3753675m. Unrelated actors, controllers and triggers are isolated.

The source uses the actual UnHide→Goto chain, freezes trailing18353 while leading4801 continues, then writes an ordinary save. Fresh load compares both complete saved rows before integration, preserves selective freeze, and uses ordinary OFF to wake only the trailing owner. A separate short load uses the genuine earlier4912-byte RFSV1 APC save and its unchanged original fixture archive; no logical save field is rewritten. The final presentation sample must equal the last actual world step.

The first source failure is retained: it completed120 frames and wrote4536 bytes, but no independent owner was admitted because of the UnHide propagation gap. The observer rejected that run; it is not movement evidence.

The next source run admitted both owners and passed all20 pooled-state checks and15 event-dispatch checks, but failed the continued-motion assertion after freezing the trailing ship at1.0 seconds. Independent reconstruction of the real sphere hulls found the pair within approximately5 micrometers of contact. The sequential solver continued stepping the leading owner but admitted no further displacement. That failed run is retained as a known contact-stop limitation; coupled response and formation control are not established.

The bounded independence fixture freezes the trailing ship earlier, at0.75 seconds, while preserving original geometry, owner transforms, commands and collision. Both120-frame phases require a positive actual sphere gap on every frame. The minimum records both full poses and the actual collision spheres, and a separate Python calculation reconstructs that minimum from the original model. The acceptance claim therefore requires motion, selective freeze, ordinary wake and restore while the chassis remain separated.

The final source and fresh-load data are retained under `artifacts/xemu/secondary-fighters-20261008-011009`. Each boot completes120 frames on stock64 MiB with a living player and unchanged900/0 vehicle health/armor. Source leaves6459 free pages; fresh load leaves6299. All40 staged input paths restore exactly.

- Frames20–40: both original owners take20 independent steps and move1.691695 m/1.692443 m toward the authored fixed point.
- While18353 is frozen,4801 moves1.567901 m over frames60–78. Source totals are118/44 steps, with stable full handles1376276/1441813.
- Ordinary save writes5064 bytes. Fresh frame0 restores both complete rigid/order/freeze rows, RFVA poses and RFPV state exactly, without setup replay.
- After load,4801 advances0.735382 m while18353 remains frozen.4801 then retains its exact ramp/ceiling-contact pose from frame40 through119 while its independent step count reaches118. The ordinary OFF wakes18353, which advances2.589685 m over frames78–118 and reaches57 steps.
- Every actual pair-hull gap is positive in both120-frame phases: minimum0.3753593 m in source and0.000120799 m after load. Independent reconstruction of all actual hull poses finds no checkpoint-invalid static pose: the largest source contact overlap is22.80 micrometers at frame76, within the existing2 mm allowance, and every loaded hull is separated. The loaded stopped sphere remains about10.26 micrometers below the ceiling and10.46 micrometers clear of the finite ramp edge.
- Native checks pass20 pooled-state rejection/atomicity cases,15 event-dispatch cases and12 static-edge groups, including the exact failed92/94 segments and free retreat/tangent motion. The source records100 actual edge replacements for4801.

The original final-run report retains its FAIL: its old assertion incorrectly required the leading ship to resume moving when the trailing ship woke, despite the leading ship already contacting the original ramp. Corrected read-only validation requires the exact stationary pose, continued independent steps, actual zero-time ramp contact, early post-load leading movement and real trailing wake/movement. `validated-report.json` records this additive validation; raw source/load data and the original FAIL are unchanged. The legacy phase was then completed separately with the identical compiled PE.

The genuine4912-byte RFSV1 APC save from the prior accepted build loads unchanged, restores its retained graph cursor and moves2.986384 m over frames0–40. Its60-frame boot leaves6272 free pages, with the player alive and all40 inputs restored. This establishes actual backward-read continuation, without rewriting the payload or replaying its setup. Legacy payload SHA-256: `026277df463ca88cd032f38c81bfed2c91e60d299fd1375ba5dfe532bb9a2324`.

Exact source/fresh-load artifacts:

- RFWC SHA-256: `0a85abc74c1caf1e412dc7f21aa26cdd03fab86437df7136e378fcfcffa53253`
- Common tested PE,3690496 bytes: `3abb3e20eb46242995917e59dfd8465f953cf2c7a4789a89bf1d7c5b57faef10`
- Common map: `6adb012cdbba016cbe232b37cf754f787bb8761b6e1ce8a9b3f8a11e9b6cde3d`
- Source XBE,3702784 bytes: `c22035147341a43b3eae07d8becd7be57ed9bc66552d8540eeed4b248bf48ce7`
- Loaded XBE,3702784 bytes: `854ec38b4e36d166c8899f9b4e298e34f94dea7cdce0024bbd867f69dbe87a1d`
- Legacy XBE,3702784 bytes: `cc1908f538ffe0ee71506760f7ef0caf15d4c5cffac536ffed40fcb8f927a674`
- Original-geometry fixture VPP: `ba59504aa3d2216483fc1706d35dc26c81e0ef4e01d11c172d543b8a80adf465`

Full arrival at the shared destination, static obstacle avoidance, natural combat/campaign progression and simultaneous collision impulses are not claimed. The sequential close-pair stop and original-ramp stop remain explicit first-pass limits.
