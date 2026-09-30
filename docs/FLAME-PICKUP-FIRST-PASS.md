# Flamethrower ordinary-level integration

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
