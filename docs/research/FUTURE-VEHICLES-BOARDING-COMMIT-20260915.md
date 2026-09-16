# Vehicle boarding commit orchestration

Priority P1. Ahead-of-implementation evidence; no builds, emulator or shared implementation edits. Original RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

`tools/future_re/vehicle_boarding_commit.py` executes cdecl4a1970 through return for16 combinations of use-kind1/4, supplied attach return0/1, supplied jeep-gunner predicate0/1 and actual ground-vehicle predicate false/true. Preceding admission services and selected notification/weapon/attachment services are supplied; original vector transforms, state stores,4a8670,4ad8a0,42d7b0 and40a420 execute. Results: boarding-commit.json. All16 cases pass. No claim of an actual moving or rendered vehicle.

## Ordered state boundary

At4a1dd8, vehicle use-kind1 calls4a8670(player), which zeros player+10c4/+10c8/+10cc, then ORs player+10 bit400. Turret kind4 skips both. Actual4ad8a0 clears bytes player+f94/+f95 and word+f98 for either kind. Reference field names suggest weapon zoom state, but the exact stores are the evidence.

Next41ae70(actor handle,actor weapon) is called. Actor world position+3c is copied to actor+13e0 then transformed by4fb990 relative to host position+3c/orientation+48. The identity-basis fixture actor(12,23,34),host(10,20,30) produces local(2,3,4). Nonidentity matrices are not covered by these16 cases.

At4a1e41, thiscall427240(host,actor handle,selected tag) executes. Its AL return is ignored: subsequent state mutations and callbacks are identical for supplied failure and success. This composes with the separately proved helper's partial-failure semantics; the fixture intentionally does not mutate ownership itself.

If42acd0(actor) says not jeep gunner,407e20(host+2a0,1,-1,-1) runs;407e80(host+2a0,0) always runs. Host+810 becomes `(old & ~20000) | 10000`;489f70(host,1) is notified. Host+1434 and+1440 receive class+6c and+78 vectors. Jeep gunner additionally gets actor+1434 from7c7618 and+1440 from7c75f0. Those word/vector assignments are verified, not relabeled as complete camera ownership.

Actual42d7b0 checks class+724 mask401200 (jeep400000, Driller1000, APC200). When true,40a420 sets host+1a8 bit80000000 and host+7c bits6000000 preserving other bits. Actual helper stores are verified. Physical meaning of these bitfields should reuse existing physics/entity documentation rather than being guessed from one caller.

## Integration consequences

Entry event pulse occurs before attachment and survives attachment failure in original. A reconstructed product-first boarding operation should validate seat availability and ownership before publishing its successful entry event. In particular the outer occupancy query can accept a stale seat handle while427240 refuses it. Do not let a failed ownership change leave active vehicle flags/control publication behind.

Authored L1S2 Driller01 UID8122 and L1S3 APC UID9627 are use-kind1/movement5 targets and both satisfy actual ground-vehicle mask401200 via Driller/APC bits. They exercise vehicle pulse and the final physics flag stage; the authored L12S1 jeep entry event9694 is independently documented by the campaign worker, with Follow_Waypoints9692 downstream. This report does not prove that campaign sequence playable.

The integration entry point is src/diagnostic/scene.c live use dispatch, retaining class/entity structures from src/core/entity.c and src/core/entity_assets.c. Reuse the original primitive seat-attachment report; add a coherent board state machine around it. Current results establish the use action's complete ordering but not subsequent per-frame vehicle movement, weapon fire, camera pose or passenger synchronization. Those are the next highest-impact missing boundaries, ahead of general vehicle polish.
