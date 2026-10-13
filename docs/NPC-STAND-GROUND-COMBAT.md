# Authored Stand Ground combat movement

Source-written 2026-10-12 against `69a614c7401b6ee2fb8abdbc35eab1b70192c221`.
Compilation and short startup smoke remain parent-owned and unperformed for this
slice. Functional coverage belongs to testers. No fixture or campaign route was
added.

## Gameplay consumer

Ordinary living, registered, unseated humanoids with an authored Stand Ground
override retain combat acquisition, target facing and firing while declining
combat-pursuit route creation and refresh. The common combat scheduling block
applies this to autonomous player targets, opposed/reactive targets and explicit
Attack orders. Shot range and obstruction refusal remain the existing policy.

Qualification uses the actual owner/registry handle, existing body/model/pose
binding, ordinary authored/current ground movement, and the immutable RFL actor
record. RFL `attack_style=3` maps to runtime Stand Ground2; raw2 maps to Direct3.
No UID, class-name or weapon-name allowlist is added. Raw0/class defaults are not
inferred. The installed source survey found99 armed humanoid overrides across31
levels, including seven Attack events targeting six actors across four levels;
these are static source counts, not guaranteed runtime admission.

Only a genuine alerted combat route with active follow2 and origin NONE is
stopped. Scripted0 correctly recognizes the registered living player's implicit
target even when the older hearing/damage path left combat_target zero. Explicit
and reactive targets must resolve to a living ordinary actor or an existing
vehicle/turret target. Both mirrored action6 fields are excluded: fixed-point
noise investigation also uses follow2 and is not a combat pursuit.

The early ordinary movement service reconciles a legacy pursuit before its first
translation, after higher-priority medic, retreat, alarm, seat, scripted-physics
and flight dispatch. The shared pursuit block also declines future creation and
refresh. Active/faulted retreat, entered flight, medic/alarm ownership, frozen scripted animation, scripted physics, seats, catatonic action1,
flight7, turret13 and medic14 remain outside this consumer.

The existing pursuit stop sets active/follow to zero, requests the ordinary idle
locomotion transition, and retires the retained navigation window/retry. It does
not change combat alert/target, attack/burst/navigation deadlines, selected
weapons, inventory, reload state, paid attacks, queued Shoot_Once or
Fire_Weapon_No_Anim obligations. Point-target firing is unchanged. The ordinary
stationary aim and idle ground paths remain live; no position snap, spawn pin,
body-velocity clear or support reset is performed. Falling and platform carry
remain physical owners.

Authored Goto/Goto_Player/Follow_Waypoints retain their current movement priority.
An idle or default-patrolling actor is not stopped merely by raw3; only the
existing genuine combat transition can supersede a default patrol. Patrol
resumption and authored cursor/wait/facing/event behavior are unchanged.

## Original evidence

Read-only installed RF.exe SHA-256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- `418c70..418cd3`: names none0, evasive1, stand_ground2 and direct3.
- `464acf..464ae8`, table `59c028={-1,1,3,2}`: placed override translation.
- `406b79..406b80`: pursuit predicate immediately refuses runtime style2.
- `406a1b..406a36`: combat movement still resolves/faces its target, then returns
  for style2 before route reacquisition/advance or direct approach.
- `408cc3`: combat selection installs pursuit only if its predicate succeeds.
- `4bcac0..4bcb8d`: Attack events use the ordinary selector/target setter without
  replacing attack style.

This is the bounded combat translation policy. Full original Evasive tactics,
crouch/burst choice and lost-target search remain separate implementation work.

## Save and stock64 ownership

RFNC36, RFCH13 and RFAP14 layouts, capabilities, candidate validation, capture,
publication, final native-close selection and failure behavior are untouched.
Existing full RFL/tables checkpoint identity already covers the immutable style.
An admitted older pursuit is preserved through restore publication and retired
only by subsequent ordinary simulation; save or failed load never normalizes a
live or staged movement owner for this policy. New standing combat saves use the
existing alert/target/movement representation.

No retained field, timer, pool, archive read, allocation, resource demand, schema,
save tail or budget increase is added. The source reader and target qualification
use fixed temporary locals. No compiled peak-memory or runtime result is claimed.

The actor controller_handle is a mover-activation backlink, not exclusive AI
control (level.c mover activation). It does not disqualify Stand Ground after
an actor opens a door; actual player, seat, attachment and physics guards remain.
