# Tankbot autonomous Missile owner

Source-written against `805e5689a8a61f482f8f45ea11dd22bbef090b77`,
written October 10, 2026. Independently source-reviewed and parent-integrated; compilation awaits the 12:00 Xbox batch. No compilation, syntax check, gameplay fixture, route, emulator run,
or save/action runtime result is claimed. Parent owns the scheduled Xbox batch.

## Original consumer and existing implementation

- Ordinary non-Fighter combat `405af0` invokes secondary `406330` at `406294`.
  `406330/406390/426ca0` own readiness, visibility, an independent deadline,
  authored range, blast-plus-body clearance, forward dot at least 0.95 and LOS.
- Original `levels2.vpp/L7S4.rfl` Tankbot UID10696 explicitly selects Tankbot
  Chaingun / Tankbot Missile, authored mode1 and affiliation0. The supplied
  original-level inspection found no hidden/seat assignment or direct
  disabling Set_AI_Mode for this owner. `entity.tbl:4034` marks it sentient.
  These observations justify an ordinary acquired-target consumer, not an
  automatic alert or a fabricated Attack event at startup.
- `weapons.tbl:2633–2660` supplies Tankbot Missile's range150, fire wait4s,
  capacity36 of `15cm rocket`, no magazine, speed15, lifetime6, explosive
  damage25, blast3 and crater5. Existing resource demand and selected-owned
  loadout already admit it. No grants, replacement weapon or new resource
  profile are introduced.
- Queued Shoot_Once mode1 already launches through
  `scene_ai_tankbot_missile_launch`. Original `4badd5–4baddd` resets the
  secondary deadline before its call to `426ca0`, so forced requests must
  retain priority over an autonomous cooldown. No special-weapon target
  release is added; the separate ordinary-primary target-release slice is
  unchanged.

## Autonomous admission

`scene_ai_tankbot_secondary.inc` uses the existing shared
`rf_weapon_ai_secondary_admit`. It requires the actual held, owned Tankbot
Chaingun / Missile pair, current resource admission, positive finite Missile
reserve and a living generation-qualified source and already acquired target.

Ordinary combat mode0 retains the acquired player. Explicit/reactive modes1/2
retain the actual `combat_target` handle. Only actual on-foot player/NPC
bodies are represented. A vehicle, occupant or point is not converted to an
unrelated actor. Existing acquisition, affiliation, alerting, pursuit, primary
selection and target-release policies remain unchanged.

Source death/hidden state, full registry/entity identity, linked/seat/turret
ownership, AI mode, holster, scripted animation, active Shoot_At, queued
Shoot_Once and pain lock all retain their existing autonomous suppression.
The existing Drone identity, body-geometry and world/clutter sight helpers
are reused because those helpers have no weapon-specific admission. The new
owner does not call Drone's weapon selector or alter Drone deadlines.

The shared admission computes the original approximate distance metric,
uses loaded range150 and blast3 plus both body radii, and requires current
3D eye-to-target/body-forward alignment and unobstructed world/mover/clutter
LOS. Target identity/order, geometry and pain are requalified after LOS;
a reset epoch also rejects a replaced timeline even when geometry matches.
No unseen-fire flag is synthesized.

Dispatch follows genuine acquisition and target resolution but precedes every
primary ammo, cadence and range exit. An accepted Missile neither consumes
Chaingun ammo nor postpones `combat_due`, resets a burst, changes selection,
starts primary animation or skips the remaining primary branch. Primary
exhaustion/fallback and selection remain their current owners' policies.

## One accepted-only deadline for both paths

Two runtime-only fields in each allocated NPC retain the full source handle
and wrapped millisecond deadline. This adds 8 field bytes per actual NPC;
existing `sizeof(*campaign_npc_bodies)` startup budget/accounting includes the
structure growth. There is no separately exhaustible cooldown bank and no
new bank-full refusal of otherwise eligible queued shots.

Both launch paths call `scene_ai_tankbot_secondary_launch`:

- Autonomous launch checks the existing deadline and exact-pair admission.
- Existing queued mode1 bypasses the deadline. It keeps its established
  FIFO/life/hidden/holster/seat gates and remains before ordinary combat.
  Its exact registry/entity identity check requires no allocated physics body;
  the preexisting launcher needs only its registered eye/ammo owner.
  Forced calls do not acquire autonomous range/aim/LOS/pain/AI-mode gates.
- The forced path deliberately does not require Chaingun as primary. Every
  accepted existing queued Tankbot Missile launch refreshes its source's wait,
  so a later legitimate primary selection cannot erase that accepted wait.
- The wrapper calculates4000ms, calls the unchanged finite launcher, then
  publishes the new deadline only after RF_OK. No queue enqueue, expiration,
  empty reserve, invalid ownership or full eight-flight-pool refusal changes
  the old deadline. The original single-shot queue and reserve debit remain
  in their current owners.
- A launch-in-progress guard prevents nested publication and saving while a
  launcher callback is active. Reset does not reopen this guard. After return,
  the wrapper re-resolves the full source and checks the reset epoch before
  publishing; it never dereferences a stale pre-callback owner.

Service before ordinary enemy early-outs retires elapsed waits even during
combat suppression. It clears an old-generation field if its slot now has a
different source handle. Target, order, primary selection, hidden state, pain
and holster changes do not clear a living source's timer.

## Save and lifecycle boundary

RFNC has no independent secondary-deadline representation. The new
`scene_ai_tankbot_secondary_save_pending` is read-only and blocks only actual
save/export while a launch or unexpired wait exists. It is added to the
existing NPC export, composed player/terrain save and world snapshot guards.
It is not added to NPC row capture, projectile capture, candidate construction,
NPC assign or any load preparation. Failed save or preparation cannot erase
or postpone the wait.

The proposed hooks reset at normal NPC teardown, player restart, fresh combat
startup, successful standalone NPC restore commit and final successful world
load. Legacy composed-player admission rejects every nonempty NPC population
(`scene_checkpoint_player_scope_mode`), so it cannot own a Tankbot wait and
receives no new reset. World reset is after
storage close under `!status && world_published`, not during NPC assignment.
No per-source target/selection cleanup gets a reset.

RFAP already represents Tankbot missiles as variant2. Its layout, capture,
restore and source attribution are unchanged. This slice adds no active-flight
save veto: after4000ms, a still-flying six-second Tankbot missile remains
eligible for the existing RFAP save path if its other gates admit. Successful
load restores represented Tankbot flights through their current owner; the
new reset retires only the unsaved timer. No Drone-only flight cleanup is
extended to Tankbot.

## Explicit first-playable approximations

Existing `combat_alert` plus the live gates is the readiness approximation;
the optional original ready-action transition and its presentation timing
are not reconstructed. A newly acquired, aligned and unobstructed target can
admit its first Missile immediately, independently of primary reaction timing.
Ordinary unalerted acquisition remains its existing20-unit policy, even
though secondary range is150. Original L7S4 startup alone is not proof of an
autonomous launch; the player starts about48.69 units behind the Tankbot.

Launch remains the already implemented eye-origin straight flight. No inferred
secondary hand transform, predictive lead, homing, spread, new launch/flight
audio or primary animation is introduced. Existing Rocket-style impact,
explosion/terrain, collision and Nano/physical-shield owners are preserved;
Tankbot's exact original muzzle, separate launch/impact presentation and audio
remain deferred. See `SCRIPTED-SHOOT-ONCE-FIRST-PASS.md` for existing launch
coverage; that historical queued-shot result does not validate this new timer
or autonomous path.

## Proposed parent hooks

The external staging deliverable includes a complete patch for the new owner,
the two NPC fields, forward declarations, include after Drone secondary,
pre-loop service, forced queued wrapper, post-acquisition autonomous dispatch,
three save guards and all successful lifecycle resets described above.
`rf_scene_tankbot_secondary[12]` records attempts, accepted autonomous/forced
shots, gate holds, and latest full handles/frame/deadline/status. Startup
clears its telemetry separately from runtime timer reset.

No original input, active repository, build, tests, new fixture, route,
gameplay run, worktree or publication was changed or executed by this helper.
The staged source remains uncompiled and runtime-unverified.
