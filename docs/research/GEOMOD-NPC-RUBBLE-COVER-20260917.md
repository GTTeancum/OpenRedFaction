# Live NPC shots intercepted by real extracted rubble

The opt-in `RF_REPLAY_DEV_NPC=2` / `dev-npc.flag` byte2 fixture loads authored
L1S1 actor8456 (env_guard) into the ctf06 developer room. Default DEV remains
empty; existing mode1 remains the harmless walking miner control.

The first600 frames use the ordinary three-rocket connected-beam recipe from
`tools/probe_beam_center_fragments.py`, target index1. NPC combat is disabled
during preparation and the actor starts outside the blasts. At frame600 the
actor is staged across from the player, and the first live radius>1 fragment
is translated to the midpoint between their eyes and held there. Existing
geometry, orientation, collision, registry identity and health remain intact.
The fragment is explicitly retired by a synthetic400 damage stimulus at810.

This placement/holding/removal is a controlled experiment, not natural fragment
support or an NPC destruction feature. Ordinary NPC awareness, aiming, spread,
shot scheduling, authored handgun damage, firing animation and player damage
run through the production paths. No direct call substitutes for an NPC shot.

The PC acceptance observes three blocked shots at630/690/750 with player health
100, followed by player damage from shots810/870/930 after the same fragment is
retired. Six records at660..960 capture shots, damage events, blocked rays,
player health, live fragment state and source/piece identity. A separate781-frame
PC capture was inspected: the beam spans the firing line and obscures the guard's
upper body, with room, launcher and full-health HUD visible. It intentionally
floats because the fixture holds it there. No screenshot was uploaded to GitHub.

## Save boundary

An initial attempt to load a player checkpoint with an NPC correctly failed at
AUTHORED_SCOPE_CAPTURE_ERROR -4: the current save profile excludes mutable NPCs.
That restriction is unchanged. The accepted fixture creates rubble live and
rejects player-checkpoint mode; it introduces no save-scope exception.

## Reproduction

Prepare artifacts/beam-center-fragments/1.bin with the existing beam recipe,
then run `python -B tools/check_npc_rubble_cover.py`. Its output is under
artifacts/npc-rubble-cover. The generated live.bin contains962 simulation ticks.

Native command:

```text
python -B tools/xemu_render_check.py --npc-rubble-test --dev-room --spawn --level ctf06.rfl --archive levelsm.vpp --authored-source 95 --authored-sources 3 --input artifacts/npc-rubble-cover/live.bin --seconds 240
```

The native harness saves/restores dev-npc.flag, enforces a single project XEMU,
compares all48 fixture words against PC and asserts the covered/uncovered
outcomes independently. No host input or desktop capture is used. All123 PC
regressions pass in artifacts/npc-rubble-tests.log. Native run artifacts/xemu/render-20260917-223741 passes76 checks, including
all48 cover-state words exactly matching PC. Stock64MiB endpoint free memory
is3009 pages (11.754MiB). The native final framebuffer was inspected: the cover
is gone, the guard/room/launcher are visible, and damage tint plus reduced HUD
health agree with the three admitted hits. The covered-stage image was inspected
on PC; the Xbox covered stage is established by sampled state, not a captured
intermediate Xbox image. Audio was disabled; audible quality remains unverified.
The harness restored the disc and closed its owned emulator.

Remaining: naturally resting cover layouts, other angles/fragment shapes,
unsupported NPC weapons and live high-damage NPC retirement/subdivision. This
fixture does not establish those behaviors or audible-quality acceptance.

The existing harmless mode1 miner movement/contact-exclusion control still passes (artifacts/npc-rubble-harmless-control.log).
