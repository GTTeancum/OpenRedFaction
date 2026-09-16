# Future vehicle RE index

Ahead-of-implementation research; main thread owns current gameplay implementation. No builds, emulators or shared source edits by this worker.

- Completed: [Exit candidate admission](FUTURE-VEHICLES-EXIT-ADMISSION-20260915.md). Original4279d0 prefix,48 cases, five ordered candidates and blocked exit without state mutation. P1.
- Active:427380 seat/host detachment and4279d0 commit effects; retain exact callback order and ownership fields. P1.
- Queued: board/entry use dispatch and seat assignment; player possession/control/camera handoff. P1.
- Queued: vehicle physics and weapon routing gaps beyond already recovered class tables/contact effects. P1.
- Queued: destruction/ejection and vehicle-specific damage lifecycle. P1.

Existing reused evidence: ENTITY.md occupant predicates; LEVEL-ENTITIES.md use-kind/class/movement parsing; DEATH-LIFECYCLE.md physics/contact/APC/Driller effects. Campaign event/cutscene worker owns general scripting and event activation, so this worker reports vehicle-specific callback boundaries without duplicating those handlers.

- Completed: [Seat release ownership](FUTURE-VEHICLES-SEAT-RELEASE-20260915.md).64 full427380 cases; first seat mutation, stale-handle partial failure, detach audio and player movement fallback. P1.

- Completed: [Common seat attachment](FUTURE-VEHICLES-SEAT-ATTACH-20260915.md).80 full427240 cases; last tag selection, occupied rejection, host/tag assignment, velocity clearing and player mode10 fallback. P1.

- Completed: [Player exit policy](FUTURE-VEHICLES-PLAYER-EXIT-POLICY-20260915.md).64 original4a1970 linked-host cases; policy veto, local event flag/feedback and checked detach result. P1.
