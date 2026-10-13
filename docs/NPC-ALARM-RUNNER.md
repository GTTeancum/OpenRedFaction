# Authored Alarm runner

Status: source-written, externally staged, uncompiled. No worker builds,
functional tests, fixtures, campaign routes or images were run or produced.
The parent owns the hourly stock-64-MiB compilation/package/startup smoke.

## Operational slice

A surviving local-player damage reaction can now send an eligible authored
Waiting humanoid to its real Alarm UID. Admission requires a living registered
on-foot actor with positive ordinary movement speed, an unretired registered
Alarm46, no active global alarm and no existing live runner for that alarm.
Accepted scripted commands, suppression, seats, hidden/dead actors, independent
combat/patrol/flight/medic ownership and scripted physics are excluded.
The initial slice does not add sight acquisition or broaden combat targeting.

The actor uses the existing bounded navigation request and ordinary swept
movement. Only final destination arrival advances to facing. It turns toward
the event's authored forward vector and then starts its actual mapped action44,
`hit_alarm`, once. The existing animation clock proves natural completion;
removal, cancellation or replacement is never proof that the action finished.
Culled models advance only their owned finite action44 cursor, so this gameplay
owner cannot remain stuck behind visibility culling.

After completion the runner retires its receipt and returns to Waiting before
normal runtime event dispatch. Existing Alarm46 handling owns linked-NPC wake,
17-second global alarm and audio. Actor fields are not written after linked
callbacks. Dispatch failures are surfaced and prevent save omission. A global
alarm raised during travel returns this bounded implementation to Waiting;
original secondary flight fallback is deferred. Optional action44 Foley is
also deferred; the existing actual alarm sound remains operational.

Existing accepted-command cancellation releases the runner's route and owned
clip. Play_Animation replacement keeps its newly installed clip, including an
identical clip ID. A later accepted pain animation interrupts an already-started
alarm action; travel retains ordinary pain handling. No new controller, actor,
route, animation-slot or motion-cache pool is introduced.

## Save and publication

RFNC31 appends a conditional 16-byte receipt: present, authored alarm UID,
phase (travel/facing/action44), started. Absent rows use four bytes. Older
versions retain their existing layouts. The writer only raises the version for
an actual runner. Existing row, whole-save, staging and motion-cache ceilings
are unchanged; allocations are charged by their real sizeof/request counts.

The receipt requires living action16, a compatible ordinary movement lane and
mutually exclusive gameplay ownership. Travel joins the existing saved move to
the exact authored alarm position and route goal, with borrowed graph nodes
resolved through the normal route path. Facing/action44 require arrival-range
placement. The action phase joins the exact selected action44, positive slot
weight and a pre-completion cursor. The one-frame natural-end receipt is
runtime-only and refuses save until the next runner service dispatches it.

Capture and restore reject duplicate claims for one alarm. Restore joins the
actual loaded actor/class/alarm identity, candidate movement, mapped resources,
body placement, owned playback and the candidate event row. Retired or queued
candidate alarm events are refused. Component-only restore and section handoff
refuse active runners; same-level composed save is the supported continuation.

Outgoing and incoming owners participate in the existing resource and final
native-storage-close publication transaction. The four future motions (idle,
walk, run, action44) fit the mutually exclusive existing per-actor request
allowance and the fixed 56-request publication seal. The saved receipt, move,
script clip, AI action, body, selection, playback, graph borrows and payload
identities are sealed before publication. Only final success publishes the
incoming receipt or clears the outgoing one; load does not replay an alarm,
restart action44, perform navigation or consume RNG.

## Original evidence

Read-only RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- `407fb0 -> 4085f0 -> 4087a0`: damage-flight admission honors no-flee and Goto
  suppression before trying the actor's authored alarm.
- `4087a0..4088ef`: positive movement speed, global-alarm refusal, authored UID
  resolving to type46, live peer/action16 duplicate suppression, real route
  admission, ordinary stance, action16/substate1 and cleared `530:200000`.
- `4049b0`: action16 travel dispatch and arrival substate10.
- `404a30`: event forward target, eye-height facing, one-shot action44 latch,
  completion query, event activation and return to Waiting2/state0.
- `589444`: little-endian `33 33 73 3f`, the 0.95 facing threshold.
- `4ba8f0` and `4b0590`: linked wake and existing global alarm consumer.

Targeted read-only L6S1 authored records include tech1 UID4556 -> Alarm5144,
guard4554 -> Alarm5021 and admin_fem4564 -> Alarm5123. The implementation uses
actual records and mappings, with no UID/class-name allowlist. The original
broader combat/sight eligibility and exact navigation/secondary-flight behavior
are outside this bounded consumer; no retail-parity claim is made.
