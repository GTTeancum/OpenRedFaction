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
ammo capacities. Normal campaign mode does not receive these grants. Restart
the room to restore its starting supply; an in-room reset control is still open.
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
