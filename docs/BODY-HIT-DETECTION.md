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

The helper assumes valid finite body spheres and orthonormal orientation,
as supplied by the physics body. It does not implement animated limb hitboxes,
per-part damage, or new enemy interception/target-selection behavior.
