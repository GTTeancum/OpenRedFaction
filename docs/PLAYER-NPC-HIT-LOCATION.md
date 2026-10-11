# Ordinary player posed hit-location damage

Source-only staged integration, 2026-10-10. Baseline `f6a3d0e202122a94bf5bb7252d42b6bd0a367f5c`. No compile, syntax check, runtime, route or manufactured shot was used to qualify this change. The 21:00 neutral startup pass predates it and does not establish damage behavior.

## Actual owner and missing consumer

The already-used L3S1 guard1 actors 3/892/897/929, guard2 actor 407 and medic1 actor 783 carry authored SP collision-sphere factors. Guard1/medic1 have ordered factors 0.5/1/5; guard2 has 0.5/1/3. These values are loaded from each actual class, never embedded in the gameplay adapter. No UID check, anatomical height band or winning physics-sphere substitution is used.

The prior class constructor resolved these scalars but discarded them on its physical-sphere projection. Ordinary player rays selected flattened movement spheres and then applied unscaled body damage. This change retains the resolved scalar/radius/model-index metadata and uses the actual posed model contact for the original location selection.

## Original anchors

- `48c26d..48c2bf`, reversed `48c09b..48c101`, and `48cb3b..48cb6e`: player-owned kind2 projectile versus actor0 uses linked Use1/self/eligibility admission and reaches model response `49afe0`.
- `49b00f..49b043 -> 46c340`: strict AABB overlap. The projectile bounds span its segment endpoints, expanded by its radius. This is not a segment-versus-target-cube test and does not require a physical target sphere intersection.
- `49afe0/5031f0/54e530`: nearest posed triangle contact, source radius-expanded batch/triangle boxes, front-facing/thin-triangle policy for radius at most 0.025. All eight admitted ordinary profiles have actual radius 0.02. The metadata reader preserves each `$Velocity` and `$Collision Radius`; no query constant substitutes for them.
- `4265ef..426647 -> 4fcea0`: zero spread retains the accepted full basis; positive spread builds the projectile basis from the already sampled direction. Existing RNG draws remain single and retain their prior streams.
- `48bbe0/48c7f0`: actual undeviating four-plane and forward-axis gates, using retained definition flags and speed. Every selected ordinary profile has the applicable bit0x20.
- `42cf5b..42d031`: ordered strict minimum of contact-to-current-CSPH-center distance divided by resolved class radius. There is no inside-sphere requirement. `503290` poses raw CSPH centers and rotates/translates them through the target's current basis/position. Class construction `423dcf..42405d` owns name-prefix overrides and SP scalar selection.
- `4c5d52..4c5d6a`: location factor multiplies fresh base damage before `4c600f -> 4c9b20` penetration attenuation. Nano exits earlier at `4c5a7f..4c5b5d`. Physical shields are independent clutter contacts.

Exact bounded disassembly and selected authored records are retained in the accompanying `hit-location-next` evidence bundle.

## Scope and resource ownership

Only `guard1`, `guard2` and `medic1` receive mandatory new class metadata. Other classes retain the prior proxy/unscaled body path, including model-less and zero-radius classes. Existing movement/stance geometry is unchanged. Class metadata is immutable, 108 bytes per class in the existing bounded NPC allocation accounting, and is freed by the normal/failure body close. It binds the exact class skeleton, ordered actual CSPH index, resolved positive radius and finite nonnegative SP scalar. Raw CSPH data stays in the existing render-model owner.

The eight ordinary profile names are Pistol, Undercover Pistol, Shotgun, Machine Pistol, Assault Rifle, HMG, scoped assault rifle and Machine Pistol Special. Actual table names are the existing `campaign_weapon_names` entries. MP Special is selected through its existing real descriptor, with no new inventory grant. Speed/radius receipts and validity flags consume 216 static bytes. The additive metadata parser does not change any existing weapon, movement, checkpoint or history layout.

Only the ordinary nonprecision PLAYER loop and already-admitted handheld penetration consumer change. AI shots, Sniper/Rail, melee, missiles, radial damage, mounted fire and rendering are outside this slice. The helper reuses current retained posed triangles and collision cache, demanding pose evaluation without advancing animation or normalizing raw authored transforms.

## Query and damage transaction

The accepted ordinary shot snapshots source full handle, actual descriptor/slot/effective mode, base/kind, pellet count, spread, initial origin/basis, speed/radius and definition flags after the existing accepted ammo debit and before presentation/hearing callbacks. Later selection cannot reinterpret that accepted shot. Positive-spread basis and planes are built once per existing pellet sample.

Both forward eligibility and undeviating planes remain anchored to that accepted original pellet origin and basis throughout instantaneous penetration. Only the ray segment/cursor and visited handles advance. This is an explicit port continuation policy, not a reconstruction of original moving-projectile scheduling. The broadphase uses each current query segment's swept AABB; returned hit fractions stay local to that segment, then use the existing cursor-to-absolute conversion for cover and continuation.

A query distinguishes unsupported prior-policy fallback, real rejection, qualified miss/hit and error. A qualified contact keeps the actual local mesh hit long enough to rotate/add it through the same captured target body basis/origin. Raw current posed CSPH centers are transformed into that world frame. Distances are stored as float; division uses double for strict comparison against the stored float minimum, then stores the selected minimum to float. This preserves the documented store order as a portable projection; arbitrary x87 bit identity is unverified. Exact ties retain the first row; no normalization or clamp is added.

The stack receipt retains full target identity, class/skeleton/pose/generation, geometry and admission fields, contact point, factor and selected index. Immediately before body damage, it revalidates source and target registries, current class/model/pose generation, body position/basis/bounds, published target position, linked handle/Use1 status, actual Nano effects flags/armor, class flags and affiliations. The existing shared-charge epoch is only an extra successful local-player replacement fence. It is not represented as a complete actor lifetime generation and does not require charge episode or FP resource admission. No charge reset is changed. Source epoch is checked before any retained target pointer is dereferenced after a damage callback; bookkeeping re-resolves the same full target handle.

Shield selection stays independent and receives unscaled base; an accepted shield hit terminates that contact, including break. If it declines, only an already retained body contact at or before that surface may continue. A shield-only token cannot become a body hit. Nano likewise consumes unscaled base. Body damage then receives `base * selected_scalar`; handheld penetration subsequently applies its existing fresh-base attenuation primitive. No location factor is placed in the generic damage endpoint, and no second AI/difficulty scale is introduced.

All state here is immutable resource metadata or synchronous shot-local data. There is no new save/history lane, delayed action, inventory state, or general projectile owner. Private candidate pose publication uses the existing class/model binding; no constructor defaults overwrite saved actor state.

## Explicit deferred and safety policies

- Source affiliation0 plus target affiliation0 requires the original player's unrepresented AI target560. This overlap stays on the prior proxy/unscaled policy before new geometry/scalar work. Body flag0x400 shortcut and active Nano geometry likewise retain the prior proxy/unscaled policy.
- Self, live linked Use1, forward/plane failure and stale identity are real rejection, not fallback. Missing metadata for a qualified class, missing model or nonzero posed-query errors fail explicitly rather than silently damaging a proxy.
- Existing registered/living/not-hidden/not-deleted/allocated-owner admission remains the port's lifecycle protection. Same-call identity or geometry changes cancel the retained contact rather than guessing a replacement. Nonfinite arithmetic fails; source damage is never clamped or invented.
- The existing instantaneous 100-unit ray and zero target sweep displacement remain port scheduling policies. Original target displacement-times-contact-time and general physical projectile scheduling are not added.
- Runtime damage, location multipliers, penetration continuation, saves/revisits during combat and Xbox arithmetic remain unverified until the parent-owned authorized checks.
