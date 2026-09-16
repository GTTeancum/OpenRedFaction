# Future vehicle RE index

Ahead-of-implementation research; main thread owns current gameplay implementation. No builds, emulators or shared source edits by this worker.

- Completed: [Exit candidate admission](FUTURE-VEHICLES-EXIT-ADMISSION-20260915.md). Original4279d0 prefix,48 cases, five ordered candidates and blocked exit without state mutation. P1.
- Queued:4279d0 final exit commit effects beyond proved seat release; retain callback order and ownership fields. P1.
- Active: secondary weapon scheduler/muzzle dispatch and vehicle physics/control-record consumption. P1.
- Queued: vehicle physics and weapon routing gaps beyond already recovered class tables/contact effects. P1.
- Queued: destruction/ejection and vehicle-specific damage lifecycle. P1.

Existing reused evidence: ENTITY.md occupant predicates; LEVEL-ENTITIES.md use-kind/class/movement parsing; DEATH-LIFECYCLE.md physics/contact/APC/Driller effects. Campaign event/cutscene worker owns general scripting and event activation, so this worker reports vehicle-specific callback boundaries without duplicating those handlers.

- Completed: [Seat release ownership](FUTURE-VEHICLES-SEAT-RELEASE-20260915.md).64 full427380 cases; first seat mutation, stale-handle partial failure, detach audio and player movement fallback. P1.

- Completed: [Common seat attachment](FUTURE-VEHICLES-SEAT-ATTACH-20260915.md).80 full427240 cases; last tag selection, occupied rejection, host/tag assignment, velocity clearing and player mode10 fallback. P1.

- Completed: [Player exit policy](FUTURE-VEHICLES-PLAYER-EXIT-POLICY-20260915.md).64 original4a1970 linked-host cases; policy veto, local event flag/feedback and checked detach result. P1.

- Completed: [Boarding prerequisites](FUTURE-VEHICLES-BOARDING-PREREQUISITES-20260915.md).810 original cases; seven ordered exclusion queries, shared vehicle/turret admission and exact multiplayer policy. P1.

- Completed: [Boarding shield/liquid policy](FUTURE-VEHICLES-BOARDING-LIQUID-DELAY-20260915.md).384 original cases with real room liquid comparison and timestamp setter; no attachment/rendering claim. P1.

- Completed: [Boarding commit orchestration](FUTURE-VEHICLES-BOARDING-COMMIT-20260915.md).16 original cases verify post-admission state, event ordering, local-position capture and ignored attachment return. P1.

- Completed: [Per-frame control ownership](FUTURE-VEHICLES-CONTROL-OWNER-20260915.md).96 original cases distinguish driver host input, jeep gunner aiming, camera overrides and clearing the selected record on input lock. P1.

- Completed: [Vehicle firing ownership](FUTURE-VEHICLES-FIRE-OWNER-20260915.md).256 original prefix cases; host weapon selection, both death gates and jeep gunner-only fire, distinct from movement ownership. P1.

- Completed: [Secondary weapon admission](FUTURE-VEHICLES-SECONDARY-ADMISSION-20260915.md).576 original cases with real deadline/backoff/burst-flag helpers; separate secondary ID and deferred scheduling, no projectile claim. P1.
