# Core gameplay developer room

Current mandate: pause campaign-route work and build usable core gameplay here.
Use the installed `levelsm.vpp` / `glass_house.rfl` map as a local testbed, not
as multiplayer implementation. Its original section directory has no NPC,
event or trigger section, three items and one GeoMod region. Original assets
remain unchanged and untracked.

PC launch: `build/pc/Release/rf_pc_play.exe --dev-room Installed_Game`.
This uses the ordinary input, movement, weapon and rendering paths; it does
not launch automatically or send host input. `--dev-room-replay` accepts an
ordinary input file and output PPM for process-local checks. Level exits are
disabled in the explicit developer-room mode.

Evidence in `artifacts/dev-room-glass-house`: stationary120 frames and an
ordinary240-frame walk/turn/three-shot sequence both complete alive, with no
NPCs or enemy attacks. Player position changes from(.296,-11.118,16.100) to
(2.308,-11.118,13.218); handgun magazine changes16 to13. Both final native PC
renders were inspected: the enclosed textured room, central glass structure,
pickups, first-person handgun and HUD are visible, with the expected changed
view after movement. This does not verify recoil/reload animation timing,
glass destruction, GeoMod, sound, every weapon or interactive controller use.

The shared scene now accepts an absent NPC section explicitly. Xbox's extra
startup diagnostics also need to accept omitted trigger/event sections as
empty owners. The first Xbox attempt failed there; verification after that
fix passes all35 PC/Xbox comparisons in artifacts/xemu/render-20260915-061952.
Endpoint free memory is37.46484375MiB on stock64MiB; all18 disc entries
restore. The native framebuffer was inspected and shows the expected room,
central structure, handgun, HUD and moved view. Audio and transient animation
frames remain unverified. Reproduce input/state checks with
`python tools/dev_room_check.py`; inspect its output separately.

Next work: add deliberate
weapon selection/supply/reset controls for this room; inspect fire/reload and
first-person animation sequences; implement GeoMod topology, collision and
visible destruction within the stock64MiB budget. Controlled target tests can
be added explicitly while keeping the default room enemy-free.

## Supported weapon loadout

Explicit developer-room mode now supplies the handgun, assault rifle, Riot
Stick and shotgun on entry using the shared acquisition routine and authored
ammo capacities. Normal campaign mode does not receive these grants. Use + Reload (PC E+R; Xbox X+Y) refills all four weapons once per press.
The combination suppresses normal reload while held; release before using it
again. Firing while held still consumes ammunition. This restores supplies,
not room geometry, player position or health. Restart for a fresh room.
Cycle weapons with the normal control. This does not implement additional guns.

`python tools/dev_room_check.py --loadout` reproduces the180-frame selection
and shotgun check. PC selection checkpoints31/61/91 were inspected for rifle,
Riot Stick and shotgun; final PC and Xbox captures show the shotgun mid-action.
One accepted shotgun shot emits four pellets and leaves7loaded/48reserve; the
second requested shot is inside the current cooldown. This checks current
behavior, not retail cadence fidelity. The early Riot Stick equip pose fills
much of the view and needs sequential animation/framing review.

Stock64MiB run artifacts/xemu/render-20260915-062344 passes35 state comparisons,
with37.46484375MiB free at endpoint. All19 staged disc entries restore. Use
`--dev-room --spawn --level glass_house.rfl --archive levelsm.vpp` with the
Xbox harness and this recipe's input. The flag is restored after the test.
Full animation sequences, audio, reset UI and GeoMod are still unverified/open.

## Sequential animation capture

Set RF_REPLAY_CAPTURE_DIR to an existing local directory when running a PC
recording of at most600 frames. Every frame is rasterized and saved after the
HUD as frame-000001.ppm onward. Missing/unwritable output and overlong captures
fail rather than silently skipping frames. This is native process output,
not desktop capture. Capture is disabled for interactive play and by default.

The60-frame dev-riot-idle-sequence capture has all60 files. Capture/control
runs match player body, life, ammunition, combat, enemy and weapon state, and
the final image bytes. Frames4 and60 were inspected: the Riot Stick remains
large in idle. These endpoints do not constitute a full sequence review.
Source inspection shows weapon switching requests clip0 (idle), not an equip
clip; earlier references to an equip pose were imprecise. Full idle/bash/held
fire/reload visual review and camera placement correction remain open.

## Refill verification and framing review

The developer-only refill uses authored magazine and reserve capacities and
cancels an in-progress reload. PC checkpoints verify15 loaded rounds before
refill,16 after refill, then15 after firing while the chord remains held.
`python tools/dev_room_check.py --refill` reproduces the final100-frame input.
Xbox artifacts/xemu/render-20260915-063114 passes35 comparisons;19 staged
files restore. PC/Xbox final captures were inspected for the room, handgun and
HUD. These checks do not prove audio or complete transient animation quality.

Two Riot Stick camera pullback experiments exposed open sleeve ends. They
were rejected and the previous placement retained. Compare the original
presentation before changing its camera again; the oversized idle endpoint
alone is insufficient evidence for an arbitrary camera offset. Full sequential
idle/attack/held-fire/reload inspection remains open.


## Live excavation control

Hold Use and press Alt Fire (PC E+G; controller X+left trigger) while aiming at
an outer wall. The developer cutter makes a4x5x4 box centered on the wall hit,
once per press, without firing the selected weapon. It affects only outer
room0; the central structure remains static. Up to8 successful cutters are
retained; capacity rejection leaves the previous terrain and collision intact.
Reloading the developer room restores it; an in-session reset control is next.
This is an explicit gameplay test tool, not retail explosive GeoMod behavior.
Generated/fragmented outer surfaces temporarily use the shared fallback shading
without authored lightmaps. Interior UVs are provisional world-space mapping.

The live400-frame comparison turns toward the wall, cuts at frame110, then
walks forward from frame130. The intact run stops at x-15.388512; the cut run
enters the excavation and stops at x-17.382938,y-11.951555,z5.566381. Both stay
alive. tools/dev_geomod_check.py reproduces these inputs and checks state.
The existing ammo-refill replay also passes. PC images of the approach and
inside the excavation were inspected; this is now actual gameplay geometry.

Xbox artifacts/xemu/render-20260915-073815 passes all35 comparisons at400frames,
including body position inside the excavation. The native final framebuffer
was inspected and matches the excavated interior, weapon and HUD. Endpoint
free memory is9489 pages (37.0664MiB) on the64MiB target. All19 staged disc
entries were checked against their saved bytes and restored. No host input or
desktop capture was used; no GitHub screenshots were added. Audio and full
weapon animation sequences were not reviewed in this check.


Repeated excavation verification: tools/dev_geomod_check.py --extended adds a
held-chord400-frame control and a720-frame/four-cut route. Holding Use+AltFire
from frame110 creates exactly one cut and reaches the same position as one
press. Separate presses at110,400,500,600 produce four retained cuts and let
the player walk to(-23.391994,-14.450785,.282855), beyond the first excavation.
Both remain alive. The PC final images were inspected; interior walls, handgun
and HUD remain visible. PC peak GeoMod owner+overlay accounting is465106bytes
for this route, excluding external render allocations and allocator metadata.

Xbox artifacts/xemu/render-20260915-074302 completes the720-frame/four-cut
route with all35 PC comparisons equal. Its native final image was inspected.
Endpoint available memory is9473 pages (37.0039MiB); all19 disc entries match
the saved pre-run bytes after restoration. The held-chord control was PC-only.
This verifies repeated live box excavation and locomotion, not new blast
shapes, explosion damage, audio, lighting parity or an in-session reset.
