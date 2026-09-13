# Firing runtime integration

This is an implementation map derived from the original executable, not evidence
of playable combat. Source SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Verified building blocks

The port has retained NPC inventories, selected animation mappings, held weapon
models/materials, hand tags, muzzle placement and target aiming. The standalone
4257c0 ammunition helper matches original execution. These services do not yet
constitute a firing dispatcher, and muzzle queries must not consume ammunition.

## Dispatcher425830

The original dispatcher gates firing on actor state, current weapon, alternate
mode eligibility, timer state and inventory availability before creating shots.
The full function contains ordinary, continuous, special-weapon and network
branches; reconstructing only a normal pistol branch will not complete it.

The ordinary shot-loop trace establishes these integration requirements:

- Advance primary hand510 and wrap against the primary hand-list count before
  asking41b040 for the shot pose. Class flags can request multiple hands.
- Descriptor7c supplies the projectile count per hand; actor814 bit1000 doubles
  this count. Spread can change each projectile direction through RNG.
- Factory4c77a0 can be called again for a linked projectile. Preserve the actual
  dispatcher predicates rather than treating every factory call as one ammo unit.
- The ordinary post-creation path reaches4257c0 after shot processing. Flag814
  bit1000 can cause a second consumption call. Factory failures and special
  branches require separate execution tests before composing this ordering.

These statements are instruction/decompiler trace findings. Full dispatcher
execution and failure-prefix comparisons remain required.

## Factory4c77a0

`tools/audit_projectile_factory.py` decodes583 instructions over4c77a0..4c8030,
checks the original hash, and records direct calls and a reviewable instruction
listing in artifacts. Its AUDITED result proves static boundary presence and
address order only, not conditional runtime coverage.

| Call site | Callee | Required service |
|---|---|---|
|4c7a11|486da0|Generic object creation: kind2, owner handle and prepared descriptor|
|4c7ae4|4c8030|Projectile model setup when an object model exists|
|4c7b71|4d8ed0|Optional dynamic light|
|4c7bc4|497ca0|Optional attached effect|
|4c7bfd|5056a0|Optional positional sound|
|4c7c9b|48c9a0|Collision registration under the original gates|
|4c7eb3|4c2570|Special projectile setup|
|4c7fc8 /4c7fd6|49afe0 /49a420|Immediate model/ordinary collision branches|
|4c8000|4c4b50|Projectile hit processing|
|4c8006|48ab40|Mark object7c bit2 for later retirement|

Creation starts with a zeroed152-byte descriptor and baseline flags80000870;
weapon fields and predicates modify it before486da0. Allocation failure returns
zero after descriptor cleanup. Successful creation publishes weapon ID298,
descriptor pointer294, source-related flags and position, then conditionally
creates resources and links into the projectile list. Every optional owner needs
matching cleanup; a visible mesh alone is insufficient.

Descriptor264 bit20 reaches an immediate collision branch inside the factory.
After processing,48ab40 sets flag7c bit2. **It does not remove the registry entry
or free the object.** The factory subsequently returns its object pointer. Keep
creation, marking and eventual removal distinct; do not free this object before
the dispatcher's remaining accesses. The existing48ab40 export confirms the
single-bit mutation; actual cleanup scheduling must still be connected.

## Next implementation order

1. Reconstruct and verify the152-byte creation descriptor and generic kind2
   ownership, using retained weapon fields and the existing collision registry.
2. Reconstruct the full factory with explicit optional resource ownership and
   rollback/marking behavior; test immediate-hit and persistent projectile paths.
3. Connect the dispatcher to muzzle/aim, spread RNG, factory, ammunition,
   animation, timers and sound in the original order, including failure exits.
4. Verify a representative shot end to end in stock64MiB XEMU: registry state,
   target damage, ammo delta, timers and cleanup, then present a new visual.

Outstanding scope also includes live player/clutter targeting, replacement-class
hand lists, all campaign weapons and continuous projectile simulation.

## Creation descriptor verified (2026-09-13)

rf_projectile_descriptor_prepare now builds the full152-byte descriptor and
boost decision at486da0. It preserves name-length>4 selection, model token,
raw descriptorbc/ac fields, position/basis, scaled forward velocity, special
spin and flags. Important boundaries: factory accepts weapon==weapon_count,
but4c90f0 spin predicate excludes it; special-weapon spin still applies.
Speed<50 tests the original unscaled speed, even when boost changes velocity.
The scale product is stored as float before multiplying forward components.

verify_projectile_descriptor.py executes original4c77a0 to the486da0 call
with real string/vector/spin/4c90f0 code. Only owner lookup and player/powerup
predicates are supplied, with arguments/order checked.2048 cases match all
152 bytes plus boost on PC and compiled NXDK at027f:250 boosted,1264 spinning.
Five PC/NXDK invalid/nonfinite guards and two NXDK null guards pass. Both
builds and22 CTests pass. Evidence artifacts/projectile-descriptor.json.
This does not allocate a projectile; generic kind2 ownership and all factory
post-allocation effects remain open. No new native scene replay or visual.

## Fixed projectile pool verified (2026-09-13)

Original487100 kind2 checks the50-object cap and calls48b590. The underlying
48b4d0 pool contains50 records of314h (788) bytes. Initialization links them
in increasing slot order;48b610 releases to the head, so reuse is LIFO.
Allocation does not clear payload. The optional original heap fallback is
disabled in the stock-Xbox port pool.

rf_projectile_pool owns39424 bytes:39400 record bytes plus24 bookkeeping
bytes, including live-slot guards. Free links are slot+1 tokens rather than
raw pointers. Acquire/release preserve all payload except the free-link word,
which release overwrites. Raw record storage is reserved for the pending
reconstructed object initialization; it is not yet a complete projectile type
or an allocation in the running scene. Callers must clean resources and
remove registrations before returning a slot. Marking flag2 alone does not
authorize immediate release.

verify_projectile_pool.py runs48b4d0/48b590/48b610 unhooked with heap fallback
off.4096 PC/NXDK operations match original normalized free links, selected
slots, free/live/peak counts and all untouched payload bytes:2034 allocations,
2006 releases,56 exhaustion returns. Four NXDK invalid-release/null guards
preserve pool bytes. Both builds and22 CTests pass. Evidence:
artifacts/projectile-pool.json. Generic486da0 initialization/registration,
model/physics ownership and complete factory effects remain open.

## Projectile registration owner adapter (2026-09-13)

rf_projectile_store combines the fixed pool with50 stable type2 owner
wrappers (40824 bytes on32-bit PC/Xbox). It appends the object list, inserts
the generation-checked registry handle, consumes the UID, and invokes an
explicit initialization service. Successful initialization publishes the
generic400000 flag. It does not zero the reusable original-sized payload.

Failure invokes cleanup after list detachment while registry identity is
still available, then releases handle and pool slot. UID/generation changes
are retained across failed initialization. Flag2 marking does not invoke
close. Close takes a handle, so stale generations cannot close reused slots.
The initialize/cleanup callbacks must preserve identity and list links; no
reentry or registry/list mutation is supported. Cleanup must release partial
resources and cannot fail. The eventual factory must supply those services.

verify_projectile_store.py and the PC projectile_owned_lifetime CTest cover
52 initialization/cleanup cycles, full pool/registry exhaustion, stale handles,
mark-without-removal, and failed initialization after acquiring a resource.
Compiled NXDK callbacks verify visibility and cleanup order. Both builds and
all23 CTests pass. Evidence artifacts/projectile-store.json. The first
Unicorn run exceeded its100000-instruction observation cap in the ordinary
registry memset; raising that test cap to1000000 completed successfully.

This is a port ownership adapter using independently verified pool/list/
registry components, not a claim that full486da0 is reconstructed. Room,
model, sphere and physics initialization and complete4c77a0 resource/hit
services remain required before live projectiles. No new native scene run.

## Moving creation physics verified (2026-09-13)

rf_physics_creation_body_open_moving carries descriptor6c linear velocity and
descriptor78 angular vector through the existing49ec90/49f010 preparation.
The previous static constructor supplies zero vectors through the same path;
the creation seed layout stays unchanged. Generated/inherited mass, fallback
spheres, material coefficients and ownership budgets share the existing code.

verify_creation_body.py --moving compares640 complete original constructor
cases with PC and compiled NXDK, including all308 represented state bytes
and owned sphere records. Only original heap calls are supplied; the fallback
opaque word remains normalized because its original value is unspecified.
Checks cover560 allocation failures,640 short budgets, repeated open/close,
and14 NXDK null/nonfinite motion failures without allocation or owner changes.
The unchanged static mode also passes640 cases. Both builds and23 CTests pass.
Evidence: artifacts/creation-body-moving-verification.json and
artifacts/creation-body-verification.json. No new native XEMU run or visual.

This completes the shared moving physics entry point, not projectile factory
integration. Resolve model radius/spheres and map the creation descriptor in
the registered projectile initializer, then implement remaining factory
resources, collision/hit effects and deferred retirement before live firing.

## Registered projectile model/physics resources (2026-09-13)

rf_projectile_initialized_open composes the fixed pool/list/handle adapter
with generic model attachment, model-derived collision spheres and moving
creation physics. Descriptor name is resolved by the caller; wrapper kind
comes from descriptor word1. Negative radius uses attached model bounds;
without a model the object radius defaults to1 while descriptor radius0
remains0 for physics. A single model sphere is centered after querying it.
Original unspecified temporary sphere words are port-initialized to zero.
The caller descriptor stays unchanged; resolved fields enter the physics seed.
Only prepared projectile descriptors are accepted: no mass grid, initial
tensor or caller-installed sphere list.

Model service errors, missing models, sphere conversion errors, insufficient
budget and physics failures release partial resources before registration
and pool rollback. Close releases physics then model. Flag2 alone preserves
the resources/registration for later factory accesses. The raw788-byte record
is still reserved; these resource owners live beside the50 pool slots.
Each owner is348 bytes:17400 for the array,58224 including the40824-byte
store, before sphere/model storage. Per-slot peak includes scratch and owned
spheres. Model backend allocations require their own caller-enforced budget;
this is not yet the complete scene memory accounting or a loaded model backend.

verify_projectile_resources.py compares12 PC/compiled NXDK integration cases
with supplied model/heap services, checking all represented attachment/body
state and owned spheres. Six successes cover model-less negative/zero/positive
radii and zero/one/two model spheres. Failures cover absent model, load, bounds,
sphere service, budget and nonfinite motion; two additional NXDK allocation
failures exercise temporary and retained sphere allocation rollback.
The harness supplies calloc separately because NXDK calloc depends on real
allocator metadata that a malloc-only stub cannot reproduce.
Both builds,24 CTests,640 moving constructor comparisons and52 registry
lifecycle checks pass. Evidence: artifacts/projectile-resources.json.

No native XEMU scene run or new visual. Remaining: bounded retained model
services, generic common/parent/room fields, full4c77a0 post-creation effects,
collision/hit dispatch, deferred retirement and425830 firing scheduling.
This verifies a port composition of reconstructed components, not the entire
original generic factory or functional combat.
