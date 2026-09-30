# Movement settings used by turn animations

## Xbox ordinary NPC idle gravity (2026-09-29)

Ordinary NPCs without a scripted move now recheck support on an eight-frame
stagger. A lost static floor selects the existing falling movement mode; on
following frames, `rf_physics_fall_propose` advances the owned body velocity
with scene gravity, then the body sweeps, commits and publishes its position.
A walkable support query accepts static floors or a registered mover and
restores normal or slow stance. Before restoring stance, the live landing writes its contact and calls
the reconstructed NPC impact handler with actual body velocity, damage/foley
services and a lethal-death handoff. Authored Goto actors now use the same
body-velocity gravity and landing path. Idle NPCs on translating movers now
step each frame: current mover velocity carries the body through a bounded
collision sweep, and the following ground contact refreshes the support
handle/material. This is a first-pass live scheduler; rotating-platform carry,
the complete original wake/landing order and wider authored placement remain
open.

The focused stock-64-MiB XEMU L1S1 run
`artifacts/xemu/npc-death-audio-20260929-184745/` completed 90 frames with
358 idle support checks, five transitions to fall, 76 moving-body frames and
five static landings, with zero idle-ground errors and 4,050 free pages. The
owned-corpse death check also passed in the same session, and the isolated
test disc was restored. These counters verify the simulated state and retained
render submission; visual appearance was not inspected or captured.

After the impact binding, the stock-64-MiB XEMU run
`artifacts/xemu/npc-death-audio-20260929-185532/` still completed 90 frames
with five static landings and five impact-handler calls, zero impact errors,
and 4,049 free pages. These were short falls: none crossed the handler's
damage or sound threshold. A live high-speed impact, including lethal death
presentation, was still unverified at that point; the isolated impact component
has separate original/PC/NXDK and Xbox checks in `docs/DEATH-LIFECYCLE.md`.

The bounded Xbox `--impact-drop` fixture sets L1S1 miner UID 8432 to falling
mode with a 20-unit downward velocity before the normal scene step. It does
not call the impact handler directly. The stock-64-MiB XEMU run
`artifacts/xemu/npc-impact-drop-20260929-190158/` completed 90 frames: six
live impact calls, one lethal hit, one impact-sound request, no impact errors,
one owned death/corpse, and 4,049 free pages. The authored Slay event was not
staged, so this death came from landing. The fixture verifies the live damaging
path without claiming that a natural campaign fall or moving-platform landing
has been exercised.

Authored Goto actors now use the same owned body velocity and scene gravity
when support is lost. Their landing runs the shared contact, impact and stance
path; a lethal landing exits before trying to republish the retired actor.
The bounded L1S1 Goto 9363 fixture activates miner 8432, then seeds downward
velocity through `campaign-npc-drop.bin` at frame 31. Stock-64-MiB XEMU
completed 90 frames in both cases: the nonlethal speed-11 case
`artifacts/xemu/npc-scripted-drop-20260929-191600/` recorded one scripted
landing and one damage event with 4,065 free pages, while the lethal speed-20
case `artifacts/xemu/npc-scripted-drop-20260929-193346/` recorded one scripted
landing, one death, one impact-sound request and an owned corpse with 4,049
free pages. Both restored their isolated test disc. These are synthetic
velocity fixtures; natural fall distances, scripted moving supports and other scripted
movement modes remain open.

The Xbox-only `tools/xemu_npc_platform.py` fixture builds a generated CTF06
platform above the static floor, drops one miner onto it and advances the
ordinary scene for 500 frames. The stock-64-MiB run
`artifacts/xemu/npc-platform-20260929-200024/` recorded one mover landing,
455 grounded mover checks, 60 carry commits and zero idle-ground errors. The
NPC's x position advanced from 9.449 to 12.449 while the platform translated
three units over 60 frames. It finished with 1,678 free physical pages and
restored the isolated test disc. This verifies translational support state and
position in guest memory; rotating support, live player/NPC interactions and
visual appearance were not checked.

`rf_movement_set_mode` in src/core/movement.c reconstructs complete routine
0x427450 and predicate 0x40a210. It returns entity settings as explicit shared
C state rather than writing a raw original entity structure. This is the
routine called by candidate/turn helper 0x41f9f0 with requests zero or one.

Inputs map as follows:

| Shared input | Original source |
| --- | --- |
| forced_action | Entity +75c |
| entity_scale | Entity +98 |
| config.flags | Entity info (+294 pointer), offset +724 |
| config.base_speed | Info +50 |
| config.slow_factor | Info +54 |
| config.alternate_factor | Info +58 |
| config.response | Info +5c |
| override_enabled | Global byte 0x64ecb9 |
| config.override_slow | Global float 0x594590, initial value 7 |
| config.override_normal | Global float 0x59458c, initial value 9 |

A forced_action value other than -1 replaces the request with zero. Request
zero sets entity +8c4 to zero and +8c0 to slow_factor times base_speed, or the
slow override when the global byte is nonzero. Request two sets mode two and
speed to alternate_factor times base_speed; it ignores the global override.
All other requests, including negative values, set mode one and speed to
base_speed or the normal override. The API supplies override values explicitly
because the original reads mutable globals rather than fixed constants.

When config.flags bit 0x800 is set, the routine also updates entity +8c.
Request zero sets it to 3000. Other requests set it to config.response times
entity_scale divided by base_speed, without intermediate float stores. When
that flag is clear, +8c remains unchanged. The field is called `response` only
as a neutral API label; its complete physical interpretation remains unresolved.
The output speed and mode feed later animation selection through +8c0/+8c4.

The C API validates finite configuration, rejects a zero denominator when
needed, and rejects non-finite calculated outputs without partial mutation.
It allocates nothing. Both PC and NXDK build the same implementation.

`tools/verify_movement.py` compares 6,000 original executions with the unmodified
predicate against C. It checks exact bytes of all three output fields and that
no other original entity RAM changed. Cases vary forced actions, all request
classes, descriptor flags, scales and mutable override values. Three C-only
rejection cases check unchanged state. Evidence:
`artifacts/movement-settings-verification.json`.

These settings do not implement movement integration, collision or player input.
The next integration point is the turn helper's combination of action starts,
five deadlines, entity +7bc and this settings routine.
