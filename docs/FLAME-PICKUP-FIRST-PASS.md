# Flamethrower ordinary-level integration

2026-10-09 source update: the original continuous firing voice now has a shared,
generation-qualified player weapon owner. Ordinary flame stop closes that voice
before its existing release cue, and restored ignition2 can reacquire it on the
next admitted tick. Gameplay state and save formats are unchanged. Parent scene
hooks and scheduled Xbox verification remain separate; see
`docs/PLAYER-WEAPON-LOOP-AUDIO.md`.

The installed flamethrower item requests sparse first-person, fuel-stream and thrown-canister resources in ordinary levels. Napalm supplies fuel without creating ownership or resource demand by itself. Imported ownership and scripted grants request the weapon. Resources include the existing shared Fire01 impact bitmap/recipe, authored flame particle settings, canister explosion materials and powerup_flamecan world model, all within existing resource caps.

The full flame input/canister/visual update now runs whenever those resources are present, including off-weapon canister flight. Scene entry resets ignition, fuel remainder, flight and visual state. Flame and Fusion slots bypass ordinary magazine/hitscan handling outside DEV; their own adapters exclusively own firing and fuel/ammo changes. DEV-only blood assets remain separate.

Focused installed-table pickup/import/script demand checks and PC/NXDK builds pass. `tools/check_flame_pickup.py` runs original CTF06 spawn, geometry,506 props and pickups without DEV. Real flamethrower and Napalm grants at0/60 supply100 loaded fuel and100 reserve. Normal cycling selects slot10. Primary stream120..179 leaves55 loaded/100 reserve and visible flames; alternate120 releases its canister after108ticks, replaces the tank from reserve, then impacts/explodes by359. Final ammo is100 loaded/0 reserve; canister counts are1start,1release,1explosion,0live,0poolmiss. Both PC final images were inspected; stream and canister blast are visible. Audio was not auditioned.

Native alternate run `artifacts/xemu/render-20260922-174650` passes360 frames and all harness comparisons without DEV. Exact canister counts1/1/1/0/0 and tank state match PC; the inspected native framebuffer shows the canister explosion. Stock64MiB retains3658 free pages (14.29MiB), with the original disc restored. The new ordinary-level primary stream remains PC-only. Continuous audio, broader live NPC encounters, ordinary-level saves/terrain and presentation refinement remain open.

A focused non-DEV Fusion replay (`artifacts/fusion-pickup-live/ordinary.log`,123frames) also confirms the shared special-weapon guard: one real shot/launch, loaded0/reserve1 and selected slot12, without an additional ordinary hitscan shot.


## Ordinary active-state persistence (2026-09-30)

RFAP3 extends the existing RFWC3 flight component with row8 for each thrown
flamethrower tank and exactly one row9 controller whenever any flame state is
live. RFAP1/2 remain readable. The 112-byte row layout is the port's save format,
not a claim about the original executable's serialization. Ordinary world saves
now permit active flame state; the older standalone RFCP profile retains its
existing restrictions on ignition, fractional fuel, reload, pending throws and
canister flights. Its legacy held/cooldown-only behavior is unchanged.

Canister rows preserve pool slot, position, velocity, radius, remaining fuse and
life, resting/contact state and contact normal. Controller fields at byte16 are
12 little-endian words: ignition, ignition delay, reload remaining, reload elapsed,
reload drain already applied, fractional fuel remainder, damage-cadence armed,
remaining cadence frames, active stream, alternate held, release delay and
alternate cooldown. Completed reload history is normalized away. Cadence is
rebased to the restored combat frame; stream/reload/release progress continues
through the ordinary gameplay tick without replaying the grant or spending fuel
again during assignment. Original tank-replacement evidence remains42c362..42c386.

Boot demand includes the flame resource slot even for an off-weapon canister.
World preflight rejects invalid controller/flight state before publication and
checks ownership/selection for pending release, ignition and reload. A flying
canister keeps ticking after switching away. The redundant combat-frame-zero
reset is removed because scene initialization already resets flame owners and a
subsequent world load must survive its first gameplay tick. Cosmetic old particles
are not serialized; the stream resumes ordinary particle submission.

`tools/xemu_flame_save.py --mode canister` prepares an enemy-free CTF06 with
ordinary flamethrower/Napalm grants, saves two frames after tank release and loads
through the remaining flight/impact. Optional stream, pending and reload modes
exercise those corresponding controller states, one bounded save/load pair each.
The harness uses a private HDD, no images and no PC runtime; disc flags and its
small custom archive are restored afterward. Source/Python syntax and native canister/stream save/load checks are complete
as recorded below. Pending-release/reload modes are implemented but not separately
run in this bounded batch. This does not close burning-target saves, continuous
sound ownership or visual polish.


The first native attempt (`flame-save-canister-20260930-121609`) stopped before
scene initialization because the harness named its archive `flamesav.vpp`, which
is outside the Xbox selection whitelist. The harness now uses the existing
`scene-fixture.vpp` slot; this failure supplied no flame-runtime evidence.

The next attempt (`flame-save-canister-20260930-121641`) correctly released one
tank and retained it alive at230 frames, but exposed a missing snapshot hook:
flame-only state did not reserve the RFWC3 header, so envelope overlap validation
rejected the save withRF_RANGE. The prefix decision now includes flame state.
This was an integration defect, not a malformed test input. Ordinary player
admission also now permits the dedicated flame reload counter alongside Fusion.


Native canister check `artifacts/xemu/flame-save-canister-20260930-121907/report.json`
passes. Source230frames saves one live canister and131 remaining cooldown ticks;
fresh load240frames restores both, advances to exactly one explosion, and records
no extra launch. Loaded fuel100/reserve0 remain unchanged through reload. The
minimum endpoint is3487 free pages (13.62MiB); disc flags/archive restore passes.

Native stream run `artifacts/xemu/flame-save-stream-20260930-122207` saves ignition2,
fractional fuel40, remaining damage cadence3 and active stream1. Source fuel72 and
reserve100 restore intact; the next29 gameplay ticks consume24 fuel, leaving48,
with29 ordinary flame particle-submission calls. Minimum endpoint is3471 free
pages (13.56MiB). The initial harness incorrectly expected30 resumed ticks: source
inspection confirms startup world publication happens at the end of frame0.
`validate_stream` now accounts for29 ticks in a30-frame load session. Offline
revalidation of the same native data passes in `validation.json`; the original
`report.json` FAIL is retained and no extra native run was needed. Both native
sessions and the final disc-restoration build completed. No images or audible
output were inspected, so particle submission is functional evidence only.


## Burning NPC save owners (2026-09-30, source implementation)

Ordinary RFWC3 saves now carry NPC burning owners through RFAP4 row10. RFNC
continues to own health, armor and actor pose; RFAP owns the64-slot burn table.
Each112-byte row stores kind10, slot, authored target UID, source kind, authored
source UID where applicable, remaining lifetime and damage phase at offsets0..24;
offsets28..40 preserve three relative pain deadlines and the selected pain action,
and offset44 preserves the shared pain RNG. The remaining bytes are zero. The
current300-frame lifetime and15-frame cadence
are retained port policy. Per-pulse damage remains class_health*(0.25/6.5), based
on retained42f1dc/rf_burn_owner_tick evidence; this change does not retune it.

Source kinds distinguish absent, local player, authored NPC, ordinary vehicle and
passive vehicle. Registered dead NPC sources retain their authored identity instead of writing
a registry handle into the save. Sources removed after ignition originally
rejected capture because unregistration clears their handle; the source-written
2026-10-09 extension below retains that identity. Unknown sources reject capture;
there is no fabricated fallback actor. Preflight resolves identities and checks
that every target is a saved, live, registered NPC before assignment. Source NPCs
must also have a saved NPC record and a qualified original owner, including the
retired-source extension below. Missing/unsupported actor states retain their
existing save restrictions.

NPC export now has an explicit burning profile only for composed ordinary saves;
standalone NPC export retains its previous refusal. The world prefix includes
burn-only state, boot resource demand includes flame effects, and publication
restores the target's burn backlink and source without re-running ignition.
Lifetime/phase continue directly; expiry clears both target backlink and source.
Player ignition remains excluded by the current damage-effects admission and
is not invented by this serializer. Attached particle history is regenerated,
not persisted.

`tools/xemu_burning_save.py` is a bounded source-save/fresh-load harness for
L8S4 guard10318. Its opt-in fixture stages zero armor and catatonic AI, then calls
the ordinary kind4 damage pipeline with damage1 atframe30. Source70frames saves
remaining life/phase; load270frames is intended to verify remaining damage pulses,
health arithmetic and terminal retirement without another ignition; this target
has authored health300 below its class health500, so it dies before expiry. Native
evidence and the independent player-death limitation are recorded below. No
campaign route, PC runtime, images or audible inspection are involved.


Initial native burn save `artifacts/xemu/burning-save-20260930-123541` reached
one active burn with260 frames left and phase10 but rejected the target's pending
pain.animation_lock (1450ms at current1150ms). The elite class has500 base health,
so each burn pulse triggers ordinary pain animation; ignoring that timer would
lose gameplay state. RFAP4 now serializes the three pain deadlines as relative
milliseconds, including distinct disabled/expired states, the selected action
and shared pain RNG. Ordinary NPC capture permits pending pain timers and the
matching pain clip only when the burn owner is included; RFNC already stores the
clip playback. Deadlines are rebased during final assignment after NPC pose restore.
Original L8S4 record UID10318 names class `elite` at uncompressed offset1331095;
entity.tbl has no fire-factor override for that class, so the parser's default1
applies to the bounded health-arithmetic check.


The corrected native run `artifacts/xemu/burning-save-20260930-123956` completed
its70-frame save and270-frame load. It saved target10318 with260 burn frames left,
phase10, health260.538452 and player-source identity. Pending pain deadlines were
0/300/624ms, selected action22 and pain RNG2745024. The restored burn produced
exactly14 further pulses, one retirement and one target death, with final health
-8.69236755 matching binary32 damage arithmetic; its burn backlink/source cleared.
Minimum endpoint memory is4503 free pages (17.59MiB). Disc restoration passed.

The generic native harness still failed because the player-death counter was1
during the load run. That original `report.json` remains FAIL, and the generic
runtime gate was not relaxed. `tools/xemu_burning_save.py --validate-existing
artifacts/xemu/burning-save-20260930-123956` rechecks all burn-specific assertions
against the recorded checkpoint and source/load telemetry without another native
run, writing `validation.json` with result `PASS_BURN_CONTINUITY`. This supports
NPC burn save continuity only; it does not establish player survival or explain
the separate player death. No images, audio output or campaign routes were used.

## Retired ignition-source identity (2026-10-09, source-written)

Each live burn now captures its authored NPC source UID at ignition, independently
of the native damage-source handle. RFAP4 kind10, its112-byte layout and reserved
bytes are unchanged. Registered dead sources retain their exact native handle;
actual NPC removal normalizes the burn's native source to UINT32_MAX without
losing kind2/UID on save or resave. Same-frame capture before that normalization
qualifies the original retired owner and its retained damage handle, never a
replacement object occupying the old registry slot.

RFNC preflight determines whether the saved source will remain registered or be
retired. Only the private burn candidate is normalized before world publication;
load does not resurrect a source or rerun ignition, damage, audio, loot or death
callbacks. Existing pulse arithmetic, pain state, target death and burn expiry
remain unchanged. Whole-record retirement/reset and scene teardown clear the
retained UID. Added storage is256 bytes for the live64-slot pool and256 bytes per
private restore stage, with no new allocation during gameplay.

Source evidence remains41a350 damage credit,42f1dc burn-owner damage and48684f
registry invalidation, plus the port's ordinary Remove_Object and corpse-retirement
paths. Other RFNC owner-state restrictions remain independent. The existing burn
unit test's owner stubs were adapted for the new identity dependencies; no new
test cases or fixtures were added. No build, test or native runtime was run for
this extension; it awaits the parent-coordinated16:00 Xbox batch. Earlier player-
source burn continuity evidence does not establish removed-source runtime.
