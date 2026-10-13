# Civilian reaction to nearby player gunfire

Status: source-written on e0828e68b8cd0f13c6d30dcd08d804acdc8621e8; compilation and runtime are unverified. The parent owns the next hourly stock-64-MiB Xbox compile/package and short startup smoke. This worker has not built, tested, launched the game, touched a saved disk, or created a fixture.

## Missing operational consumer

`campaign_enemy_hear_shot` expressly rejects unarmed actors. The existing qualified civilian flight owner previously acquired the player only through an actual damage reaction. Firing a loud weapon near an undamaged civilian therefore had no flight consumer, despite the installed original's explicit nearby-noise branch.

This slice connects accepted loud on-foot player gunfire to the already-qualified ordinary civilian flight family. It does not call a damage adapter or alter health, affiliation, attack orders, armed hearing, the existing armed alert count, or disguise consequences.

## Original evidence and bounded differences

Installed `RF.exe` SHA256: b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

- 426ac3–426b0c: the real shot tail checks authored noise/suppression and emits nearby alert408280. Its radius40 differs from this port's preexisting16-unit hearing policy, which is intentionally retained.
- 408332–4083a5 and408418: nearby peers must be Waiting2 or Waypoints4, pass sentient/class eligibility, omit Goto suppression40000000, noise exclusion800 and800000, and not have friendly affiliation2. This slice admits only Waiting2 within the existing no-Alarm civilian family. It does not expand patrol, Alarm or legacy medic qualification.
- 4083fc–408412: range is measured from the peer's published origin to the noise origin. This slice retains the existing port's finite Euclidean range policy and adds no facing or clear-ray test to hearing.
- 40852e–408550: an unarmed primary with the real alert flag requests one binary32-stored random10–20 second duration through4085f0. This is distinct from damage407fb0's two60–120 second requests, which remain byte-identical.
- 4085f0–40878f: the preexisting flight owner already implements the admitted no-flee/suppression, animation/weapon reset, timer, target and navigation request path. The new consumer reuses it unchanged.

The original global750ms noise arbitration, visible-room optimization, armed investigation state and peer/NPC-origin noise are not reconstructed here. Existing audio/noise range policy is not retuned. The change adds the previously absent unarmed operational reaction through the already-admitted port owner.

## Live ownership

Ordinary primary/alternate firearms invoke the new consumer once inside the already-accepted `hearing.emit` branch, after the existing armed hearing call and before pellet/ray iteration. Dry fire, melee, authored silent weapons and an attached Undercover suppressor do not originate the new reaction. The accepted player's full handle and current world epoch are captured before launch/audio callbacks; a later replaced player or world cannot become the source of that earlier shot.

An actual Flamethrower damage pulse uses its captured source/epoch and the same `scene_player_shot_hearing` policy with the retained Flamethrower descriptor. Its existing armed hearing is unchanged. The mounted turret path is unchanged; the civilian consumer independently requires the existing actual on-foot player receipt.

Each candidate must already be a live, nonlegacy qualified civilian with both weapon slots empty, Waiting2, no entered episode, no no-flee/noise exclusion, and no script movement/look/fire/animation, patrol, seat, controller, attachment, scripted physics or combat overlap. Friendly affiliation2 is excluded as in the original nearby alert. Existing class/motion admission establishes grounded sentient humanoids with actual flee/cower clips; no new class allowlist is invented.

Before any duration draw or flight request, the consumer performs the existing flight visibility-capability admission and complete actual motion-cache demand. Capability admission is not a requirement to see the shooter. Source/body/possession and actor identity are sealed again before publication. A new accepted command continues to use the existing civilian cancellation and lifetime fence. Active flights do not restart on every automatic round.

## Resources and save continuity

There is no new actor field, global mutable owner, heap pool, motion asset, playback slot limit, save version or save record. The bounded accepted-shot scan borrows existing actor storage. Actual clips use the unchanged1280KiB NPC motion cache and16-slot playback admission; unavailable resources leave the civilian unclaimed.

The ordinary civilian flight snapshot already represents the full player target,10–20 second remaining deadline, movement/navigation state and shared CRT RNG receipt. RFNC30+ admits this same entered family; RFNC validation does not infer60–120 seconds from the damage origin. Existing RFNC32/RFCH10 publication, candidate source joins, mandatory entered-flight RNG, cancellation and final-storage-success restore remain unchanged. No noise-origin tag is needed for continuation: after onset it is the same flight state and scheduler.

The historical medic783 lane and its compatibility receipts are untouched. Broader campaign/functional verification remains with testers.
