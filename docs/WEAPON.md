# Weapon reset

## Loaded weapon-model presentation

`rf_weapon_update_presentation` follows 0x4ae0d0 using a compact state with
player model +34, auxiliary +38, current presentation weapon +1080, pending
selection +f80 and timer +f84. This timer differs from the queue's +b8 timer.
Model values are opaque 32-bit tokens for existing instances; the function
does not create meshes, load assets, animate or render them.

Nonlocal players and invalid indices other than -1 return zero unchanged.
Weapon -1 returns zero without changing player state: its apparent cleanup
callee 0x4a73b0 is a single ret in this executable. The former cleanup callback
was an unnecessary dependency and has been removed. For a valid weapon,
an empty descriptor +40 string preserves and returns
the existing model. Paired 0x85ccd8/0x85cd00 transitions update only +1080,
even when the model is zero. Otherwise a changed weapon clears an existing
model and auxiliary value; an unchanged existing model returns immediately.

With no model, weapon 0x85cce0 uses base 0x87210c, and paired second uses
paired first. The first matching entry in the 32-record cache at 0x7c71b0 wins:
use its alternate pointer for weapon 0x85cce0, normal pointer otherwise.
Without a match, descriptor +48 supplies the model. Null models return
RF_NOT_FOUND at the original fatal assertion boundary; invalid fallback
descriptor indices return RF_RANGE rather than reading arbitrary memory.

Successful installation clears pending +f80 and timer +f84. Descriptor +64
other than -1 enters 0x50ce00, which calls 0x550820 only if global backend
0x17c7bcc is exactly 0x66; other values return unchanged. The required resource
adapter now represents 0x550820. Only after that call does the routine clear
auxiliary +38 and set current +1080. Mode byte 0x7cabd4 exactly equal to one,
together with matching weapon 0x7cabc4, enters 0x4b0610. It returns unchanged
unless the separate 32-bit global 0x7cabbc equals one. That selected path calls
0x48ab90(model,1,0x7cabd8,resolved string from 0x7cabd8), a material-binding
operation whose body remains external. The mode adapter owns these arguments
through caller data. Missing reached adapters preserve earlier mutations and
leave the return-model output unchanged. The callbacks may update state or
context; subsequent reads follow original order. Their successful bodies and
actual model ownership remain open.

`tools/verify_weapon_presentation.py` matches 4,000 original executions:
3,236 complete, 54 resource, 25 binding and 685 missing-model boundaries.
Original cleanup (113 calls), resource gate (157 calls), mode gate (59 calls),
string access, timer and mode query execute unchanged. The test compares
outgoing callback arguments, full compact state, return value and unrelated
player bytes. It does not replace original callees or validate successful
external adapter bodies. Win32 Release, four CTest cases, the 3,000-case
current-weapon verifier and NXDK build pass. No new emulator execution or
visible weapon rendering is claimed by this checkpoint.

## Current weapon and presentation dependency

`rf_weapon_current` reconstructs 0x4a5910 using the shared entity registry.
Missing primary entities return -1. A valid linked entity of class 1 or 4
supplies weapon[0]; otherwise the primary supplies it. Lookup uses full handle
generation and type-zero validation. Stable views allow the repeated original
linked lookups to collapse into one without changing the result.

Every result except exactly -1 invokes original 0x4ae0d0, including -2. That
callee returns immediately for a nonlocal player, which the reconstruction
handles directly. A local player requires the presentation adapter; absent
adapters return RF_NOT_FOUND with output unchanged. Callback failures propagate
without discarding any model effects they already performed. A successful
adapter leaves the originally captured weapon as the return value, matching
the original even if presentation changes entity state. Presentation control
flow is now reconstructed above; external adapters and model ownership remain open.

`tools/verify_weapon_current.py` checks 3,000 cases: 2,590 complete original
executions (428 include unchanged nonlocal 0x4ae0d0 returns), and 410 stops
before local presentation. Full player/entity snapshots remain unchanged on
these paths. This covers linked classes, stale handles, nonzero object types,
negative handles, and weapon values -2/-1/0/1/63/64.

The shared diagnostic is explicitly a nonlocal rig. It now resolves its
current weapon before empty-ammo decisions, and the original verifier starts
at 0x4a6f10 rather than the post-lookup block 0x4a6f41. The same state hash
d5f86d40 passes on PC and 64 MiB XEMU; local-player presentation and outgoing
selection's earlier gates remain excluded. Latest numeric report:
`artifacts/xemu/20260908-184258-453142/report.json`.

## Selection queue

`rf_weapon_finish_selection` reconstructs 0x4a4c91..0x4a4db4 after the earlier
selection gates. Entity +1428 is a 32-bit mask checked by 0x42a6b0; a requested
index in [0,31] with its bit set remaps global 0x85ccd8 to 0x85cd00.
Already-pending requests return immediately. Global 0x87211c splits the
comparison between inventory +4 and +8 (primary/secondary current weapons).
An already-current request with a zero force byte returns, with a formatted
message when player +10 mask 0x10 is set (0x4a68d0).

Otherwise nonzero ownership byte, or the in-count descriptor +264 mask
0x40000 (0x4c9070), permits queuing. A zero defer byte then calls 0x4aa0b0.
Finally a nonzero player +f94 byte (0x4ace90) calls the reconstructed
`rf_weapon_clear_followup` (0x4ad8a0), even when ownership failed and no request
was queued. It clears bytes +f94/+f95 and the 32-bit value +f98, preserving
+f96/+f97. This byte is read from state after the apply callback, allowing
that adapter to update it through user data. The compact selection state is
now 16 bytes, including these eight bytes. The former followup callback and
input snapshot have been removed; this routine requires no external adapter.

Missing reached message/apply adapters return RF_NOT_FOUND before
that operation. Queuing and timer clearing remain committed if the apply
adapter is missing or fails. Invalid pointers or a descriptor count above 64
return RF_RANGE before mutation. No original descriptor read occurs for an
out-of-count index. This tail does not replace the complete 0x4a4a50 routine.

`tools/verify_weapon_selection.py` compares 4,000 original executions with
unchanged predicate, queue and followup-clear callees, stopping before the two
unavailable external operations. Expanded results: 3,071 complete, 408 message,
521 apply boundaries; 797 requests queued and 872 followup clears, including
687 clears without a queue mutation. The comparison checks both queue fields,
all eight followup bytes and unrelated player bytes.
It does not validate successful callback bodies or earlier selection gates.
PC/NXDK builds and PC tests pass; the shared Xbox diagnostic now exercises
this tail as described below.

`rf_weapon_queue_selection` reconstructs the complete 0x4acd50 routine:
store the requested signed value in player +f80, then tail-call 0x4fa3e0
to set the deadline at +b8 to -1. It deliberately preserves all signed
weapon values; eligibility checks belong to callers such as 0x4a4a50.
The queue primitive preserves all other compact state fields. A null state returns RF_RANGE
as an added safety check. This queues a request; activation and presentation
remain unimplemented.

`tools/verify_weapon_queue.py` compares 640 executions against the unchanged
original instructions and timer callee, including signed extremes and every
combination of selected boundary values. It checks the original 4096-byte
player view for unrelated writes. PC tests and the NXDK build pass; this new
primitive now runs in the emulator diagnostic; gameplay activation remains open.

The earlier description of 0x4a4e80 as a "paired switch" was too narrow.
Its body includes ammunition tests, firing through 0x425830, sound, reset and
empty-weapon handling. PAIR identifies the empty handler's paired branch,
which dispatches this firing routine with arguments (player,1,1); it does not
establish that the operation merely changes the selected weapon. Full firing
behavior still requires reconstruction. Actual selection 0x4a4a50 reaches
0x4acd50 after its eligibility checks.

## Empty-weapon handling and replacement choice

The 64-frame shared diagnostic now passes each SELECT decision into the
reconstructed selection tail with flags (defer=1,force=0). Its initial queue
is -1 with deadline 2000, followup bytes 1/255, preserved bytes ab/cd and value
99. At frame 32 ammo exhaustion queues weapon 1, clears the timer and followup
fields; 31 later requests hit the already-pending return. Original execution
observes exactly one queue call and one followup clear. All 16 selection-state
bytes are hashed each frame. This deliberately enters after the unreconstructed
early 0x4a4a50 gates, leaves the actual current weapon unchanged and never
reaches the local transition/message adapters. It validates the assembled
recovered blocks, not complete weapon switching.

PC, original instructions and 64 MiB XEMU agree on state hash d5f86d40;
pose/cache/eye remain dc7c08a6/21cd6b06/60a29326. Numeric XEMU evidence is
`artifacts/xemu/20260908-183800-793148/report.json`, captured with --no-capture.

`rf_weapon_decide_empty` now reconstructs 0x4a6f41..0x4a70db after current-weapon
resolution. The earlier 0x4a5910 presentation update is still caller-owned.
It returns NONE, PAIR, MESSAGE or SELECT with a selected weapon where applicable;
it does not execute the outgoing firing/selection/message operation.

The initial gate requires player byte +f40 or the current weapon matching global
0x872118. Passenger predicate 0x42acd0 blocks a nonzero request byte. Current
weapon equal to 0x85cce0 is blocked by projectile predicate 0x4c9e30. Descriptor
+260 must be positive, and weapon 0x87210c is excluded. Ammo is checked on the
linked entity for linked classes 1/4, otherwise on the primary entity. Positive
reserve-plus-loaded or the in-count descriptor +264 mask 0x20 prevents action.

Weapons 0x85ccd8/0x85cd00 form the paired branch; the former uses that branch
only when byte global 0x64ecb9 is zero. Positive counterpart ammo requests
0x4a4e80(player,1,1). Otherwise the paired path goes directly to replacement
choice. Outside that path, linked classes 1/4 or a passenger request the
out-of-ammunition message via 0x4383c0 with zero flags. Remaining cases select
from the PRIMARY inventory's preference list (even when ammo was checked on a
linked entity), then request 0x4a4a50(player,weapon,1,0) for a valid selection.

The input's passenger/projectile predicates and linked classification are
resolved snapshots. `tools/verify_weapon_empty.py` exercises their unmodified
original callees, including a projectile-list fixture, then observes final
operation boundaries. All 4,000 cases match: 3,272 NONE, 64 PAIR, 202 MESSAGE,
462 SELECT. The shared runtime also hashes NONE before ammo exhaustion and
SELECT afterward, agreeing with original instructions and 64 MiB XEMU.

The remaining callback at 0x4a6f10 handles an empty weapon, rather than a generic
player-state reset. It checks reserve plus loaded ammunition, may show the
string "Out of ammunition" at 0x5a05e0, handles a paired weapon through 0x4a4e80,
and can choose another weapon with 0x4a6e50 before calling 0x4a4a50. The current
`player_reset` callback name is historical; this full operation is still open.
Its current-weapon lookup 0x4a5910 also calls 0x4ae0d0 for a valid weapon, which
has presentation/model effects and must not be treated as a pure accessor.

`rf_weapon_reserve` reconstructs 0x42add0: a null entity, negative weapon, or
negative descriptor ammo type returns zero; otherwise descriptor +24 indexes
entity reserves at +2ac. The shared view contains 32 reserves, 64 loaded counts
at +32c, and 64 ownership bytes at +42c. Added bounds checks reject nonnegative
weapon/ammo indices outside those arrays rather than reproducing original reads
past them.

`rf_weapon_choose_available` reconstructs 0x4a6e50 after entity lookup. It scans
all 32 preference entries at player +1154 in order; invalid or unowned weapons
are skipped (0x403250). If descriptor +260 is positive, reserve plus loaded
count must be positive. The sum retains original 32-bit wrapping behavior.
With byte player +f41 nonzero, weapons whose descriptor +268 has mask 0x100
are deferred: retain the first such eligible weapon as fallback, but continue
looking for an eligible unflagged weapon. Return -1 if none is available.
The meaning of the flag is not inferred beyond this observed selection rule.

`tools/verify_weapon_inventory.py` matches 4,000 complete original executions
of both functions with unchanged lookup, ownership and flag callees. 3,475
choose a weapon; fixtures include null entities, duplicates/invalid preference
entries, deferred fallbacks, shared/negative ammo types and wrapping count sums.
Two C-only cases verify weapon/ammo bounds rejection.

The shared 64-frame diagnostic also observes this choice: weapon 1 is a deferred
fallback; weapon 0 is chosen while reserve 0 is one, then weapon 1 is selected
after reserve reaches zero at frame 32. PC, original instructions and stock
64 MiB XEMU agree. The diagnostic hashes reserve and replacement outputs but
does not apply an actual weapon switch; presentation and the complete empty-
weapon handler remain separate work.

`rf_weapon_reset` follows 0x41ae70 after a valid type-zero entity has been
resolved. Null state and indices outside [0,63] return unchanged. Descriptor
views contain flags +264/+268 and release sound class +204 from the original
1360-byte records at 0x85cd08. All 64 descriptors must be available even when
global weapon count 0x872448 is smaller: the early active-weapon path uses the
fixed 64 limit; predicate 0x4c90f0 additionally checks the dynamic count.

The entry's active byte is captured before any reset. If nonzero, byte global
0x64ecbb differs from exactly 1, and descriptor +264 has mask 0x2 or 0x4 set:
stop sound +81c if it differs from -1, then store -1; emit release sound when
its class is nonnegative, storing the returned handle in +820; and stop
nonlooping animation weights when 0x40a1e0 reports a character and +810 mask 0x1
is clear. The stop is the shared 0x51c390 reconstruction, retaining slots and
references until update. Every valid reset then clears the entry's active byte
and entity +7d0 bit 0x2000.

If weapon predicate 0x4c90f0 succeeds (+268 bit 0x40 with an in-count index)
and +13d4 is not -1, call 0x48f130(handle,0); the caller does not clear that
handle. Next, a player-associated entity with a previously active entry calls
0x4c90f0 again and discards its result at 0x41afcc. Otherwise a locally
associated, previously active entity calls
0x48aa90 only when its weapon matches one of five globals (0x872110, 0x872464,
0x872444, 0x85cd04, 0x85ccfc). 0x48aa90 is a read-only lookup returning the
first matching local player pointer, and this caller discards that result too.
The earlier interpretation as a local-release operation was incorrect; the
false callback dependency and unused state/context fields have been removed.
The original 0x41afbb..0x41b015 block has no state changes on stable views.
Finally a player-associated entity calls
0x4a6f10(entity+1430,0), regardless of the captured active byte.

The compact state uses resolved zero/one predicates: character_present is
0x40a1e0 (non-null model wrapper and class +94 equal to 2); player_present is
0x42a8e0 (+7c bit 8 and non-null +1430). Entity resolution, list ownership and
class loading remain separate.
Callbacks may update these fields through their user data; later decisions read
them after preceding effects, matching the original order.

External sound, effect and player-reset adapters are required only
when reached. Missing operations return RF_NOT_FOUND before that operation,
retaining any earlier mutations. The release-sound adapter owns 0x4285a0 position
selection, 0x434d00 class resolution and 0x48a9c0 emission. Effect switching is
now reconstructed in effect.c and used by the diagnostic; successful audio/player
effects remain unverified.

`tools/verify_weapon_reset.py` compares 2,400 original executions with full
entity-reset, playback and reference state. 1,598 complete; the other cases stop
at observation boundaries before deliberately absent adapters: 160 sound stops,
97 release sounds, 83 effect stops and 462 player resets. 132 original 0x48aa90
lookups now execute to completion instead of being mistaken for side effects.
Original callees are not replaced. At each boundary C must return RF_NOT_FOUND
with exactly the same preceding mutations. This verifies supported paths and
the operation boundaries, not successful external adapters.

The shared diagnostic now uses valid weapon index 0, marks it active at frame
48, and invokes this reset through the preparation callback. Sound handles and
release sound are absent; character playback and an effect pair are present. Its nonloop stop
causes a later sidestep restart, producing starts 17,17,18,18. The complete
64-frame profile, including 16 reset calls, weapon state and changed poses,
matches original instructions, PC and stock 64 MiB XEMU. This remains a
scripted rig rather than an initialized gameplay character.

## Effect switching

`rf_effect_set_enabled` reconstructs 0x48f130 and 0x4973b0/0x4973d0. Each
original 40-byte effect record provides two object pairs at 0x75ec48/4c and
0x75ec50/54; nonzero byte global 0x64ecb9 selects the latter. Both pointers
must be non-null or neither changes. Disabling clears byte +140 and leaves
deadline +154 intact. Enabling with any nonzero int changes the byte to exactly
one and stamps current game time only when its previous value was not one.
Aliased pointers retain this same sequential behavior. Bounds checks reject
invalid record indices instead of accessing arbitrary original memory.

`tools/verify_effect_switch.py` matches 4,000 complete original executions,
including missing pairs, aliases, noncanonical enabled bytes, override low-byte
selection and clock boundaries. Two C-only cases reject invalid indices.
This controls existing effect state; effect creation, simulation and rendering
are not implemented. The shared runtime invokes a real effect-stop adapter
on weapon reset and hashes both enabled bytes and preserved timestamps.

## Local-player transition evidence

0x4aa0b0 returns immediately unless its player is global 0x7c75d4. Otherwise
it clears followup state through 0x4ad8a0, calls 0x4aa080, clears byte +fb0,
requests state 7 through 0x4a9380, and calls the presentation-bearing current
weapon accessor 0x4a5910. Either true 0x4c8350(weapon,0/1) predicate invokes
0x41ae70(entity handle,weapon). After resolving the entity again, a weapon
matching global 0x872468 (0x4c90d0) stamps entity +136c with game time.
This routine does not directly install player +f80 as the current weapon;
calling it queued-weapon activation would overstate the recovered behavior.
The `apply_queued` adapter name describes its caller position, not a proven
complete activation operation. Its full body remains unreconstructed.

0x4aa080 tolerates a null player; otherwise it clears bytes +f9d and +f9c,
sets +fa0 to -1, and stamps +fa8 using 0x4fa360(offset=0). 0x4ab180 sets byte
+1044 to 1 on global local player 0x7c75d4 when non-null. These observations
identify the next state dependencies; they are not implemented by this change.


## Explosive projectile inputs for live integration

rf_weapon_explosive_read/load independently decodes named weapons.tbl SP
velocity, lifetime, collision radius, damage radius and crater radius, requiring
an explosive Weapon Type. It rejects duplicate, missing, malformed or negative
fields, requires positive speed/lifetime, preserves output on failure and uses
a bounded table scratch allocation. Multiplayer overrides are ignored. These
are authored parameters, not recovered flight, homing, fuse or blast policies.

Installed tables.vpp verification: Rocket Launcher has speed20, lifetime15,
collision radius0.051, damage radius5 and crater radius5. The existing primary
loader also resolves its six-round clip,1.7 reload,1.25 fire interval,400 SP
damage and explosive damage kind3. Grenade has speed10, lifetime5, collision
radius0.15, damage radius8 and crater radius5; its distinct multiplayer values
do not overwrite the SP fields. The grenade impact-delay/fuse, gravity and
bounce behavior remain separate and are not implied by this loader.

rf_weapon_explosive_tests checks both installed definitions, primary rocket
rules, table budget rejection and malformed/duplicate/missing-field rollback.
NXDK builds the XBE/XISO. No live launcher, projectile motion, blast damage,
impact presentation or native runtime execution is claimed by these tests.
Next is bounded projectile flight against current world collision, impact
routing to damage/GeoMod and launcher resource/input integration in the room.


## Swept straight-flight core

rf_weapon_flight_launch/step adds allocation-free straight projectile motion.
Launch normalizes direction, applies supplied speed/lifetime/radius and refuses
to overwrite an active flight. Step clamps travel to the remaining lifetime,
sweeps the complete segment through a caller collision callback, and advances
the projectile center to the accepted fraction. Impact events retain the
surface contact separately from the center and carry caller object/room/face
identities. A hit at lifetime end wins over expiry. Impact/expiry deactivate
flight and emit once. Callback failures or invalid hit results preserve both
flight and output event. Remaining lifetime uses double precision to reduce
repeated tick subtraction drift; positions remain shared float coordinates.

Installed Glass House collision tests launch at(0,-10,10) toward x16 with
speed20 and radius0.051. A one-second step hits the wall rather than tunneling,
stopping the center at x15.949 with surface contact x16. Fixed60Hz stepping
hits on step48 and agrees within float tolerance. A0.1 lifetime stops at x2
and expires before reaching the wall. Subsequent steps emit no further event
or query; callback failure/invalid-fraction tests preserve state and outputs.
NXDK builds the XBE/XISO. No native projectile execution or render is claimed.

This is practical first-pass motion, not recovered homing, acceleration,
gravity, bounce or grenade-fuse behavior. The callback still needs live scene
routing across world/movers/actors, followed by damage and GeoMod dispatch.
Projectile pool/resource ownership, launcher supply and visible presentation
remain separate integration work; normal weapons are unchanged by this core.

## Rocket launcher resource preflight

The authored first-person definition resolves fp_rocketlauncher.v3c with
fp_rocket_hold/fire/reload.rfa through the existing shared resource owner.
Installed-asset PC verification loads24bones,549vertices and3clips with
879608resident/891204peak accounted bytes, below the1MiB per-weapon budget.
Each clip runs240ticks at60Hz with finite prepared skinning matrices and
ends in idle. This establishes resource compatibility and numeric playback
only: framing, complete visual animation review and audio remain unverified.
The launcher is not yet selectable or firing in the live developer room.
No native run or new Xbox build was needed for this test-only change.

## Live DEV launcher first pass

Glass House now exposes the Rocket Launcher after Shotgun in the ordinary
weapon cycle. Supply/refill includes its authored6loaded/18reserve rounds.
First-person idle/fire/reload uses the verified resource owner, with existing
shared camera framing provisionally reused. Normal primary timing, ammo
consumption and reload operate; alternate lock/homing is not implemented.
Outside explicit DEV mode, the four prior slots/resources remain selected.

A fixed50-entry scene-local flight array launches from the gameplay eye along
the aim vector. It advances once per combat tick before selection/reload
early returns. The full sphere sweep tests current world collision, including
the live terrain overlay. Impact or lifetime expiry retires each flight once.
Only room0 impacts dispatch bounded box excavation with authored crater
radius5; successful edits immediately rebind collision. Expected bounded
cut rejection leaves the previous terrain intact. No hitscan damage is used.

This remains incomplete: actor/mover collision, visible projectile/trail,
explosion effects, blast/self damage, homing, spherical cuts and eligibility
are pending. Fire/reload sound requests use the authored names, but audio and
full animation sequences have not been reviewed. The launcher display is
currently white; no working targeting display is claimed.

tools/dev_rocket_check.py reproduces four ordinary PC input cases: a rocket
still in flight immediately after firing, completed impact/excavation, reload
during flight and switch to pistol during flight. Existing shotgun loadout
checks pass. Native260frame shot replay at
artifacts/xemu/render-20260915-080750 passes36 comparisons, including all
eight ROCKETS words, with9262physical pages free (36.1797MiB). Its native
frame shows the launcher and wall recess;19disc staging entries restored.
Reload/switch-in-flight cases remain PC-only. No GitHub screenshot added.

## Radial blast damage first pass

Each rocket world impact now dispatches one blast before changing terrain.
The origin is the surface contact biased0.01units along its inward normal.
For each living registered actor and the player, transformed body spheres
provide distance to the nearest exposed sphere surface. Damage decreases
linearly from authored400 to zero at radius5; the largest visible sphere
contribution applies once per body. This falloff is practical port policy,
not a recovered original formula. Existing bullet-obstruction geometry
queries include stationary world and movers. Damage precedes excavation
so a newly cut hole cannot expose a victim retroactively to this blast.

Requests use explosive kind3, player source attribution and existing SP
damage dispatch for armor, health and feedback. NPC fatal damage routes
through retained death entry/presentation. Player self-damage is enabled.
Expiry does not explode. Knockback, damageable clutter, actor impact sweep,
projectile/trail visuals, explosion presentation and homing remain open.

The ordinary DEV input harness now has seven cases. Far detonation retains
100health/100armor; nearer detonation leaves86.22935health/85.08179armor;
close detonation exhausts armor and activates the player death state.
Delayed impact and reload/switch-in-flight cases still pass. PC endpoint
render inspected: launcher, cut cavity and reduced HUD bars are visible.
NPC blast damage and geometry occlusion are implemented but need dedicated
behavioral coverage; these empty-room replays do not prove either. Full
blast audio/animation review also remains open.

Blast native verification (2026-09-15):400frame ordinary nearby-shot replay
at artifacts/xemu/render-20260915-081337 passes37 PC/Xbox comparisons,
including all blast diagnostic words;9261pages (36.176MiB) free. Native
frame inspected with reduced health/armor HUD and excavated cavity visible.
All19 staged disc entries restored; owned emulator exited. Far/lethal and
reload/switch cases remain PC-only. Estimate remains ~49%; explosive weapons.

## Authored rocket VFX geometry owner

rf_vfx_geometry_asset_open composes the existing VFX directory, mesh and
instance decoders for legacy mesh-only assets (versions3000a through3ffff).
It owns at most32meshes and their playback buffers, rejects non-SFXO chunks
and unsupported versions, and rolls back all partial allocations on error.
The budget includes directory accounting and temporary largest-chunk bytes
alongside all retained geometry. Archives may close after successful load.
Textures, material playback, parent hierarchy and renderer binding are not
part of this owner; its name explicitly identifies geometry-only scope.

The installed DrillMissile01.vfx has7meshes, all parented to Scene Root:
Mesh01,Box02,Box03,Box07,Box08,Box04,Box01. PC storage is48989resident and
57839peak bytes. Tests step0..16effect frames at quarter-frame increments
after archive close, checking finite vertex/UV output. Exact peak budget
loads successfully; peak-minus-one and resident-minus-one reject without
output ownership. NanoAttackMissile.vfx rejects its unsupported newer
composition rather than silently dropping content. These checks establish
numeric resource readiness only, not visual correctness or live drawing.

NXDK XBE/XISO build passes with the new geometry owner; live renderer
integration is pending, so no native visual execution is claimed here.

## Rocket material and texture ownership

rf_vfx_asset_materials_open flattens legacy per-mesh material IDs through
first[mesh] offsets, retains render/texture fields and color words, and
loads deduplicated texture animations through the existing VFX loader.
It has a64-material bound and transactional failure cleanup. Geometry and
archives may close after success; embedded serialized track offsets are
not standalone tracks and must not be sampled without original mesh data.
Geometry-based material track evaluation remains a separate API.

DrillMissile01.vfx resolves9materials to Dmissile_Parts01/02/03/04.tga and
MissileFlare01.tga. All5textures contain one frame and load successfully.
Retained material/texture storage is51536bytes, alongside48989geometry
bytes (~98.2KiB combined). Tests reject resident-minus-one budget, close
source archives/geometry, then sample all bound primary/secondary slots
for61 timestamps without lost resources. No live render, blend correctness
or native runtime visual claim follows from these resource checks.

NXDK XBE/XISO build passes with the material owner; scene integration
remains pending and this change does not add a native visual run.

## Live authored rocket drawing first pass

The DEV scene now opens shared rocket geometry/material owners once and
transfers the five single-frame image allocations into its shared material
table without copying pixels. A bounded workspace holds384vertices and
128faces for one effect mesh at a time. Flight records retain their launch
basis. Active projectiles update the seven authored meshes at15Hz effect
time, provisionally wrapping the known16-frame asset, then transform them
into world space. Shared generated-geometry projection clips/culls textured
triangles and appends them before the first-person weapon. No render-time
allocation or archive reads. Zero-area animated faces are omitted.

This first pass uses primary textures and existing geometric shading. VFX
blend/color/opacity tracks, additive flare state, secondary layers and
particle trail/impact explosion are not yet integrated. The purple flare
texture is authored MissileFlare01.tga, not a missing-texture placeholder;
its black background currently shows because additive blending is pending.
The launcher targeting display also remains white.

tools/dev_rocket_check.py adds a125-frame flight view with ordinary look
input after launch. It verifies one flight,7active meshes and192emitted
vertices; PC frame inspected with the textured projectile/flare visible
left of the crosshair. Seven prior gameplay cases still pass, and draw
counts clear after projectile retirement. Workspace plus resource accounting
is127161bytes on the current build. This is not final visual parity.

Live rocket visual verification (2026-09-15):125frame ordinary in-flight
replay at artifacts/xemu/render-20260915-082740 passes38 PC/Xbox
comparisons, including7active meshes,192vertices and matching vertex hash.
Native frame inspected: authored rocket/purple flare visible just left of
crosshair; black flare background remains an additive-blending limitation.
9233pages free (36.066MiB). All19disc entries restored and owned emulator
exited. No GitHub image uploaded. Estimate ~49%; projectile visuals.

## Destruction focus: faceted rocket craters

Rocket impacts now use an inscribed20-face icosahedral cutter at authored
radius5 rather than a box. The shared terrain owner retains up to8 mixed
convex cuts, with60vertices/20faces reserved per history slot. The same
transactional clipping and collision publication apply. Room capacity is
4096vertices/512faces under its1MiB budget, with a512-entry overlay map.
Complex overlap may still reject within these fixed bounds.

The first crater render reused metal wall panels, producing a misleading
folded/protruding appearance. The DEV room now explicitly uses installed
rck_canyon_rock01.tga as its excavated substrate. Crater UVs use dominant
plane projection at one tile per4world units. This is testbed material
policy, not recovered per-surface GeoMod eligibility/material metadata.
Collision fallback still maps generated faces to the prior room surface
policy. Interior illumination and debris are unfinished; coarse facets
remain visible and are not represented as final destruction quality.

tools/dev_destruction_check.py reproduces intact/single/two-impact views,
an angled approach and traversal using ordinary game input. Two successful
cuts reach generation3; the player walks to x=-18.045902 beyond the original
x=-16wall, alive, at y=-13.067719. Peak terrain plus overlay accounting is
706675bytes. PC images show a deeper rock cavity after the second shot.
These checks cover this outer room, not arbitrary campaign geometry,
material eligibility, dynamic objects or a complete destruction system.
