# Generated medic syringe owner

Source-written staging against clean `2122f65db243647baa745fb998d28606e42ff0ec`.
No compilation, syntax check, test, fixture, runtime, image, campaign traversal,
worktree or active-repository edit was performed. Parent owns integration and
scheduled Xbox verification. This is not a claim of working runtime behavior.

## Original-source basis and bounded scope

- Constructor `423a85..423aba` recognizes case-insensitive `medic1`, finds real
  clutter class `syringe`, creates type4 with empty name and identifier `-1`,
  and stores its actual returned handle in hand1 (`1460`). It does not invent
  a pickup UID or eligibility sentinel.
- Real table class is `syringe1.v3d`, material metal, life50, flag `can_carry`.
  Parser flag is `0x80`, not collectable1 or collide_weapon/object6.
  `41055d..410572` sets physics0x20 only for class flags6. This implementation
  leaves all ordinary projectile, firearm and object-contact scans unchanged;
  the real syringe does not receive invented collision spheres/flags.
- `420f90` resolves hand1 and `hand_right`, publishes posed world position and
  object/current/next physics matrices. It does not unlink the held child.
- `48a570/48a660` recursively mirror Hide/Unhide. `4b9c7c..4b9ca9` marks held
  clutter for deletion and clears the held slot, even for an actor without a
  weapon. No free-world syringe-drop simulation is added.
- `4109d4..4109fd` always inserts the generated type4 into clutter family
  `5c9360`. `488ecc..488f19` scans this family, hidden test and AABB, without
  a held/carry/physics0x20 exclusion; `489010 -> 489532 -> 410270` applies
  ordinary clutter damage. Independent review supplied these bounded addresses.
  The generated population joins the existing shared CF5 world cover, ordinary
  radial falloff, class factors and health/deletion service; it is never added
  to authored clutter records or their checkpoint/trigger UID namespace.
- No bounded source evidence establishes initial-death syringe deletion.
  The child follows retained dying actor and UID-matched transferred corpse
  poses. Only unavailable owner/model or explicit source removal closes those
  resources as a clearly labeled port resource policy. This preserves its
  independent state/health metadata; it is not converted into a death-drop.
- The actual selected L3S1 medic783 has `none`/`none` held-object tails. A new
  read-only level projection validates the complete retained raw record and
  exposes these two names without inflating every level entity record. This
  bounded owner admits those tails. Other authored held-clutter overrides fail
  admission explicitly; general left-hand/mixed held clutter is not implemented.

## Runtime identity and ownership

`campaign_npc_body.syringe` has the real `rf_clutter_base_owner *`, damage binding,
full parent registration pointer/handle/slot, hand tag and independent residual
lane. The existing generic type4 allocator supplies a real registry generation
and internal descending runtime UID. That internal UID is never used as an
original authored identity, mission target, pickup identity or persisted key.
Parent authored UID plus the dedicated hand1-syringe lane identifies history.

Live resolution verifies the exact source registration and entity pointer,
child registration, class pointer and shared-model identity. A dropped,
destroyed, stale or unavailable child cannot qualify medic Use. Reserve0 never
calls child deletion. Radial damage preserves original health factors and emits
no invented debris, explosion or pickup. State2 closes its marked allocation at
the next syringe tick; state2 metadata remains after that close.

Actor/corpse tick updates the real static child pose after both source pose
lanes. Live actors use their published position; transferred corpses use the
retained model owner's world transform after checking corpse UID/model. Child
model/material rendering uses existing static retained/CPU fallback policy and
existing temporary renderer buffers. No new render scratch allocation exists.

## Independent persistence interface

- State0 / health+0: untouched constructor default
- State1 / exact finite health `(0,50]`: explicitly retained present child
- State2 / health+0: deleted absence

Negative zero and nonfinite/out-of-range payloads reject. Reserve and child
history are intentionally independent. Legacy default reconstructs the real
body only for a qualified available medic. Deleted absence never reconstructs.
Retired rows keep lane metadata but do not create a child without an owner pose.
A saved-alive candidate requires the currently constructed parent registration
and available model. The normal fresh-world load satisfies this; a standalone
retired-to-alive call without reconstructing its parent is explicitly unsupported.
This interface does not claim general actor resurrection support.

`scene_medic_syringe_capture(owner,&state,&health)` reads the lane.
`scene_medic_syringe_restore_prepare(owner,state,health,retired,budget,&candidate)`
validates before allocation, creates a hidden body in a PRIVATE intrusive list,
and retains exact candidate metadata. It does not demand/evaluate the live pose.
The candidate owns a real temporary registry entry; unsuccessful admission
consumes but recycles its generation/FIFO entry and internal runtime UID. It
never changes the old child, reserve, pending heal or live object list. A
candidate allocation is excluded from all live array-based render/damage scans.

RFNC must re-pose `candidate.child` from its private saved parent/tag matrices
before publication, as implemented by the separate codec worker. It then calls
`scene_medic_syringe_restore_validate` before any parent row assignment. This
proves full source identity, old child closability and candidate private-list
membership/class/ref/hidden/health state. No generated-child mutation callback
may run between that validation and publication. Parent retirement may unregister
only the source in that interval; the candidate's retired flag permits it.

Only after all fallible world publication and final successful storage close,
`scene_medic_syringe_restore_publish` closes the prevalidated old allocation,
moves the candidate node into the live list using void intrusive-list primitives,
and publishes the lane and pointer. Shared close has no external callback:
its only possible admission failures are the preflight registry/link/model-ref
invariants. `restore_discard` closes only the private allocation on failure.
The separate codec owns reserve joining, RFNC/RFCH versioning, stage accounting,
source-pose rebind, finite history and success-only transaction retirement.

## Bounded stock-memory ownership

- One shared model/geometry/material bundle only if a placed `medic1` exists.
  Combined resource/load workspace is capped at256KiB. The large model-file
  directory is heap-allocated, never copied to Xbox stack.
- All generated live AND private candidate body allocations together are capped
  at128KiB. The allocator's predicted retained/scratch peak is checked against
  remaining aggregate capacity and caller budget before it allocates anything.
- Each restore candidate receives the codec's remaining composition budget;
  exposes `allocated_bytes` and `peak_bytes`; live constructor limit is64KiB.
  The separate codec counts candidate allocations within its2MiB restore stage.
- On32-bit Xbox `scene_medic_syringe` adds56 bytes per retained NPC slot,
  already counted by the existing640KiB NPC-body composition. A private
  `scene_medic_syringe_candidate` is64 bytes per relevant staged row, counted
  by the codec. Those figures are source-layout arithmetic, not measured build
  telemetry. Children use one registry slot each, competing inside the existing
  shared1024-object capacity; no capacity or unbounded owner array is added.
- Texture pixels move once into the existing combined renderer materials owner;
  instance rows/model geometry remain with the syringe resource. Children close
  before class/resource/list teardown; shared model refs must reach zero.

## Integration files

Copy the four new `scene_medic_syringe_{types,owner,blast,draw}.inc` files.
`integration.patch` contains source-only hunks for level projection, shared scene
hooks and scripted disarm. Full staged versions are included for source review;
do not overwrite the action/codec worker's scene changes. `TO-DO.fragment.md`
is the proposed open milestone. There are no generated build/game artifacts.
