# NPC melee physical-shield adapter

Prepared October 10, 2026 against frozen source `2e7bc44a`, independently
source-reviewed, and integrated only after the failed 03:00 build completed.
This adapter was not part of that build. It remains uncompiled and
runtime-unverified until a later hourly batch. No extra test was run.

## Original evidence and the port boundary

Read-only installed RF.exe SHA-256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
The following addresses were inspected directly in that file for this work:

- `0040f360` passes string `0059405c`, whose bytes spell `riot_shield`, to
  class lookup `00410b60`; `0040f36a` stores the result in `005afb78`.
- `0048c2c7–0048c302` handles projectile-versus-kind-4 candidate admission.
  For that exact shield class it excludes a projectile's own shield owner and
  the owner's related object at `+200`, then falls through to ordinary
  projectile eligibility at `0048c306`. It does not exclude all other shields.
  Radius-dependent model-response selection follows at `0048c316–0048c334`.
- Melee flag `0x20` is tested at `004c7ed7`; `004c7ee3–004c7f28` constructs
  displacement from authored velocity multiplied by lifetime. The bounds are
  expanded using object radius at `004c7f42–004c7f60`. Candidate pairs then
  reach model/general collision at `004c7fc8` / `004c7fd6`, and accepted
  object contact reaches `004c4b50` at `004c8000`.
- Object-contact dispatch passes the weapon damage kind and owner attribution
  into generic damage at `004c610c–004c6132`. Clutter damage at
  `00410282–0041029c` multiplies the incoming amount by the authored damage
  factor before health subtraction. Shield death has a special branch at
  `004102b0–0041036c`; the NPC side calls weapon removal `004031a0` at
  `0041034b`, rather than ordinary prop destruction.
- The separate actor projectile eligibility path at `00494e23–00494e62`
  checks selected Riot Shield, its live clutter owner and negative
  dot(actor-forward, projectile-travel). This is the source of the existing
  directional shield-ray policy. It does not by itself prove that every
  swept melee shield contact in the original uses this ray-only admission.

Read-only `Installed_Game/tables.vpp` archive bytes at offset 1013760, size
108890, contain weapons.tbl. The real Reeper Claw, Baby Reeper Claw, Drone
Smash and Mutant Attack 1/2 entries declare `melee`, `from_eye`,
`no_fire_through`, bash damage and radius `0.5` (lines 1942–1963,
1982–2003, 2175–2194, 2236–2256, 2268–2287). They retain different authored
velocity/lifetime reach and AI attack ranges; those are not interchangeable.

These facts support routing melee into real shield durability. They do not
establish exact swept-shape, contact-order, reach or direction equivalence.
This adapter intentionally uses the port's current finite normalized AI-range
ray and existing physical shield queries/commits. No radius expansion,
frontal invulnerability cone, swept limb, invented shield ownership or
rewritten collider is introduced.

## Practical consumer

The existing geometry-only campaign_enemy_melee_contact remains unchanged.
The proposed scene_ai_melee_shield.inc adds an actor-paired production entry:

`campaign_enemy_melee_actor_contact(stream, pair, start, delta, reach,
player_eye, damage, kind, valid, context, contact)`.

The pair retains source slot/full handle and target slot/full handle, with
UINT32_MAX target slot denoting the actual existing player. Registry wrapper,
entity view, damage owner and body qualification must agree; source and target
must still be live. There is no actor enumeration, affiliation change or
current-target substitution. An ordinary accepted shot retains its original
ray and damage request across presentation. Independently scheduled delayed
contacts use fresh current geometry against the same accepted target.

For NPC targets, a selected actual shield holder is demand-prepared before
its shield query. Without this, the hand-placement query could read old pose
matrices and the subsequent exact-body veto could update them, making cover
fraction and commit geometry disagree. This is pose evaluation at current
animation time; no second animation step is added.

Selection retains existing policy:

1. Query the intended body with the current bounded segment.
2. Independently query that actor's real held physical shield over the full
   segment, even if the coarse body missed. An NPC's current posed body
   triangles can veto a shield behind its actual body through the existing
   scene_ai_shield_ray_select service. Player shields retain the existing
   unprojected prepared-view shield-triangle policy and its priority over
   overlapping coarse player body spheres; there is no new exact player-body
   triangle test. This is not a claim of exact nearest anatomical contact.
3. Select a strictly nearer eligible current-pose prop before that chosen
   actor/shield fraction. The existing helper chooses retained prop geometry,
   not authored initial positions or bounds-only fake cover.
4. Resolve world and detached rubble with the existing damaging fragment
   helper clipped to that nearest candidate. A wall, rubble, stale prop,
   protected prop or broken prop consumes the contact. No second recipient
   is damaged after cover has absorbed it.
5. After full pair/caller-guard requalification, commit only the selected
   physical shield with unchanged AI-scaled base and damage kind. Preserve
   original complete endpoints and NPC candidate query limit `1`; using
   `hit.time` as the commit limit would reject the same strictly-nearer
   triangle. Any accepted shield contact consumes the impact, including the
   one that breaks it. Existing ownership retirement/fallback remains sole
   authority. Excess damage does not spill into the body.
6. Only a successful shield API return (`RF_OK`) with `accepted=0` may
   continue to its retained real body intersection with unchanged accepted
   identities/geometry. A stale pair/guard consumes without body damage; a
   nonzero shield API status propagates as an error, never pass-through. Re-run prop/world/rubble
   arbitration to the original body fraction, without retargeting or a new
   shield query. The first pass reached commit only if no cover damage
   happened, so this longer fallback cannot double-apply cover damage. A
   shield-only intersection never manufactures body contact.

Strict prop tie and full-endpoint behavior remain inherited: an exact tie is
not a nearer prop; a prop exactly at full ray t=1 is not newly admitted. NPC
shield/body ordering and player overlap policy remain their existing distinct
services. There is no blanket rule that every forward attack is blocked.

## Callback and lifetime ownership

The required validity callback is read-only. Shared guard data retains actor
full identities, body state, eye/basis transforms and source selected weapon,
actual ownership, action/order and pending script state. Ordinary callers
capture before presentation, requalify immediately afterward, and reacquire
source/victim handles before post-shot bookkeeping, even on shield-only or
breaking contacts. Actor body damage also uses retained handles and
post-callback target reacquisition before death handling.

The delayed wrapper adds the existing pending slot ticket/phase3 check and
live/order/geometry predicates before any possible cover or shield mutation.
The original due bit is already consumed; reservation/cancellation/servicing
rules remain unchanged. Existing post-contact guards remain in place.
No new timer, save field, ammo rule, cadence update or callback reservation is
introduced. Each independently due impact gets the same full primary base.

The player shield query borrows already prepared current-frame skinning
matrices. The concrete clear-cover path performs read-only geometry sweeps
and no gameplay callback or animation advance before shield commit; any
cover mutation is terminal. Thus the queried player pose remains valid
without a second animation step or a 50-matrix stack copy. If future cover
services introduce callbacks on their clear path, exact prepared-pose
retention/requalification must be added before relying on that assumption.

## Files and integration

- `src/diagnostic/scene_ai_melee_shield.inc`: paired adapter, retained identity/
  geometry guard, clipped terminal cover and existing shield API consumers.
- `src/diagnostic/scene_ai_melee_delayed_guard.inc`: thin existing ticket/live/
  geometry callback for delayed impacts.
- `INTEGRATION.md`: minimal include and ordinary/delayed call-site snippets,
  including safe ordinary epilogue reacquisition. Active scene.c is untouched.

Proposed top-level milestone only after parent integrates:

> NPC melee physical shields: actor-paired ordinary and delayed contact now
> queries real held player/NPC shields with current-pose cover ordering,
> generation/guard requalification and consuming break hits; source-written,
> uncompiled/runtime-unverified, with swept-radius and exact original parity
> deferred (docs/NPC-MELEE-PHYSICAL-SHIELDS.md).

Do not mark this milestone integrated before applying the snippets. Existing
geometry-only tests remain unchanged and confer no coverage on this adapter.

## Deliberately outside this slice

Incidental actors, melee penetration, vehicle/rider melee, original radius-0.5
sweep, exact retail reach/contact order, swept target/prop motion, new shield
impact audio, new actor/item grants and extra validation fixtures remain out
of scope. No build, test, syntax check, emulator, route, campaign traversal,
image/capture, asset/original-input change or active-repository write was done.
No runtime blocking, durability loss, shield break, cancellation or fallback
result is claimed. The staged files require parent review/integration and a
future authorized consolidated Xbox batch.
