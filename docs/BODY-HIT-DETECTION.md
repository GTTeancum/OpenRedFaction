# Body-shape bullet collision

Player and enemy hitscan now use the transformed physics sphere union after
the existing broad bounds rejection. The shared `rf_physics_body_segment`
function accepts finite segments, returns the nearest sphere entry, includes
initial overlap and tangency, and allocates no memory. Target selection keeps
the nearest actor; existing static/mover bullet obstruction uses the precise
entry fraction. No actor-specific exception or friendly-fire immunity is added.

This is a practical gameplay implementation, not a recovered retail hitbox
wrapper or animated mesh collision. It uses existing stance/body transforms.
The broad-only defect was measured in the L3S1 encounter documented in
L3S1-CHECKPOINT.md: a shot missed all miner910 spheres but caused damage.

## Verification and limits

`rf_body_segment_tests` rejects that captured shot and retains guard781's
captured middle-sphere hit. It also checks nearest ordering, segment cutoff,
tangency, translated/rotated bodies and zero-length initial overlap/miss.
Both PC and NXDK builds pass.

Stock64MiB Xbox rifle run `artifacts/xemu/render-20260915-030204` passes
all33 PC/native comparisons, with6326free pages (24.7109375MiB) at endpoint.
Its120frames fire ten rounds but score no actor hits; this proves matching
miss/ammo behavior, not a successful rifle encounter. All18 staged files were
restored and the owned emulator exited.

The separate360-frame L1S1 staged actor run
`artifacts/xemu/render-20260915-030331` passes all33 comparisons: enemy fire
records12shots,9body hits and3misses on both platforms. The player survives.
No player shots occur in this fixture, so it verifies enemy hits, not a player
kill. Endpoint memory is5419pages (21.16796875MiB);18disc entries restore and
the project emulator guard is empty after completion.

The old15510-frame campaign replay no longer reaches L3S1: it dies in L2S2a.
Local evidence: `artifacts/body-segment-campaign/run.log`. Its timing/aim was
calibrated using oversized boxes. Historical checkpoint results remain valid
for their recorded revisions, not proof that the new hit detection passes
that route. Recalibrate normal-input aiming and revalidate the uninterrupted
campaign on both platforms. Do not restore broad-only hits to preserve a replay.

## Opening encounter recalibration

`replay_cover_combat.py` now raises pitch during frames3050..3073 instead of
lowering it. The former ray passed below guard8490's body spheres. The3600-frame
PC run in `artifacts/body-aim-raised` restores four40-damage hits at3080,3112,
3144 and3176, killing8490; nine rounds fired and the normal reload completes.

Stock64MiB Xbox repeats this3600-frame route in
`artifacts/xemu/render-20260915-030826`: all33 PC/native comparisons pass,
including9shots,4hits,1kill,47.19998health and16loaded rounds. Endpoint free
memory is4026pages (15.7265625MiB). All18 staged disc entries restore and
the owned emulator exits. This covers the first guard, not the medical branch.

The4500-frame medical branch also kills5677 with four hits, opens cabinet8552
and collects kit8553. It finishes alive at(25.543123,-4.118479,4.213687), with
13shots,8hits,2kills and16loaded handgun rounds. Health is about33.8 after
25restored; additional enemy hits mean the previous >40health expectation is
obsolete. The updated verifier checks33..34health, exact combat/ammo outcomes
and the consumed kit. Its assertions were rerun against the completed capture
after adjusting that health expectation, with generated input byte-identical.

Corridor clearing remains open. At4255 the approaching guards are still behind
solid geometry: facing their coordinates alone does not give an unobstructed
shot. Native PC framebuffer `artifacts/body-aim-turn/frame.png` confirms the
room wall. Do not treat these partial encounter results as restored section-exit
or L3S1 coverage.

The separate `artifacts/body-aim-corridor-low` probe restores the earlier
downward pitch during4200..4223 and kills5666 at4512, but still dies before
clearing the other pursuers. It is not adopted as a successful route. Aiming,
cover and timing for those remaining guards are the next continuation.

## Restored section exit through movement

The accepted PC continuation leaves the medical room after turning at4256,
instead of waiting for three melee pursuers to reach striking range. It reuses
the northbound movement644frames earlier. No health, damage, cooldown, actor
position or event changes were needed. `tools/replay_area2_body_route.py`
reproduces this4706-frame route: position(27.092039,-4.145080,28.661510),
33.79999health,2kills. Guards5666,5676 and5678 remain alive; clearing them
is not necessary for this section exit. Input matches the executed local
`artifacts/body-corridor-push` fixture byte for byte.

`--exit` appends the existing east-hall and exit inputs. The6806-frame PC run
in `artifacts/body-corridor-exit` naturally crosses5150 into L2S3 at6631,
ending alive with5health and16loaded handgun rounds. The generated input also
matches the executed fixture byte for byte. Final combat counters reset at the
handoff, so they do not establish how many east-hall guards were killed.
Stock64MiB Xbox verification is complete in
`artifacts/xemu/render-20260915-032327`:6806frames and all33 PC/native
comparisons pass, including the natural transition and retained player state.
Endpoint free memory is4402pages (17.1953125MiB). All18 staged disc entries
restore and the owned emulator exits. This verifies the5health route, not
later L2S3 encounters or further aim revisions.

Rejected alternatives remain local evidence: `body-aim-corridor-track` hits
both remaining pursuers but dies; `body-aim-early-cover` changes the second
guard's stopping position and loses that kill; `body-aim-early-second` restores
the kill but still dies on retreat. These are not accepted campaign routes.

## L2S3 continuation under review

Appending the former L2S3 hall inputs to the corrected exit produces an8956-frame
probe (`artifacts/body-area3-hall`). It misses guard2020 and dies; this is not
restored L2S3 combat coverage. The pre-shot snapshot in `body-area3-aim` puts
the player at(58.116360,.788409,75.962776) and the guard at
(57.338871,-1.107309,78.765457). The old forward vector
(-.415501,-.548758,.725412) points left of the guard, and subsequent movement
drops the player off the ledge. Corrected aiming and a ledge stop are being
tested; do not infer success from the prior broad-box route.

The `body-area3-ledge` probe corrects yaw by15frames of ordinary.98turn
input, shortens the downward pitch input by4frames and stops the approach
10frames earlier. It hits2020 once (40damage), but return fire kills the
5health player before the next successful shot. The `body-area3-pass` probe
omits all L2S3 shots but retains the old pauses; it also dies. These establish
that aim correction alone is insufficient and that this guard can acquire
without being shot. Immediate movement through the room remains under test.
The local authored item inventory lists medical kit1931 at
(108.701927,7.130690,60.701210), well beyond this first encounter; no nearby
health supply was established by that inventory.

`body-area3-moving` removes335idle frames before onward movement, with firing
still omitted. It also dies at local455. The next diagnostic target is the
L2S2a eastern chamber: preserve more health there before retrying L2S3, rather
than treating the5health arrival as sufficient for a stationary gunfight.

`body-east-health` stops at5606 before the exit and confirms guard8071 dies
from four shots, while all four shots at5683 miss its body. Guard5683 remains
at100health at(35.189323,-5.141514,49.586697). The player still has33.79999
health here; health falls later during onward movement. A second-guard torso
aim correction is now under test in `body-east-corrected`. Its outcome is not
covered by the completed Xbox run above.

The helper assumes valid finite body spheres and orthonormal orientation,
as supplied by the physics body. It does not implement animated limb hitboxes,
per-part damage, or new enemy interception/target-selection behavior.
