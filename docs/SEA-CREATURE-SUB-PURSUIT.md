# Sea Creature sub-mode pursuit: bounded XYZ first pass

Status: integrated after the completed 05:00 batch, based on clean `5ea4bb0f`; independent source
review complete with no blocker. Parent owns shared-scene integration and the scheduled Xbox
build. No compilation, syntax check, test, gameplay fixture, emulator session,
PC build or campaign route was run for this slice. Runtime remains unverified.

## Scope and actual owner

The ordinary NPC movement loop discarded Y for every approach, including an
actual `Sea_Creature` whose authored movement descriptor and current descriptor
index are both `7` (`sub`). It also refreshed pursuit targets and advanced route
points using XZ alone. The bounded consumer in `scene_submode_pursuit.inc` and
its parent hooks retain XYZ for this exact class/mode combination only when
the existing `script_move.follow == 2` combat pursuit owns movement.

Actual original owners already documented by Sonar combat inspection include
L5S3 UID 4645, L10S2 UID 6810, L10S3 UIDs 6795/6808 and L10S4 UID 7013.
The authored class has speed 8, acceleration 20 and mass 5000. Those are
evidence, not new constants: the movement proposal consumes the owner's
existing resolved `movement.speed`, including admitted speed selections.
No spawn, grant, affiliation change, AI mode change or fixture is introduced.

`follow == 2` is the existing ordinary-NPC combat movement path. It can serve
ordinary player awareness, an authored Attack target or an acquired opposed
actor. This change preserves all these existing target producers; it does not
exclude every nonzero `combat_scripted` value or select a new target itself.
Explicit Goto, Goto_Player (`follow == 1`), Follow_Waypoints, Look, standalone
combat aim and other movement modes retain their prior behavior.

## Original evidence and coordinate distinction

Read-only inspection of the installed original `RF.exe` establishes:

- `40ba04` supplies actor eye position `actor + 0x7d4` to the follower.
- `40bb2c..40bb44` subtracts that eye from the current route point and passes
  the complete XYZ vector to `408e50`.
- `408e54..408e7b` normalizes, scales and accumulates all XYZ components into
  the movement command at AI `+0x500` (actor `+0x7a0`, via `40a350`).
- `40bbc7` samples the eye for arrival. `40bbdc`/`40bbea` check free/swimming
  mode; `42a0a0` accepts descriptor index 4 or 7. Only when neither predicate
  applies does `40bbf6` replace sampled Y with route-point Y. Thus sub7 keeps
  full eye-to-route XYZ distance at `40bc17..40bc1d`.
- Separately, `40b8c4..40b910` replaces the movement-steering target Y with
  the owner's eye Y before `40cb50`. Flattened steering is not evidence that
  translation or arrival should discard Y.

This implementation deliberately does not claim exact eye-space reconstruction.
The current port's target producer stores player/NPC body positions, or the
existing vehicle aim position for a vehicle target. Its route start and
candidate destinations are already consumed as body-space destinations.
The original final pursued-target producer has not been reconstructed here.
Subtracting the source eye from an unchanged body destination would introduce
an unsupported source-eye/target-feet offset.

Parent-selected first-pass policy therefore retains the existing target and
route storage, uses current body XYZ consistently for route/final arrival and
translation, and refreshes stored targets by their full XYZ change. Route
selection still starts from the same current body position. There is no guessed
eye-height correction, coordinate conversion, handcrafted path or new schema.
The original-backed principle is retention of vertical pursuit; the exact
body-space adaptation, direct kinematic stepping, existing 1-unit refresh
threshold and 0.25-unit arrival radius are explicit port policy.

## Narrow consumers and collision ownership

1. Pursuit refresh includes target Y change for the exact class/current-mode
   predicate. That predicate intentionally does not require follow2 because
   this function also starts a pursuit that was not previously active.
2. Movement admission requires follow2, the authored/current sub7 class,
   live entity and full registry identity, positive health, no hidden/frozen
   object flags, no terminal entity flag and no linked owner. Existing top-level
   seat and suspended-scripted-physics checks remain in place. An exact-sub
   actor failing these checks stays still rather than falling back to XZ.
   The entity bit `flags_810 & 1` is terminal/death state, not a pain timer.
   Existing nonterminal pain behavior is unchanged; this adds no movement
   pause or new timer interpretation.
3. Route waypoint arrival and final arrival use the same body XYZ distance as
   the proposed displacement. A waypoint above or below the actor cannot be
   skipped merely because its XZ coordinates match.
4. The step remains `min(resolved_speed * elapsed, distance)` and uses the
   ordinary full-body `0x460` sweep. Existing authored navigation clearance,
   route acquisition/retry policy and collision spheres are untouched.
5. The ground-only uphill projection and horizontal wall-slide retry are
   bypassed for this admitted pursuit. A blocking full-body contact rejects
   the proposal through the existing blocked path. No sphere shrinking,
   clearance waiver, endpoint teleport, partial-contact bypass or new slide
   solver is introduced.
6. Only after an accepted sweep, movement steering refreshes the owner's eye,
   copies the movement target locally and assigns the copy's Y to the current
   eye Y. The stored destination and XYZ displacement are not flattened.
   Existing angular preparation, ordinary commit and publication remain the
   owners. Explicit Look and standalone combat aim are unchanged.

`campaign_script_ground` and `campaign_npc_idle_ground_step` already skip mode7
because they admit only ground/falling modes 1 and 3. They are unchanged.
No new gravity, buoyancy, drag, acceleration, player-swim control, room/liquid
transition, wet-only restriction or general swimming engine is added. In
particular, authored movement mode `sub` does not imply class flag `sub`:
Sea_Creature lacks that separate flag and lacks `water_only`.

## Persistence and remaining work

The adapter owns no new persistent state, timer, projectile pool, save row or
resource allocation. Existing XYZ position, movement mode, speed and pursuit
target storage remain with their existing owners. No checkpoint capture,
restore, airborne/sub admission, velocity constraint or placement-clearance
check is changed or relaxed. This is not a claim that unsupported live
sub/swimming saves now work; their existing rejection policy remains.

The separately implemented Sonar attack, its finite reserve policy, liquid
contact expiry, visual lifecycle and save-only pending guards are unchanged.
Natural vertical pursuit, exact arrival behavior in retained authored routes,
collision-blocked response, control interruption and all save/load behavior
remain runtime-unverified. General mode7 physics and exact original eye-route
semantics remain deferred rather than implied by this kinematic first pass.
