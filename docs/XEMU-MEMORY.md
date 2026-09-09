# Guest memory evidence

Current result: the memory harness passes a fresh stock 64 MiB XEMU run,
`artifacts/xemu/20260909-165657-464529/report.json`. QMP reports 67,108,864
base-memory bytes and zero plugged memory. All 468 actor configuration bytes,
64 rendered geometry records, input records, stance records and movement-setting
records match PC. The fixture lands at frame 22 and completes 63 updates without
capped passes. The supplied screenshot's NXDK `strtod` assertion is fixed.

Remaining assumptions: diagnostic initial pose/inertia and scripted animation
and input. Ground traction now uses the authored support surface. This route observes no movement contacts,
no blocked standing and no support loss, so it does not establish those paths.
PC agreement establishes cross-platform consistency; original-instruction
comparisons described below provide separate evidence for reconstructed routines.
Neither establishes a complete playable campaign or full-game memory usage.
Sections below are chronological evidence; later corrections supersede earlier
integration claims, especially the descriptor 1 run-dispatch correction.

Reproduce the memory-only actor run from the repository root with the matching
built XBE/ISO and actor-drive disc flags:

```powershell
python tools/xemu_smoke.py --actor-drive --no-capture --seconds 180
```

The run writes its report and complete guest snapshot under a timestamped
`artifacts/xemu/` directory. No new framebuffer was captured for this rerun.

Use QMP guest-memory inspection without host input or desktop capture. The
pattern follows the UT99 Xbox RAM poller reviewed at
`C:/Programming/GitHub/UnrealTournament_1.40/UT99-Xbox/Tools/poll_xemu_ram_log.py`;
this implementation uses the existing local QMP transport rather than copying
that poller's HMP socket implementation.

`tools/xemu_smoke.py` saves `guest-memory-latest.json` every 15 seconds and
`guest-memory-complete.json` after the actor comparison. Snapshots include guest
CPU registers, current instructions, stack words, stage/status words, the full
468-byte actor configuration, the 324-byte body owner and its sphere allocation.
Addresses come from the matching NXDK linker map. The owner byte count includes
the inline body state; three separately allocated sphere records make total
accounted body storage 396 bytes. Allocator overhead is excluded.

For an already-running harness instance with its QMP port available:

```powershell
python tools/xemu_guest_snapshot.py --port 46271 --map build/xbox/main.map --out artifacts/guest-memory-manual.json
```

Use only the map belonging to that running XBE. The tool reads guest state; it
does not launch, stop, resume, send input, or change guest memory. The smoke
runner owns its emulator process and reads snapshots itself, so use its output
while it has the monitor connection. A snapshot is not atomic while the guest
runs; completed fixture comparisons use stable final fields. No arbitrary
memory-scan success is accepted as proof of object identity.

On 2026-09-09, the user screenshot exposed NXDK `strtod` asserting in
`lib/xboxrt/libc_extensions/stdlib_ext_.c:91`. Both `strtod` and `strtof` in
the installed NXDK are stubs. Shared table readers now parse bounded decimal
tokens directly, avoiding those calls on both platforms. Installed coefficient
comparisons pass; arbitrary decimal-to-binary correctly-rounded conversion is
not claimed. This failure demonstrates why a successful NXDK build alone is
insufficient evidence of runtime support.

Verified run: `artifacts/xemu/20260909-142439-773475/report.json` is PASS on
64 MiB XEMU. Guest configuration matches PC across all 468 bytes; see
`actor-config-comparison.json`. Memory confirms miner1 mass 100, Flesh index 3,
run index 1, use-kind 9, primary flags 0x0102411f, secondary flags 1, and three
spheres. All eight body diagnostic words match PC after 64 rendered frames.
Existing 600-frame door/world comparisons also pass. No image was captured
because the physics-state change has no new visible appearance.

Still provisional: scripted frame-zero pose, identity initial local inertia,
zero initial velocities, cached frame-zero colliders. This does not establish
original spawn order, crouch collider switching, actor collision response, AI
or gameplay registry integration. The next milestone is connecting the body to
world collision and actor movement while keeping these assumptions explicit.

The stationary-world extension retains 18 ordered sweep records (80 bytes each)
for the miner's three actual body spheres, testing +/-2 units along each world
axis. The shared world query uses diagnostic filter 0x460; this is not evidence
of the original actor movement policy. Xbox reuses its resident collision world.
The fixed diagnostic record capacity costs 3,840 bytes, with no query allocation.
Snapshots include raw records and decoded contacts; the smoke runner compares
the eight summary words, including the complete ordered-record hash, with PC.
This checks collision queries without moving the body or changing its pose.

Verified extension run: `artifacts/xemu/20260909-143310-870614/report.json`
passes in 64 MiB XEMU, including all 468 configuration bytes, body telemetry,
18 sphere queries and the 600-frame door regression. The 1,440 raw guest sweep
bytes hash to `e74ccb35`, matching PC. Two negative-Y sweeps hit face 5336 in
room 53, with normal approximately (0.178028, 0.981902, -0.064604): sphere 0
at fraction 0.3641684 and sphere 1 at 0.6669140. For the two-unit displacement,
the nearest contact is about 0.728337 units away. This is evidence for the
current diagnostic pose, not validation of original spawn/grounding behavior.
Both builds and all four CTest checks pass. No new image was warranted.

## Falling state fixture

`rf_physics_fall_propose` reconstructs original 49e8b7..49e9e6 after steering
and horizontal speed limiting. It divides force by mass, subtracts gravity on
Y, updates velocity, then proposes position using updated velocity plus support
velocity, less half acceleration times dt squared. Intermediate vector stores
matter. `tools/verify_physics_fall.py` executes that original block and unchanged
callees, matching 256 prepared inputs against PC and actual NXDK machine code.
This is not coverage of the omitted steering, movement-mode dispatch or contact
response. Default gravity 9.8 is read from original image address 5a00dc.

Original 49e9db writes entity+f0 (body+68); 49d0a0 and 49d280 copy that value
to entity+e4 (body+5c) on accepted movement. The body fields formerly called
previous_position/previous_orientation are now next_position/next_orientation;
this naming correction changes no layout or initialization bytes.

The integrated fixture copies the retained miner state and makes up to 120
passive falling proposals at 1/60 second. It tests every sphere against the
resident stationary world for each proposal, accepts unobstructed translations
and rebuilds bounds, stopping before the first contact. No additional heap
storage is allocated. Guest snapshots retain all 308 bytes of the final fixture
state and eight summary words. This uses zero steering/support velocity and
diagnostic mask 0x460; it does not move the rendered miner or implement collision
response. The next required path is original 49fe40 (contact fraction and
remaining time), 49d7e0 (actor response) and the actor pose/room commit.

Verified run `artifacts/xemu/20260909-144003-824190/report.json` passes on
64 MiB XEMU: 23 unobstructed falling steps, then sphere 0 contact during the
24th proposal at fraction 0.12988822. Retained pre-contact position Y is
-3.91096449 and velocity Y is -3.75666738. All eight words and the state hash
match PC; configuration, body, world sweeps and the 600-frame door regression
also pass. The displayed scene is unchanged, so no screenshot was captured.

## First contact velocity response

`rf_physics_static_contact` implements 49dc1d..49dcf1: the non-rotating actor
branch for a non-liquid contact with zero inverse mass, no resolved other
object, and body flag 0x80 clear. It computes contact-normal speed minus the
actor-plus-support normal speed, scales that difference by original constant
1.1 (589460), and adds the resulting normal correction only when it points
outward. In that case it clears angular velocity (body+c8). The signed impact
speed is returned for the separate damage path, which is not implemented here.
This branch is not the general moving-object, liquid or ground-constrained
response. The falling fixture explicitly selects these contact assumptions.

`tools/verify_physics_contact.py` executes complete original 49d7e0 with its
unchanged callees and compares velocity, angular velocity and signed impact
speed for 256 low-speed fixtures on PC and NXDK. An observation hook reads the
argument at 49cd80 without replacing it. The low-speed fixtures do not exercise
damage. This validates the selected response branch, not every actor response.

`tools/inspect_actor_contact.py <guest-memory-complete.json>` reconstructs the
passive fixture's incoming velocity from its initial guest body and step count,
uses the captured contact normal, executes the original response with a
synthetic falling descriptor (mode 2), and compares original, PC and guest
velocity. It writes the original call trace to `artifacts/actor-contact-original.json`.

Run `artifacts/xemu/20260909-144607-858143/report.json` passes on 64 MiB XEMU.
The actual sphere-0 floor contact has signed impact speed 3.84905815 and
response velocity (0.75376487, 0.23733878, -0.27352908), matching original and
PC. The fixture retains pre-contact position and proposed next position and
stops after this one velocity response; it does not yet continue the remaining
frame or move the rendered actor. Full builds, all four CTests, configuration,
body/world comparisons and the 600-frame door regression pass.

Next integration evidence: original 49ffd2 computes remaining time from the
unadjusted hit fraction; 4a002b..4a005c subtracts the 0.05-unit separation margin
(5894f4) from traveled distance, clamps the adjusted fraction at zero, and
4a0060..4a0077 advances position. Flag 0x400000 bypasses that margin. Preserve
this distinction when connecting position clipping and repeated substeps.

## Contact position and remaining time

`rf_physics_contact_advance` now implements 49ffd2..4a007c for the non-0x4000
actor branch and raw hit fraction below one. It updates position and the stored
adjusted fraction and returns remaining time calculated from the original raw
fraction. `tools/verify_physics_advance.py` matches 256 original instruction
executions on PC and NXDK, including support-flag bypass and zero-clamped
separation. No original callee is replaced. The port rejects zero displacement
and nonfinite inputs instead of propagating undefined fractions.

Integrated run `artifacts/xemu/20260909-145109-832802/report.json` passes on
64 MiB XEMU: adjusted fraction 0, remaining time 0.014501864090561867 seconds.
The harness compares both words directly with PC, along with the full fixture
state hash and existing response/scene/door comparisons. Both builds and all
four CTests pass. This still stops after the first contact; no visible change.

The next control-flow target is 487770, which calls actor proposal 49f3c0 at
4877c9 and actor completion/response 49fe40 at 487935, with collision processing
between them (48ca60 and per-sphere-mode 49bb70). It loops over remaining
objects/time, commits via 48a230, and has iteration/time termination guards.
49f3c0 sets flag 0x1000000 after preparation; falling helper 49e750 skips force
and gravity integration when that flag is already set, using zero acceleration
for the next proposal. Do not reuse the current first-pass falling API for
remaining-time passes without implementing that distinction.

## Remaining-time pass

The falling API now honors flag 0x1000000: preserve incoming velocity and use
zero acceleration. Its caller manages the flag per frame. The original repeated
branch is checked by `tools/verify_physics_fall.py --repeat`, which executes
complete 49e750 with the flag set and unchanged callees. All 256 cases match PC
and NXDK; the 256 first-pass cases still match. No float layout changed.

The integrated copied-body fixture now finishes its first collision frame.
After contact response it sets the flag, proposes movement over the remaining
time and queries all three spheres again. Clear movement commits position and
rebuilds bounds; further contacts clip/respond before another pass. It follows
487770's pass-index termination checks (>3 with remaining time below .25, or
>9). These are guards, not proof of whole-world scheduler reconstruction:
moving bodies, shared collision batches, actor registration, room/pose commit
and rendering are still separate work. The diagnostic records four timing
floats: first adjusted fraction, first remaining time, final remaining time,
and total pass count. PC requires the current fixture to consume all its time;
XEMU compares every word and the final state hash.

Run `artifacts/xemu/20260909-145630-453926/report.json` passes on stock 64 MiB
XEMU. The first collision frame completes in two passes with zero remaining
time. Final copied-body position is (-92.39078522, -3.90752268, 49.75749588),
matching PC. Configuration/body/world and 600-frame door comparisons pass,
as do both builds and all four CTests. No image was captured because this
fixture is still separate from the rendered actor's pose.

## Body pose in the rendering transform

`animation_run` now selects an attached open body's current position and
orientation for `rf_model_local_view` on every frame. With no open body it uses
the explicit placement, including the frame-zero setup before body creation.
This connects rendering to body state without copying a stale spawn position
into the view transform. It does not integrate physics or mutate the body.

The PC scene check renders the completed collision-frame body pose and compares
all 1,407 output vertices with an explicit placement at the same position and
orientation. The explicit starting position differs from the body position;
the test also confirms rendering preserves the body's state. Both paths use
the existing shared skinning, transform, clipping and projection code.

The live diagnostic currently computes motion on a copied body. Applying that
completed motion to the live actor remains necessary for visible motion. The
needed position setter already exists: `rf_group_pose_set_position` implements
48a230's public/current/pending position, radius bounds and object dirty-bit
writes, verified in `tools/verify_pose_position.py`. Reuse that implementation
for the actor's pose fields. This function itself does not perform room lookup;
do not invent a room dependency inside that setter.

Run `artifacts/xemu/20260909-150147-292287/report.json` passes in stock 64 MiB
XEMU with the body-backed transform, alongside the existing motion/body/world
and 600-frame door checks. Both builds and all four CTests pass. This run proves
compatibility of the live attached-body render path; the moved-body vertex
comparison above is PC evidence. No new visible result was captured.

## Visible body trajectory diagnostic

`rf_scene_stream_miner_body` now connects the computed trajectory to the live
scene body. At frame zero it queries the caller's existing stationary collision
world and retains the passive-fall states through the first collision frame.
After each rendered frame it applies the next state, using the already verified
`rf_group_pose_set_position` to synchronize current/public/pending positions,
bounds and the dirty flag. The actor then holds its final position. The existing
scripted animation sequence continues; this is a diagnostic replay, not a
continuous actor update, AI decision or grounded-state selector.

The fixed trajectory buffer holds at most 121 body states (37,268 bytes);
the 64 render records cost 1,280 bytes. No extra Xbox collision world or per-frame
heap allocation is introduced. Original animation, class and inertia assumptions
remain explicit. The pose wrapper is port-owned diagnostic storage, not a fully
constructed original entity or registered gameplay object.

Xbox selects this mode with `build/xbox/disc/actor-body.flag` together with the
existing scene-stream and scene-states flags. Omit door-view/door-motion flags
to see the actor. PC scene checks use `--body`; the software renderer uses
`--scene-body-last` with the same arguments as `--scene-states-last`. Validate
with `python tools/xemu_smoke.py --actor-body --reference artifacts/actor-body/pc.ppm --seconds 180`.

Native capture `artifacts/xemu/20260909-150715-901303/framebuffer.png` shows the
live body at the post-collision position. The PC/XEMU image comparison passes:
12 of 307,200 pixels differ by more than three channel levels; average maximum
channel error is 0.03219. This is approximate raster agreement, not pixel identity
or proof of PS2 visual parity. The follow-up telemetry run
`artifacts/xemu/20260909-150918-144569/report.json` confirms that all 64 actor
vertex counts and full geometry hashes match PC, not just the final frame.
Both builds, the ordinary PC scene regression, cancellation/capacity checks and
all four CTests pass. Subsequent verification runs skip redundant screenshots.

Guest snapshots now include the initial trajectory state, live body, pose wrapper
and every render record. The original-contact inspector uses the initial state
when available, since the live body now moves. The smoke runner checks that
current/public/pending positions and bounds agree and that the dirty bit is set.
Run `artifacts/xemu/20260909-151037-156317/report.json` passes these pose checks
and all 64 frame comparisons. The original-contact inspector also passes using
that run's initial trajectory state and captured normal/response.

## Live per-frame passive updates

The actor-body mode now calculates each physics update from the live state
between rendered frames. It clears the prepared-pass bit once per frame,
applies force/gravity on the first pass, preserves velocity on subsequent
passes, sweeps the actual model spheres, clips contact position/time, applies
the verified static response and commits pose/bounds. An endpoint at fraction
one follows the original clear-movement path rather than entering the
fraction-below-one contact routine. The existing pass guards remain in force.
There is no precomputed trajectory or post-contact hold. The 37,268-byte
trajectory array is removed; only one initial state and eight tick counters
are retained for verification. Rendering frame 63 does not advance an unseen
64th physics step, so the final body and rendered pose describe the same time.

Run `artifacts/xemu/20260909-151445-192163/report.json` passes on 64 MiB XEMU:
63 updates, 91 passes, 28 contacts, zero capped frames, maximum two passes.
All 64 actor geometry hashes/counts match PC. The final framebuffer comparison
passes with 15 of 307,200 pixels exceeding three channel levels and average
maximum channel error 0.03161. Both builds and four CTests pass. The miner's
changed slope position is visible in that run's native `framebuffer.png`.

This remains a 64-frame passive-physics diagnostic, with scripted animation,
zero steering and stationary-world contacts. It does not implement original
grounded movement, continuous game scheduling, AI or the full entity lifecycle.
The observed slope drift is not evidence of correct grounded behavior. Next,
recover the post-physics support/ground transition through 487e00 / 4a0840 and
the movement descriptor changes before extending sustained gameplay updates.

Correction to the earlier reference-test description: synthetic mode 2 was
not established as falling. Original 42a020 compares descriptor+4 with 3 and
8 at 42a036/42a03b; mode 3 is now used for the falling fixture. Re-running all
256 original/PC/NXDK contact cases and the captured first contact with mode 3
passes unchanged. Earlier mode-2 fixtures supported the selected response
branch, but must not be cited as proof of falling-mode selection.

## Ground-query preparation from captured poses

Run `python tools/inspect_actor_ground.py artifacts/xemu/20260909-151445-192163/guest-memory-complete.json`.
The harness executes original 4a0840 and unchanged callees, stopping at 4a0a57
before 499ed0 runs. It supplies an already-constructed reusable query body with
reserved sphere capacity, avoiding allocation without replacing any callee.
All 64 guest positions and actual retained spheres are checked in four prepared
cases: falling mode 3 with zero/positive support velocity, and synthetic mode 0
with zero/negative support velocity. These descriptors are harness inputs, not
movement states recovered from the Xbox guest.

All 256 cases pass byte comparisons for start/end, selected sphere, identity
orientation, broad bounds, radius, collision flags and initial hit fraction.
The original selects the lowest sphere **center Y**, not center-minus-radius:
miner sphere 0 has center Y -0.23152138 and radius 0.60000002. Start Y rises
0.05 and additionally dt times positive support Y velocity. Falling end Y drops
0.10; the non-falling path drops dt times class speed plus 0.05. That latter
expression stays in x87 extended precision through subtraction; prematurely
rounding the depth to float failed at captured frame 1 and is not equivalent.

The temporary body's collision flags are source flags with 0x1000 cleared.
They are zero in this guest fixture; the existing live sweep's diagnostic mask
0x460 is therefore not established as original ground-query policy. 499ed0
derives additional flags internally, so its complete policy still needs tracing.
This harness does not run the world query, decide support, apply landing damage,
or change movement descriptors. It adds evidence without claiming the observed
slope drift is fixed. No new framebuffer is warranted by this inspection.

## Shared ground probe and stationary-world integration

`rf_physics_ground_prepare` now reconstructs the preparation in shared C.
499ed0 derives query flags as `(body_collision_flags & ~0x1000) | 4`, adding
0x100 when the temporary body's broad radius is below 0.05. The miner's fixture
therefore uses query flag 4, not the earlier passive sweep mask 0x460. The
query uses the selected sphere's radius and identity-oriented center; the
broader `abs(center_y) + radius` remains a body-bound value.

Actor-body mode issues this stationary-world probe once per rendered frame
without changing movement. Each 132-byte record retains all 84 preparation
bytes, the complete 44-byte world hit and match flag. The fixed 64-record array
costs 8,448 bytes plus 32 summary bytes; queries introduce no allocation or
second Xbox collision world. QMP captures the records and summary, checks their
raw hash, and compares the full record hash with PC.

Run `artifacts/xemu/20260909-152946-815112/report.json` passes on stock 64 MiB
XEMU. All 64 records match PC (`d8c768e6`), with 42 hits satisfying the original
normal-Y threshold of 0.5, first at zero-based frame 22. All 64 rendered actor
geometry comparisons and existing live physics counters still match. Both
builds, scene cancellation/capacity checks and four CTests pass.

`inspect_actor_ground.py` now compares shared PC C output with all 256 original
prepared cases and checks the 64 actual Xbox preparation records against the
original. Both comparisons pass on this snapshot. The synthetic non-falling
and support-velocity branches are PC/original checks; only the falling, zero
support branch ran in XEMU. Moving support objects, player-specific queries,
damage, landing mode changes and grounded movement remain open. Support exists
before the passive fixture's first collision, so waiting for that collision
alone is not a faithful substitute for the original support update. No new
screen was captured because this change only observes support.

## Static landing and grounded idle

Shared `rf_physics_static_land` applies the static support position clamp from
4a0b31, copying next/current position and rebuilding bounds. It clears support
flag 0x400000 and the ordinary landing path's 0x200000 flag, and clears vertical
velocity while preserving horizontal components for zero support/contact
velocity. `verify_actor_landing.py` executes 4a0b31..4a0bfa and the original
419901..return transition with actual callees and a prepared registered run
descriptor. All 42 captured contacts match PC and NXDK machine code across the
complete shared body state. Sound/damage code before the transition is excluded.

The same verifier runs original 49f646..49f8aa with zero steering, force, support
velocity and velocity. It confirms unchanged position and zero velocity for all
42 cases. The live scene implements this idle case after landing; nonzero force
or velocity returns an explicit unsupported-fixture error. It does not silently
freeze a generally moving grounded actor. The scene also requires authored run
index 1, the ordinary class flag and no special landing-class flag. It retains
the existing scripted animation and initial collider assumptions.

Run `artifacts/xemu/20260909-153645-768066/report.json` passes: one fall-to-run
transition at zero-based frame 22, 41 idle updates, 63 total updates, 22 falling
passes and zero collision impulses. The earlier isolated first-contact fixture
still checks collision response independently. All 64 rendered meshes and
ground records match PC. Native `framebuffer.png` shows the actor remaining
centered instead of sliding down the slope. Its PC comparison passes with 10
of 307,200 pixels exceeding three channel levels (mean maximum channel error
0.03133). Both builds, cancellation/capacity checks and four CTests pass.

This is static landing for the passive miner, not the complete 419830 lifecycle.
General grounded acceleration/friction, descriptor ownership, contact material
state, animation selection, landing sound/damage, moving support and support
loss remain open. The diagnostic continues recording falling-depth probes after
landing for comparison; those records are not the original grounded scheduling
policy and do not repeatedly apply the landing transition.
The follow-up run `artifacts/xemu/20260909-153914-036824/report.json` passes
after adding the unsupported-class guard; it skips redundant framebuffer capture.

## Prepared grounded motion replaces the idle shortcut

`rf_physics_ground_propose` reconstructs 49f7c3..49f89f after steering and drag
selection. Acceleration subtracts stored drag-times-velocity and adds stored
force/mass; velocity adds stored acceleration-times-dt. Proposed position adds
updated velocity plus support velocity times dt, then **adds** the stored
acceleration-times-half-dt-squared term. The original falling block subtracts
that last term. These paths deliberately retain their distinct arithmetic.

`tools/verify_ground_motion.py` compares 256 prepared cases against original
instructions and unchanged callees, then actual compiled NXDK machine code.
Both pass byte-for-byte, including nonzero steering, force, velocity and support
velocity. Half the fixtures use zero prepared steering, as required on repeated
passes. This verifies the prepared proposal, not the omitted steering transform,
special movement-mode drag selection or collision policy.

The live ordinary-run fixture now uses this proposal with zero steering and
`max(0.5, response/mass)` drag from original 49f79a, followed by the common sweep,
response and remaining-time loop. Force is cleared after proposal as in the
full 49f3c0 path. Zero displacement skips the world query, matching 4df1c0.
It no longer rejects nonzero force/velocity or substitutes an idle assignment.
General support maintenance and loss, moving supports, AI steering and input
selection remain open; zero-input live behavior alone does not validate those.

Run `artifacts/xemu/20260909-154308-802121/report.json` passes on stock 64 MiB
XEMU: all 64 actor geometry records match PC, one landing at frame 22, 41
grounded updates and 63 total proposal passes with no capped frames. Both builds,
the integrated scene checks and all four CTests pass. No new image was captured:
the zero-input miner remains planted, as in the preceding landing capture.

## Authored movement descriptors and axis transform

`rf_movement_descriptor_load` reads the six reference fields in movemodes.tbl
into the original 32-byte descriptor layout (enabled, index, three translation
references, three rotation references). Original 433670 maps translation names
through 5963c4 (`none`, `eye`, `body`, `parent`) and rotation through 5963d4
(`none`, `eye-obj`, `body-obj`, `body-world`); unknown references become zero.
The bounded loader requires all six fields, rejects duplicate/malformed fields,
and uses one temporary allocation capped by the caller's budget. It leaves the
output unchanged on error. It is not a reconstruction of the complete original
file-parser lifecycle.

`rf_movement_transform` reconstructs complete original 433a50: each axis selects
the corresponding eye/body/parent matrix vector, or zero for other selectors,
then 4faa90 transforms the input with float output stores and no normalization.
All 256 original/PC/NXDK cases pass, including every combination of the four
axis selectors and invalid selectors. The same verifier checks all 16 installed
descriptors against authored fields and original name/reference arrays. The
original file parser is not executed in that binding comparison.

The scene now loads run and fall descriptors and exposes both through
`rf_scene_actor_movement` (64 bytes). Run and fall translate on body X/Z with Y
disabled, and use rotation references 1/3/0. Grounded zero-input updates call
the shared transform before the motion proposal. The fixture permits only
body/disabled translation axes: eye and parent poses must be owned correctly
before those modes are enabled. Class acceleration scaling/clamping, real input,
rotation application, AI and grounded support maintenance remain open.

Run `artifacts/xemu/20260909-154924-008044/report.json` passes in stock 64 MiB
XEMU. All 64 authored descriptor bytes match PC, as do all 64 rendered geometry
records, landing, ground probes and live proposal counters. Both builds and
four CTests pass. No new framebuffer was captured because zero-input behavior
is visibly unchanged.

## Class acceleration and steering clamp

`rf_entity_movement_load` binds class fields +50..5c from entity.tbl. Original
41be39..41bea0 reads `$Max Vel:`, optional `+slow factor:` and `+fast factor:`,
then `$Acceleration:`. The missing-factor writes use ESI=0x3f800000 set at
41bd0d, so both factors default to 1. The bounded shared reader rejects missing
required fields and negative/nonfinite values, preserves output on failure,
and uses a temporary archive-sized allocation within the supplied budget.
This is numeric binding, not the entire original class parser.

Miner1 resolves speed 6, slow factor .5, fast factor 1.5, acceleration 20.
Five parser fixtures check default factors, a fast-only override, missing
acceleration, negative acceleration and malformed factor syntax.

`rf_movement_acceleration` reproduces 49f6cd..49f753: store input times class
acceleration, transform through the selected axes, measure transformed length,
then scale by a stored float ratio when length exceeds class acceleration.
All 256 original/PC/NXDK cases pass, including disabled axes, mixed reference
frames and zero acceleration. Grounded scene updates now call this stage with
authored acceleration. Input remains zero; repeated-pass nonzero-input policy
still needs integration when a real input source is added.

Run `artifacts/xemu/20260909-155517-851835/report.json` passes in stock 64 MiB
XEMU. All four class values match PC in guest RAM, as do both movement
descriptors, 64 rendered geometry records, ground probes, landing and motion
counters. Both builds, the integrated scene checks and four CTests pass. No
new screenshot was taken because the zero-input appearance is unchanged.

## Grounded support maintenance

`rf_physics_static_support` now exposes the position/bounds portion of landing
without clearing velocity or flag 0x200000. Static landing calls this shared
portion before its one-time velocity/flag changes. Forty-two captured contacts
match original 4a0b31..4a0bfa on PC and compiled NXDK, including preserved
nonzero horizontal and vertical velocity. The original 4a0a5c continuation also
confirms that a fraction-one miss or normal below the walkable threshold enters
4281a0, setting physics flag 1 and selecting ordinary fall descriptor 3.

The live query now selects falling or grounded probe depth from the current
mode, using authored speed 6 for grounded depth. Per-frame modes are captured
in `rf_scene_actor_ground_modes` (256 bytes) and compared with PC. The inspector
checks an additional 64 cases using those captured modes and class speed, for
320 original/PC preparation comparisons, and compares every actual guest probe
with the corresponding original result.

Moved ordinary grounded actors commit static support, or switch to falling
when the query lacks walkable support. The diagnostic movement gate compares
the current pose with its previous rendered pose; original 487e00 also has
special player, attachment and moving-support conditions that remain outside
this fixture. Queries are recorded even when an unchanged static actor skips
the commit. Moving-platform support is not implemented.

Run `artifacts/xemu/20260909-160033-238997/report.json` passes on stock 64 MiB
XEMU: all 64 rendered geometry records and mode-dependent ground records match
PC. Guest telemetry confirms one landing, two subsequent support-position
commits and zero support losses. The 320 original preparation checks pass on
that snapshot. Both builds and four CTests pass. The no-hit/steep transition is
verified against prepared original state, but live XEMU support loss remains
unexercised until controlled motion is added. No new screenshot was captured.


## Process-local steering fixture

Place `actor-drive.flag` beside the Xbox diagnostic XBE (with the existing
scene stream/state flags), rebuild the disc, and run the harness with
`--actor-drive`. This implies `--actor-body` and verifies the enabled flag in
RAM. PC equivalents are scene checker `--drive` and preview
`--scene-drive-last`. No host or application-window input is generated.

The fixture supplies +X .25 on frames 24..47, zero elsewhere, through the
loaded class acceleration and movement descriptor. Only the first proposal
pass applies steering; repeated passes omit it. Falling remains passive.
Guest symbols `rf_scene_actor_drive_enabled` and `rf_scene_actor_input_frames`
record the mode and all 192 input floats. The harness compares every input
word with PC along with the existing body, geometry and support telemetry.

Run `artifacts/xemu/20260909-160442-543906/report.json` passes in stock 64 MiB
XEMU: 63 updates, one landing at frame 22, 40 subsequent support commits,
zero support losses and no capped frames. All 64 geometry and input records
match PC. The captured probes pass 320 original/PC preparation comparisons.
The native guest framebuffer passes comparison with `pc-driven.ppm`: 21 of
307200 pixels exceed three channel levels, with mean maximum-channel error
0.0329134. Both builds, four CTests, and driven/passive scene checks pass.

This demonstrates controlled diagnostic movement over static support. It
does not establish support-loss recovery, moving platforms, player controls,
AI, or locomotion-driven animation. The starting pose, inertia and cached
frame-zero colliders retain their previously documented limitations.


## Grounded contact route and response capture

`actor-contact.flag` selects input profile 2, a sustained -X 1 input on frames
24..62. It overrides `actor-drive.flag`; remove the contact flag and rebuild
the disc to return to the short pulse. Use smoke option `--actor-contact`, PC
checker `--contact`, and preview `--scene-contact-last`. The original short
pulse remains profile 1, and no-input remains profile 0. Input is contained in
the diagnostic process.

`rf_scene_actor_contacts` holds up to 64 records of 25 words (6400 bytes),
with `rf_scene_actor_contact_count` giving the used count. Overflow returns a
diagnostic error instead of discarding evidence. Each record contains:

- Words 0..2: update frame, proposal pass and movement descriptor index.
- Words 3..17: velocity, angular velocity, normal, support velocity and contact
  velocity, each three float words, before response.
- Words 18..24: resulting velocity, angular velocity and impact value.

The snapshot reader captures these symbols. The smoke harness compares the
entire used trace with PC, as well as the input profile, all input words and
existing geometry/support records. `verify_physics_contact.py <snapshot>`
executes original 49d7e0 with each captured input and compares its result with
guest RAM, PC and compiled NXDK. Captured cases stop at the damage call after
reading its argument; no callee is replaced. This verifies the prepared static
response path, not damage, gameplay effects or general object contacts. The
randomized reference suite now covers 256 falling and 256 run cases.

Run `artifacts/xemu/20260909-161509-037641/report.json` passes in stock 64 MiB
XEMU: 63 updates, 92 proposal passes, 29 contacts, maximum three passes in one
update and zero capped frames. All 29 response records match PC and original
instructions, for 541 PC/NXDK comparisons including the 512 synthetic cases.
All 64 geometry hashes and input records match PC; 320 original ground-query
preparation comparisons pass. Four CTests and passive, short-pulse and contact
scene checks pass. The framebuffer comparison passes with six of 307200 pixels
over three channel levels (mean maximum-channel error 0.0299447).

The miner stays grounded throughout the driven part, with 40 support commits
and no support losses. This route does not verify walking off a ledge or
recovery. The fixed diagnostic camera leaves the miner partly outside the
final view; the capture is retained as test evidence, not a gameplay camera
result. Animation, initial pose/inertia and cached colliders remain provisional
as previously documented.


## Live cached stance centers

The scene now owns `rf_scene_actor_stance_cache`, a 200-byte record containing
count, two sets of eight center vectors and the maximum standing-minus-crouch
height difference. During frame-zero body setup, the animation adapter copies
its current playback/resources, stops looping weights, selects the authored
crouch motion and advances .2 seconds. It evaluates the pose, transforms the
model spheres and applies class X/Z suppression. This reuses the previously
verified 423bd0 pose-operation sequence while retaining the diagnostic initial
standing pose as an explicit assumption. Using a copy preserves the existing
scripted animation trace; it does not reproduce the full creation sequence's
mutation and restoration of the live animation state.

Only one temporary matrix allocation is added during cache creation: 48 bytes
per bone (1200 bytes for this miner), freed before frame delivery. The cache
has no heap ownership. Ordinary frames retain cached centers; they do not
resample colliders from each rendered pose.

The scripted controller requests crouched centers after its current state is
8 and its transition duration reaches zero. When standing is requested, the
scene sweeps the current crouched spheres toward the recovered clearance
endpoint using the body's query flags with bit 4. A hit retains the crouched
centers/flag. A successful change calls the verified center-only commit and
forces support refresh even if position did not change. This is stance geometry
integration, not the complete locomotion selector or crouch movement policy.

`rf_scene_actor_stance_frames` contains 64 four-word records: requested stance,
actor flags, full sphere-record hash and whether standing was blocked. Both
the cache and the complete trace are captured from guest RAM and compared with
PC. Scene checks require actual center changes, crouch and subsequent stand.

Run `artifacts/xemu/20260909-162703-696777/report.json` passes in stock 64 MiB
XEMU: stance changes at frames 39 and 48, nine crouched frames, no blocked
standing, all 200 cache bytes and 64 records matching PC. The three standing
center Y values are approximately -.231521, .170284 and .631437; crouched Y
values are -.231521, -.139646 and .251714. Clearance height difference is
.379723. All 29 contact responses and 64 rendered geometry records still match
PC. Passive, short-pulse and contact scene checks pass, along with four CTests
and 288 original/PC/NXDK stance comparisons.

No framebuffer was captured: the PC reference is byte-identical to the prior
contact-route image. These collider changes do not alter its scripted visible
pose. Blocked standing, crouch movement descriptors, stance effects/timing,
original initial-pose selection and full entity creation remain unverified.


## Stance movement settings

The original 428030 crouch/slow setup calls 427450 with numeric mode zero,
then selects physics descriptor 1. Ordinary 4280b0 uses numeric mode one and
resolves the class run descriptor. These are distinct mode fields: crouching
does not select a new physics descriptor. For miner1, the setter writes speed
3 while crouched and 6 while standing. It preserves the existing response
coefficient because class flags do not contain 0x800. Ground-probe depth still
uses class +50 (base speed), as original 4a09b4..4a09c7 does; changing that to
the active slow speed would be incorrect.

The shared scene now constructs movement settings from the authored class
values, initializes them with normal mode, and updates them only after a
successful collider stance change. Failed standing retains slow mode. The
resulting response field is connected to the body's existing drag coefficient.
The fixture supplies no forced action and disables network overrides.

Guest symbols `rf_scene_actor_movement_config` (28 bytes) and
`rf_scene_actor_movement_frames` (64 records of response/speed/numeric mode)
make the complete input and results inspectable. Smoke checks compare all
192 result words with PC; scene checks assert consistency with collider stance.
`inspect_actor_speed_modes.py <snapshot>` executes complete original 428030
and 4280b0 with their callees, using prepared clear-standing/forced-crouch state
and the actual captured class values. It verifies all 64 numeric records on
original, PC and compiled NXDK, and confirms run descriptor/parent selection
and zeroed vertical velocity in those original setup calls. The diagnostic
scene keeps its existing support/landing scheduler for vertical velocity.

Run `artifacts/xemu/20260909-163137-872744/report.json` passes on stock 64 MiB
XEMU, including all speed settings, stance cache/records, geometry and contacts.
The 64 original-wrapper comparisons, both builds and four CTests pass. No new
framebuffer was captured because visible movement is unchanged.

The process-local pulse supplies steering directly. It does not yet implement
the gameplay controller that consumes target speed to generate movement input.
Consequently, this verifies the stored movement settings, not paced crouch
locomotion, player control or AI. No extra velocity cap has been invented.


## Correction: descriptor 1 run dispatch and actual crouch speed

The earlier live integration chose the wrong prepared motion branch. Original
49f674 calls 42a100, which returns true for descriptor indices 1 and 2; this
selects 49e400 at 49f8a1. The previously verified 49f7c3 linear proposal is a
different branch. Its isolated arithmetic comparisons remain valid, but they
did not establish that branch as the miner's run path. The original zero-input
idle comparison could not expose this integration error because both proposals
left that actor stationary. Earlier nonzero live trajectories were diagnostic
results, not faithful original run movement.

`rf_physics_run_propose` now implements 49e400 for run descriptor 1. It takes
transformed input, target speed, class acceleration, support normal/velocity
and resolved traction. The speed/acceleration ratio is stored as float, and
the convergence weight is `1 - pow(.05, dt / (ratio / traction))`. Original
slope projection/asymmetric uphill handling and input normalization precede
target-speed scaling. Velocity converges toward the target with external-force
contribution; the position correction uses the original stored delta and time
products. Repeated passes skip velocity convergence and integrate the existing
velocity. Caller flags and forces remain owned by the scene scheduler.

NXDK initially differed by one float level in the convergence weight. Its
implementation now executes the original x87 logarithm/exponent instruction
sequence with extended precision and restores the previous control word.
`verify_run_motion.py` covers 384 complete original proposals through the
49f646 dispatcher, explicitly observing entry into 49e400. All represented
outputs match PC and linked NXDK, including slopes, force, support, normalized
inputs and repeated passes. The linear helper is now documented as unsuitable
for descriptor 1.

The live scene transforms raw process-local input and calls the run proposal
with its stance target speed and actual ground normal. Traction is explicitly
1 for this diagnostic; `rf_scene_actor_run_traction` exposes it in RAM. Original
467edc loads the material traction field, and 4688a0 retrieves it by ground
material index. Ground-face bitmap/material binding remains open, so the
fixture must not be treated as correct on ice, sand, water or other surfaces.

Run `artifacts/xemu/20260909-164313-033930/report.json` passes the sustained
-X route in stock 64 MiB XEMU. Actual X travel speed is 5.161285 before crouch,
2.806549 near the end of crouch and 4.931488 after standing again under the
same input. The scene regression checks that slowdown and recovery. This
corrected route has zero contacts, so the old requirement to observe contacts
was removed; the contact trace still compares exactly and reports its actual
count. The historical `--actor-contact` option now denotes the sustained route,
not a guarantee of collisions. All 64 geometry hashes and state records match
PC; 320 original ground preparation comparisons also pass on its snapshot.

The short-pulse run `artifacts/xemu/20260909-164452-099965/report.json` also
passes. Its new native framebuffer matches PC with 15 of 307200 pixels over
three channel levels (mean maximum-channel error .0318717). Both runs complete
63 updates without capped passes. Both builds, four CTests, passive and driven
scene checks pass. Initial pose/inertia, correct surface binding, support loss,
blocked standing, full input/AI and entity lifecycle remain open.


## Authored ground surface binding

The scene now resolves the stationary support hit's source face, texture name,
material prefix and authored traction before its run proposal. The body stream
borrows the same source geometry used to build its collision world. A temporary
bounded materials.tbl parse loads ten coefficient records and fifteen installed
prefixes; one byte per level texture remains for the stream, then is freed.
The temporary text (at most 64 KiB) and 2,548-byte palette are freed before
animation streaming. The 240-byte coefficient table and 512-byte frame telemetry
remain available as guest symbols. There is no per-frame allocation.

Original 468740 compares the exact substring before the first underscore,
case-insensitively, against declarations in order. It does not match arbitrary
starting text or remove directories. No underscore or no match yields Default.
The C parser uses fixed bounds of 64 declarations and 31-byte prefixes, rejects
oversized input and preserves output on failure. This is bounded port parsing,
not a reconstruction of the original table parser or bitmap-handle cache.
Original 467edc loads traction; 4688a0 retrieves the selected material value.

`verify_surface_materials.py` executes complete original 468740 with a prepared
registry containing all fifteen authored prefixes, without replacing callees.
All 67 lookup cases match the PC table reader/lookup and linked NXDK lookup.
Five malformed table cases check transactional failure; a duplicate-prefix
fixture checks first-declaration priority. Passing a guest snapshot additionally
compares all 240 coefficient bytes independently to materials.tbl and verifies
all 64 captured traction values against their recorded material indices.

Run `artifacts/xemu/20260909-165657-464529/report.json` passes on stock 64 MiB
XEMU. Every surface record matches PC. Default index 0 is retained before the
first support; Rock index 1 is selected at frame 22 and retained through frame
63. Both have traction 1, so the previous visible trajectory is unchanged and
no framebuffer was captured. Both builds and all four CTests pass. The live
fixture still has no blocked standing, support loss or movement contacts; this
run does not establish those cases or movement across ice and other surfaces.
Moving-object surface policy and original spawn/lifecycle integration remain open.
