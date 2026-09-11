# Guest memory evidence

Latest first-person run `artifacts/xemu/20260909-194149-709809/report.json`:
PASS. `--actor-eye` compares the final 64 records of eye input/pose (46 words
each) and all-frame camera/world hashes over 664 frames. Every PC frame also
matches original routines in `tools/verify_scene_eye_view.py`. A new native
framebuffer shows the tunnel from eye height and matches PC within one channel
level. Body-aligned look and scripted input remain diagnostic scaffolding.

Previous run `artifacts/xemu/20260909-193237-875652/report.json`: PASS.
The 664-frame follow profile now compares six initial eye-offset float words
from guest RAM with PC: standing (0, .7854025363922119, 0), crouching
(0, .14921127259731293, 0). `tools/verify_scene_eye.py` separately compares PC
with original loaded-pose evidence. These are computed model offsets, not a
first-person camera binding; no screenshot was captured.

Prior rerun `artifacts/xemu/20260909-191614-644905/report.json`: PASS.

This rerun validates the built diagnostic after the camera-effect RNG changes
(source HEAD `1af5eda`). The complete snapshot confirms 67,108,864 base bytes,
zero plugged memory, and the same map hash recorded in the run report. All 664
frames complete; ten actor telemetry rings and 308 final body bytes match PC.
The actor makes 664 room queries without a miss, moving from room token 54 to
53 (level indices 53 to 52). Available guest memory is 41,054,208 bytes after
upload and 44,212,224 after temporary CPU mesh release. No framebuffer captured.
The report's `actor_live.scope` uses the older generic wording "fixed camera";
for this run `actor_follow` records the actual moving, fixed-offset camera.

Evidence boundaries:
- QMP RAM establishes actual stock-memory execution and the observed allocations.
- PC comparisons establish shared-code agreement; original instruction harnesses
  separately establish the reconstructed routines documented below.
- The frame count, fixture input, and follow camera do not establish playable
  campaign input, original player initialization, or camera collision.
- Reported available memory is sampled diagnostic usage, not a full-game peak.
- The supplied screenshot is failure evidence, not instructions. Its NXDK
  decimal-parser assertion no longer reproduces in this run.

Current validation profile: `--actor-follow --no-capture` runs 664 rendered
frames with a moving diagnostic camera and reads actual XEMU guest RAM. The
harness checks stock 64 MiB, PC-matching body and telemetry rings, world/camera
hashes, bounded CPU/GPU allocations, and support-loss/recovery counts. The
supplied screenshot's NXDK `strtod` assertion is fixed by shared decimal parsing.

Snapshots now include UTC capture time, the SHA-256 of the decoded linker-map
text and QMP memory-size information. The run report also records that map hash
alongside XBE/ISO hashes. These identify the inputs used for interpretation;
they do not independently prove an arbitrary external guest matches the map.
The smoke runner launches its own explicitly selected diagnostic ISO.

Still unverified: original player-view behavior/camera collision, initial class
animation pose, full entity lifecycle and campaign input. Scripted diagnostic
steering and stance eligibility remain scaffolding. PC agreement establishes
cross-platform consistency; original-instruction comparisons below provide
separate evidence for individual reconstructed routines. Neither proves a
playable campaign or full-game memory usage. Historical sections below record
older profiles and limitations; later corrections supersede them.

Current memory-only command (matching built ISO with actor-follow disc flag):

```powershell
python tools/xemu_smoke.py --actor-follow --no-capture --seconds 300
```

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


## Blocked and clear standing against the resident ceiling

`actor_stance_update` now owns the existing scene stance decision and is also
used by a bounded test at frame 47. Original 428abe..428acc returns immediately
when 499ed0 reports an obstruction; sphere-center and flag updates occur only
in the following clear branch. The shared decision follows that ordering.

The test copies the actual crouched actor and its sphere records, sweeps upward
to the first real ceiling, and tries standing from 0.2 units below that contact.
It then tries from a lower clear position. Blocked standing must preserve every
sphere byte, movement setting and stance flag; clear standing must install the
standing centers and normal movement mode. Body ownership, settings and flags
are restored afterward. No allocations or host input are introduced. This is
a prepared world-clearance test on the scene helper, not a traversable low-tunnel
route, and it does not execute the original world-query routine.

Guest symbols `rf_scene_actor_clearance_diagnostic` (32 bytes) and
`rf_scene_actor_clearance_queries` (96 bytes) expose both decisions, sphere IDs,
endpoints, hit normals/fractions and preservation hashes. Run
`artifacts/xemu/20260909-170239-776884/report.json` passes on stock 64 MiB XEMU.
All 32 words match PC. The blocked test hits sphere 2 at fraction 0.41690731
with normal Y -0.96733391; the clear query returns fraction 1 and no sphere.
The original 64-frame actor comparisons still pass. Both builds, four CTests
and passive/short-pulse/sustained-input PC scene checks pass. No framebuffer
was captured because the rendered trajectory is unchanged.

Remaining integration issue: the diagnostic animation schedule can request a
standing pose independently of a refused collider switch. A real movement
controller must keep animation and accepted collision stance consistent. This
test proves refusal preserves collider/settings state, not animation behavior
under a low ceiling. Support-loss traversal and original spawn pose remain open.


## Stance selector integrated before animation advancement

Correction to the preceding proposed animation policy: original 41f7b9 calls
428a60, then 41f7be removes the argument and falls directly into the movement
selector at 41f7c1. It does not inspect the standing result. The claim that this
caller must force a crouch animation after refusal was an inference unsupported
by that code. No such override has been introduced. Broader animation/eligibility
behavior still needs the original caller context.

The live actor now calls the existing reconstructed `rf_motion_select_stance`
from animation selection, applies its requested physics effect synchronously,
then advances the controller. The prior after-animation manual test of current
state/duration has been removed for this path. The callback connects the scene's
real standing clearance, cached sphere centers and movement settings. Eligibility
is explicitly supplied by the diagnostic frame interval 32..47; priority, AI
predicates and candidate selection are still outside this integration. The
standalone animation fixture keeps its existing scripted requests.

Guest `rf_scene_actor_selector_frames` records current/next states, effect,
handled flag, physical flags before/after, clearance refusal and numeric movement
mode for every frame (2,048 bytes). The scene checks one crouch and one stand
effect, stable crouch state at commitment, and agreement with collider records.
The original-order gate observes blend completion on the following frame, so
crouch commits at 40 rather than the former manual frame 39. Stand commits at 48.
This is still diagnostic timing (existing animation and physics time steps),
not proof of original engine frame scheduling.

Run `artifacts/xemu/20260909-170656-795906/report.json` passes stock 64 MiB XEMU:
all 64 selector records, stance and speed settings, surface records, rendered
geometry and the two ceiling-test queries match PC. Both builds, four CTests,
all three PC scene profiles and the 2,292 original stance-decision cases pass
(the latter stops before physics effects and supplies eligibility). No new
framebuffer was captured for this ordering change. Full eligibility, initial
pose/inertia, support-loss traversal and gameplay lifecycle remain open.


## Initial miner tensor corrected

The provisional identity tensor was wrong for the populated-sphere miner path.
Original 42254d..4225e9 constructs and zeroes the 0x98-byte physics parameter
block, writes positive authored mass, position and orientation, and leaves its
local tensor zero. 486da0 imports available model spheres before calling 49ec90.
The model count query 503250/501490 reads the model's sphere count; the miner
has three CSPH records. With positive mass, 49ec90 skips generated-mass tensor
accumulation. A populated list also bypasses the later empty-list fallback.

The empty-list distinction matters: even for positive mass, 49ec90 creates a
fallback sphere when necessary and calls 4fce70, which explicitly writes an
identity matrix. The earlier intermediate inference that positive mass always
implies a zero tensor was too broad. The diagnostic now retains zero for its
positive-mass model-sphere path and uses identity only when no model spheres
exist. Nonpositive authored mass is rejected here until generated-mass creation
is composed; existing standalone generation helpers are unaffected. Class sphere
replacement continues to preserve the resulting tensor.

`verify_actor_initial_tensor.py` executes the original parameter-construction
prefix and complete 49ec90 preparation in 48 cases, with four positive masses,
positions/orientations, disabled sphere mode, empty sphere lists and populated
sphere lists. Storage capacity is preallocated to avoid needing an emulated
allocator; no instructions or callees are replaced. Empty-list cases confirm
identity, populated-list cases confirm zero, and mass is preserved. This is
not full entity allocation, model loading or registration execution.

Run `artifacts/xemu/20260909-171406-829227/report.json` passes on stock 64 MiB
XEMU. The verifier checks all 144 initial/final local and world tensor bytes
in its guest snapshot. All existing actor, selector, surface and clearance
comparisons match PC. Both builds, four CTests and all three PC scene profiles
pass. No framebuffer was captured: this translational fixture has no visible
rotation change. Initial animation pose, full creation ordering/registration,
scripted eligibility and support-loss traversal remain open.


## Constructor animation phase and redundant initial blend

The live actor previously inherited phase .25 from the standalone render fixture.
Original constructor instructions 51af14..51af7e zero phase +1d04 and set generation
+1cf8 to one. Configured actor creation now uses that seed, while standalone
pose-preview fixtures retain their existing setup. A second diagnostic issue
requested a stand-to-stand blend at frame zero. The original movement selector
uses 42a650 before requesting states; the configured actor's scripted fallback
now uses the reconstructed membership query to avoid that redundant request.

The new `rf_scene_actor_initial_animation` guest symbol stores 48 bytes: seed
phase/generation, first controller, updated phase, active count, first tick and
weight. The scene checks phase zero/generation one, current stand with no next
state or blend, one active motion and weight one. The first update reaches
motion tick 320. Full original startup priority/eligibility inputs remain open,
so the choice of stand is still a diagnostic assumption rather than proven
original creation-pose selection.

`verify_actor_animation_seed.py` executes original 51af0b..51af84 with three
initial memory patterns, verifying phase/generation and empty slots. It can also
check the captured actor seed/controller. This covers field initialization,
not the full constructor or resource loading. The existing original membership
and selector comparisons cover the reused redundant-request gate.

Run `artifacts/xemu/20260909-171941-116367/report.json` passes stock 64 MiB XEMU:
all initial animation, selector, collider, surface and geometry comparisons
match PC. Both builds, four CTests and three PC scene profiles pass. Its new
native framebuffer differs from PC by over three channel levels at 16 of
307200 pixels, with mean maximum-channel error 0.03264974. The new visible
pose was captured at `framebuffer.png`. Full startup selection, class-cache
creation ordering, input/AI and support-loss traversal remain open.


## Movement animation follows steering and movement mode

The live actor no longer receives the fixed stand/walk schedule at frames
0/16/48. When stance selection is unhandled, an animation callback passes the
same process-local steering used by physics, the active physics descriptor and
numeric movement mode into `rf_motion_select_movement`. Ordinary unarmed
candidates 0/2/4 come from 41f6ee..41f729. Priority/AI candidate selection is not
implemented by this binding. The original movement tail decides when to request
an animation and avoids repeating an already current/next state. Standalone
animation fixtures retain their schedules. Crouch eligibility is still scripted
and takes precedence, as the recovered handled decision requires.

A shared command helper prevents animation and physics from sampling different
steering schedules. Guest `rf_scene_actor_locomotion_frames` holds 64 records
of execution, descriptor/numeric mode, input, candidates and resulting controller
state/duration (3,072 bytes). Scene checks compare each executed record's input
to its physics input, verify stance-handled frames skip the movement selector,
and require idle/run choices appropriate to the supplied mode. Frame 16 now
remains stand with no transition. The short pulse requests run state 4 at 24,
then requests stand state 0 at 48 after successful standing clearance.

Run `artifacts/xemu/20260909-172348-379119/report.json` passes stock 64 MiB XEMU.
All 64 locomotion records and the existing geometry/body/stance/surface checks
match PC. Both builds, four CTests, all three PC scene profiles and 6,000 original
movement-tail comparisons pass. No new framebuffer was captured. This connects
animation selection to actual fixture movement inputs; it does not add player
input, AI, animation speed scaling, priority selection or full entity lifecycle.


## Shared animation and physics runtime step

The integrated actor had been advancing animation/controller time by 1/30 second
per rendered update while physics advanced by 1/60. Original actor update
41dd27..41dd41 pushes the shared delta at 5a4014 directly into 503360; this call
path supplies no movement-speed multiplier. Configured live actors now pass the
same scene step to controller advancement, playback and physics. Standalone
animation fixtures retain their existing step. Frame zero retains a separate
1/30 initialization update to preserve the current diagnostic creation pose;
full original startup scheduling and initial selection remain unresolved.

Guest `rf_scene_actor_animation_timing` records 64 triples (768 bytes): delta,
resulting phase and generation. The memory harness compares every word with PC.
The stance eligibility fixture now spans frames 32 through 55 to exercise a
completed crouch at the slower step. Passive idle transitions crouch at 47;
moving profiles crouch at 41 because an interrupted blend retains progress.
Standing resumes at 56. These replace the previous fixture's timing observations.
Speed checks derive sample pairs from observed stance transitions: sustained
movement measures 5.23269653 before crouch, 2.69393921 near the end of crouch,
and 4.50942993 during recovery, in world units per second.

Run `artifacts/xemu/20260909-172853-167404/report.json` passes stock 64 MiB XEMU,
including all timing, selector, body, surface and geometry comparisons. Both
PC/NXDK builds, four CTests and all three PC scene profiles pass. No framebuffer
was captured. This verifies the reconstructed builds agree; original instruction
evidence supports using a shared delta, but does not establish that the complete
original frame scheduler or live gameplay input has been reconstructed.


## Original support-query gate audit

`python tools/verify_actor_support_gate.py` executes 487f82 through the query/skip
branch with unchanged original 42a020, 429990, 486c90 and 4895d0 callees. All 256
prepared combinations pass. It stops before 4a0840, so this does not establish
world-query correctness or live support-loss traversal.

The exact predicate is: modes 3/8 always query; category 1 with ground material
-1 also queries; otherwise mode 1 requires attachment handle -1 and either
movement, physics flag 0x400000, or actor flag 8. In particular, 4895d0 simply
reads actor flag 8. The fixture's position/stance comparison omits these general
entity conditions, which must be connected when the entity runtime is composed.

The preceding 487f20..487f67 block marks movement on actor flag 0x2000000, or
flag 0x4000000 with positive distance between +6c and +e4, and then changes the
dirty flags. The threshold at 5893e0 is zero. This was inspected, not executed
by the new verifier. Original crouch routine 4289d0 directly calls 4a0840 after
replacing sphere centers; the fixture currently records/commits its support
query later in the rendered frame. That ordering remains an integration gap,
not proof that stance change is an original input to the movement predicate.

Next integration work must preserve these query triggers and ordering while
adding a route that actually loses and regains support. The existing 64-frame
routes still have zero support losses. No new screenshot or XEMU run was needed
for this source audit; executable game code was unchanged.


## Immediate stance support integration

Accepted live crouch/stand transitions now call the shared ground query and
static landing/support commit immediately after replacing sphere centers,
matching the direct 4a0840 calls in original 4289d0 and 428a60. The call precedes
the fixture's speed-mode update and animation advancement. The rendering view
is refreshed from the resulting body pose before generating that frame's mesh.
Stance changes no longer serve as a substitute movement flag in the later
ordinary support gate. Its general actor flag/category/attachment inputs remain
unimplemented. Moving supports and complete original query side effects remain
outside this static-world binding.

Two new guest arrays retain the 64 potential stance queries: 33 words per raw
probe/hit record and nine words per decision/pose record (query, mode before and
after, position before and after), 10,752 bytes total. PC checks require exactly
the two accepted changes to query, and their resulting poses to be the rendered
poses. The copied ceiling-clearance test passes -1 to suppress live support
side effects and still tests clearance/center changes only.

Run `artifacts/xemu/20260909-173908-972553/report.json` passes all new records
and existing actor/geometry comparisons on stock 64 MiB XEMU. In the drive
profile, frame 41 moves Y from -3.91847563 to -3.92274046 before rendering;
frame 56 moves Y from -3.94883037 to -3.94900179. Passive and sustained-X profiles
also pass their PC checks. Both builds and four CTests pass. The original
landing/support verifier passes 42 prepared contacts on PC/NXDK, plus its two
original no-hit/steep support-loss cases. No new framebuffer was captured.
This closes the delayed stance-query ordering gap recorded in the preceding
audit, but does not demonstrate walking off a ledge or full gameplay.


## Longer physics routes beyond the rendered fixture

The optional `actor-routes.flag` mode continues the owned miner body's physics
for eight independent 600-update routes after the 64-frame drive scene. Cardinal
and diagonal constant steering commands use the existing movement transform,
run/fall proposal, sphere/world sweeps, contact response and pose setter. Ground
material/traction comes from each support face. Every route starts from the final
rendered body, evolves its own state without replaying a trajectory, and uses
the existing simplified movement gate. The render fixture's state and counters
are restored afterwards. Animation is not advanced during these extra routes;
this is physics integration evidence, not a 600-frame animated gameplay scene.

Long routes use stack-local contact records instead of the 64-contact render
trace, retaining total contacts and capped-update counts. No route allocates
memory per update. Guest `rf_scene_actor_routes` holds eight 16-word records:
status, completed updates, landings, support losses, contacts, capped updates,
final mode, rolling full-body-state hash, final position XYZ, final velocity XYZ,
first loss update (UINT32_MAX if absent), and last landing update. The hash
includes every resulting 308-byte body state. Matching hashes are compact
comparison evidence, not a retained byte-for-byte trace of every update.

Use `--traverse` in `rf_scene_check.exe` with the usual archive arguments. For
XEMU, create `build/xbox/disc/actor-routes.flag`, rebuild the disc with
`tools/build-xbox.sh`, and run `python tools/xemu_smoke.py --actor-routes
--no-capture --seconds 180`. Remove that optional flag and rebuild before running
normal `--actor-drive` checks; the harness checks the guest mode explicitly.
The scene checks require all 4,800 updates to complete without a capped update,
and positive X to lose support, recover every loss and finish grounded.

The first comparison run `20260909-174316-688495` passed stock 64 MiB XEMU with
three losses/three landings along positive X; the first loss was update 185 and
last recovery 309. All eight route summaries and rolling state hashes matched
PC. Both builds, four CTests and the passive, drive, sustained and extended PC
profiles passed. The final revision also routes position/bounds synchronization
through the same verified pose setter as the rendered actor; its validation run
is recorded below. No screenshot was taken because the extra routes are not
rendered. General entity query triggers, moving platforms and animated traversal
remain open.

Final pose-setter run `20260909-174417-380780` also passes all eight route
comparisons in stock 64 MiB XEMU, retaining three positive-X losses/recoveries.


## Persistent animation stream length

`rf_animation_placement.frame_count` now controls placed stream duration; zero
preserves the 64-frame default. The producer retains one controller, playback
state, bone/cache workspace and mesh allocation for the entire call. The fixed
authored preview request sequence repeats every 64 frames without reconstructing
the controller. Live movement callbacks continue to receive absolute frame
numbers and can supply their own decisions. Single-frame preview entry points
retain their existing 64-frame limits.

Optional timing storage now declares `animation_timing_capacity`; zero preserves
the legacy 64-record capacity. A stream that would exceed a supplied timing
buffer returns RF_RANGE before invoking the sink. No additional allocation is
required merely to increase frame_count; callers choose their telemetry storage.

The PC scene checker accepts `--long-animation` with the usual archive arguments.
It renders 600 authored miner frames at a 1/60 runtime step, verifies fixed mesh
storage, compares the first 64 frames with an independent short stream, checks
that timing did not restart at frame 64, and rejects an undersized timing buffer
without additional callbacks. The final rolling mesh hash is 38db0b07. This is
an animation-only stream: it does not attach the moving body or extend scene
telemetry beyond 64. Those callbacks remain the next integration step.

Both PC/NXDK builds and four CTests pass. The XEMU regression below exercises
the existing 64 rendered frames plus eight extended physics routes; it does not
claim that 600 animated frames have yet run in XEMU. No screenshot was captured.
`artifacts/xemu/20260909-174748-838687/report.json` passes that stock 64 MiB regression.


## Continuous animated moving-body scene (PC validated)

The `--live` PC scene profile now keeps the same animation controller and owned
physics body through 664 rendered callbacks. It uses the existing 64-frame
steering/stance setup, then positive-X steering from frame 63 through 662.
The producer advances animation throughout movement, falling and recovery;
no body or controller reset occurs at the old endpoint. All per-frame scene
telemetry is retained in 64-slot rings, indexed by absolute frame modulo 64.
`rf_scene_actor_ring_frames` identifies each slot's absolute frame. Stance query
slots are cleared before reuse, preventing old queries from appearing current.
The optional animation timing ring has explicit capacity and wrapping semantics;
non-wrapping timing buffers retain their overflow guard.

The live summary stores magic, frame count, rolling geometry/body hashes, final
mode, landing/loss counts and status. The full resulting body state contributes
to the body hash once per rendered frame. Long contact handling retains the
first 64 contact records while counting all contacts; it does not abort physics
when the trace is full. The normal short diagnostic retains its overflow error.

PC validation completes 664 frames and 663 updates, with 433 contacts, three
support losses, three recoveries after the initial landing, and no capped update.
The geometry hash is 3874259347 and body hash 533762320. Checks cover fixed mesh
and material addresses, immutable world vertices, sequential callbacks, final
ring indices/steering consistency and the final rendered body position. All six
PC profiles, four CTests and both PC/NXDK builds pass.

Xbox selects this mode with `actor-live.flag`; its final actor diagnostics now
use frame 663 and skip the obsolete frame-63 final check. This new mode has not
yet been validated by the XEMU harness. Its expected frame-count/ring comparisons
must be added before claiming Xbox runtime success. The existing close camera
also loses the actor before the route ends (zero actor triangles in the final
frame), so a suitable full-route camera remains necessary. No screenshot was
captured. General entity lifecycle/input/AI and full-game resource behavior
remain outside this diagnostic.


## Continuous-scene XEMU memory harness

`python tools/xemu_smoke.py --actor-live --no-capture --seconds 180` now selects
the PC `--live` reference and expects `actor-live.flag` in the built disc. It
checks the 664-frame renderer endpoint and actual final draw count instead of
the short scene's frame-63 assumptions. The mode selects its own fixture;
combining it with the other actor profile switches is rejected.

The harness compares the eight-word live summary, full tick counters and all
308 final body bytes with PC. It also compares ten complete 64-slot rings:
absolute frame IDs, rendered geometry/position, animation timing, steering,
locomotion selection, stance selection, raw support queries, support modes,
surface/traction and stance/collider hashes. Rolling geometry/body hashes cover
all 664 callbacks; the rings retain frames 600..663 in modulo order. Existing
archive, geometry, GPU allocation, stock-memory and native renderer checks also
remain active. The generic short actor verifier is not applied to overwritten
long-run rings.

The initial run `20260909-175335-622687` completed all frames and passed the live
memory comparisons, then failed an obsolete final-draw-count assertion. That
assertion now uses the live PC endpoint; the complete rerun is recorded below.
No game code was changed in response to that harness failure. The close camera
still loses sight of the actor, so no new screenshot was taken.

Complete run `20260909-175428-274474` PASS: stock 64 MiB, 664 rendered frames,
663 physics updates, four landings including initial landing, three support
losses, ten matching telemetry rings and 308 matching final body bytes.


## Live-route endpoint inspection camera

The live profile now uses `rf_scene_preview_route_camera` consistently in the
PC checker, PC raster preview and Xbox scene. This is a fixed diagnostic camera
near the known positive-X route endpoint: relative to the authored miner spawn,
position offset (+16.6, -1.0, +2.4), facing negative Z. It is intentionally tied
to this fixture, not recovered player-camera behavior or a dynamic chase camera.
An attempted view along positive X was occluded by the tunnel bend; a moving
camera remains necessary to keep the whole route visible.

`rf_pc_preview.exe --scene-live-last` takes the same arguments as
`--scene-drive-last` and rasterizes frame 663. The endpoint contains 2,876 world
triangles and 479 actor triangles, showing the later walking pose. The memory
harness now reads the expected world draw count from the matching live PC
checker instead of assuming the original close camera's count. Geometry and
physics ring comparisons remain active; changing the view does not change the
body trajectory or the three support losses/recoveries.

Both builds, four CTests and the short drive regression pass. The live XEMU run
below compares all 664 frames' rolling hashes, final rings/body, and the native
framebuffer against `artifacts/route-camera.ppm`. A native image is captured for
this new visible pose/location; full route visibility and player control remain
open.


The first endpoint run `20260909-175827-110115` passed actor memory checks but
found a world-count mismatch: the checker used only static geometry (2,796
triangles), whereas Xbox and the PC raster preview include retained movable
geometry (2,876). The live checker now uses the same retained-world construction
for both its immutable world reference and scene input. This exposed 80 visible
movable-world triangles at the new view; it was not an actor physics mismatch.
The complete rerun and image comparison are recorded below.

Run `20260909-180017-502708` PASS on stock 64 MiB XEMU, including all live
actor comparisons and native framebuffer comparison: 10 of 307200 pixels exceed
three channel levels; mean maximum-channel error 0.06783203125. The new native
image is `artifacts/xemu/20260909-180017-502708/framebuffer.png`. It shows frame
663 at the endpoint; it does not show uninterrupted traversal from the spawn.


## Retained world reprojection with an explicit camera

`rf_scene_world_update_camera` accepts a camera position and orientation alongside
the retained world and optional mover poses. It makes a shallow temporary view
of the geometry owner, substitutes camera values, and uses the existing world
updater. It does not mutate the saved inspection camera, allocate a new mesh,
load archives or duplicate geometry payloads. Invalid/nonfinite camera input
returns RF_RANGE before touching the output. Other errors retain the existing
preview updater's capacity/stable-input contract.

`rf_scene_check.exe --moving-camera Installed_Game/levels1.vpp L1S1.rfl` checks
32 translated/yaw-rotated views with changing mover poses after closing the
archive and overwriting the original level object. Every projected vertex
matches a fresh world build using the same explicit view, and the mesh address
stays fixed. Its final rolling hash is 2651622685. The prior fixed-camera
`--retained-world` check still returns hash 2134686494. A rejected NaN camera
leaves both mesh metadata and payload unchanged. Both PC/NXDK builds and four
CTests pass.

This prepares world reprojection for following the actor, but the live scene
still uses its fixed endpoint view. It does not implement camera placement,
collision avoidance, smoothing, a player camera or a moving-camera XEMU run.
The next connection must give both actor projection and world reprojection the
same per-frame view, then validate draw capacity and native output in 64 MiB.
No new screenshot was captured because the live view has not changed.


## Shared follow-camera scene (PC validated)

The animation placement now accepts a `prepare_view` callback. It runs after
stance effects and before pose rendering, supplying one world-view projection
for the current frame. The scene's optional `rf_scene_actor_follow` binding
borrows a retained world owner and uses the current body position (authored
spawn before creation) plus fixed offset (0, .7, 2.4), facing negative Z. The
callback reprojects the world with this camera and uses the same camera for
the actor. It retains authored mover poses; simultaneous moving-door poses
are not supplied by this binding. There is no camera collision or smoothing.

Follow mode reserves the caller's 8 MiB scene mesh budget once, leaving 1 MiB
for actor vertices and up to 7 MiB for changing world projections. It does not
allocate or reopen archives per frame. A mismatched source world/material
count, missing body/world context or nonsynchronous preview use is rejected.
The borrowed owner must outlive the stream; passing NULL disables following.

The PC checker accepts `--follow` with the regular archive arguments. It checks
all 664 callback indices, fixed mesh/material addresses, the camera's exact
relation to each body pose, world/actor material boundaries and nonempty actor
projection at every frame. The minimum projected actor count is 430 triangles,
versus zero in the fixed endpoint view. This establishes frustum inclusion,
not visibility through intervening world surfaces. The body hash remains
533762320, with three support losses/recoveries; projected actor hash changes
to 387910986. Short drive and fixed-camera live profiles also pass. Both builds
and four CTests pass.

This callback path is not yet selected by the Xbox frontend or PC raster
preview. Xbox GPU capacity and dynamic world-count expectations must be wired
before the 8 MiB CPU mesh can be exercised there. Native visual validation is
still required; no screenshot was captured in this step. Player first-person
camera behavior, collision avoidance and input remain separate open work.


## Xbox follow-camera binding and bounded GPU stream

`actor-follow.flag` selects the continuous live actor and binds the retained
world owner to the shared follow-view callback. The Xbox renderer now has an
explicit-capacity scene streaming entry point. Follow mode reserves one 8 MiB
GPU vertex allocation, matching the bounded CPU scene allocation, and reuses it
for every frame; attempts to change that capacity after stream creation are
rejected. Existing fixed-camera entry points keep their previous capacities.
This diagnostic reservation is not a final full-game memory budget.

The PC raster preview accepts `--scene-follow-last`. Its sink retains the final
world/actor boundary as the camera changes draw counts, and its retained source
owner outlives the full stream. Both platforms use authored movable-world poses
and the same body-relative camera; camera collision and simultaneous door pose
updates remain open.

`rf_scene_actor_follow_summary` records completed frames, a rolling hash of all
reprojected world vertices, peak world mesh bytes and a rolling camera-record
hash. The 64-slot camera ring contains absolute frame, world vertex count,
position and orientation. `xemu_smoke.py --actor-follow --reference
artifacts/follow-camera.ppm --seconds 300` compares those with PC in addition
to the live actor rings, final body, renderer memory and native framebuffer.
The follow flag must be included when rebuilding the disc; remove it before
selecting another actor profile. The new native capture is a final frame, not
proof of unobstructed visibility throughout the route.

Both builds and four CTests pass. The XEMU outcome, memory observations and
image comparison are recorded below. The PC endpoint image is visually close
to the previously posted endpoint inspection; avoid reposting essentially the
same still as evidence of the entire moving-camera sequence.


Run `20260909-181012-800513` PASS in stock 64 MiB XEMU: 664 follow-camera
frames, world hash 136623072, camera hash 3459837023 and peak world mesh 484848
bytes. The full actor checks and final camera ring match PC. The native image
comparison has 14 of 307200 pixels over three channel levels, mean maximum
channel error 0.06679036458333333. The saved native image is `framebuffer.png`;
it is visually close to the previously posted endpoint still and is not reposted.

The renderer reports 28471296 bytes available after upload and 37928960 after
CPU mesh release, with 4419588 GPU image bytes and an 8388608-byte GPU vertex
reservation. These are diagnostic observations, not full-game peak or real
hardware performance evidence. The world peak is below half a MiB, so both
8 MiB scene reservations are substantially oversized for this route and should
be reduced with another bounded-capacity validation. Camera obstruction,
first-person view policy, simultaneous mover updates and gameplay input remain
open.


## Follow-buffer reduction from measured usage

`RF_SCENE_FOLLOW_CAPACITY` now defines a 2 MiB fixed follow-scene allocation on
both CPU and GPU, replacing the 8 MiB reservations. The scene keeps its existing
1 MiB actor reserve, leaving 1 MiB for world reprojection; the measured peak on
this complete route was 484848 bytes. Bounds checks remain active and oversized
projections fail rather than growing beyond the budget. This fixture-specific
bound is not a claim that every campaign view fits the same geometry budget.

The follow summary now includes the actual CPU capacity as a fifth word. The
XEMU harness compares this to PC and requires both reported CPU capacity and
GPU allocation to be 2097152 bytes. The expected simultaneous reservation
reduction is 12 MiB (6 MiB per buffer); the full route still contributes all
world/camera/body states to its comparison hashes. Neither geometry precision,
texture quality, frame count nor physics timestep changes.

Both builds, four CTests and the short drive regression pass. The full 664-frame
XEMU comparison and measured memory change are recorded below. No screenshot is
requested for this allocation-only change.


Run `20260909-181406-561630` PASS in stock 64 MiB XEMU. World hash 136623072,
camera hash 3459837023, actor geometry hash 387910986 and body hash 533762320
match the prior 8 MiB run exactly; all support transitions and the 484848-byte
world peak are unchanged. CPU/GPU capacities both report 2097152 bytes. Free
memory after upload is 41062400 bytes (previously 28471296); after CPU mesh
release it is 44220416 (previously 37928960). Exact reservations fall by 12 MiB
while both meshes live, and the final retained GPU allocation falls by 6 MiB.
These remain diagnostic memory observations, not full-campaign peak evidence.
No framebuffer was captured because geometry and view behavior are unchanged.

## Moving-actor room membership

The diagnostic miner now maintains `rf_scene_actor_room_state` through the
reconstructed 48a190 refresh, with the retained-level locator as its callback.
Refresh occurs at each rendered-frame boundary, after the previous body update
and current stance changes. A successful result carries a level room token
(index + 1; zero means absent). The object pose's dirty flag is cleared through
that refresh. This composes recovered routines into the diagnostic scheduler;
it does not establish the complete original entity update order. The miner is
nonlocal, so local-player room/audio notification is deliberately not invoked.

A fixed 64 x 9 word ring records frame, room token, flags, last query position,
selected source face, whether lookup ran, and retry count. An eight-word summary
records all-frame count, queries, misses, membership changes including initial
assignment, full rolling hash, initial/final tokens and total retries. The new
telemetry costs 2,336 bytes plus the 20-byte room state; it allocates nothing per
frame. XEMU snapshots include both records and state, and smoke validation
compares the ring and full summary against PC.

The PC 664-frame follow route performs 664 lookups without misses and changes
from token 54 (room 53) to token 53 (room 52). The full room hash is 3635044975.
World, camera, actor and body hashes remain unchanged. The ordinary 64-frame
PC drive profile and four CTests also pass. Native XEMU confirmation is recorded
below when complete; no new screenshot is warranted by this internal change.

XEMU confirmation: `20260909-185445-343865` PASS on stock 64 MiB. All 576
room-ring words and eight summary words match PC. The final five-word room
state matches the final rendered record. All existing live actor/body/camera
checks pass; 664 queries, zero misses, two assignments including initialization,
and room hash 3635044975. No framebuffer captured. Both builds, the 64-frame
PC drive regression and four CTests pass.

Late-level startup replay (2026-09-10)
------------------------------------

`tools/xemu_replay_check.py` now accepts `--level L18S1.rfl --archive levels3.vpp`
alongside its input file. Selection implies authored campaign spawn. It stages
the installed archive into ignored disc assets, verifies its SHA-256, and writes
`campaign-level.bin`: two 64-byte NUL-padded fields for archive and member name.
Xbox validates the fixed size, terminated fields and archive whitelist before
opening it. Selection cannot be combined with climb staging. The harness restores
the prior selection/replay flags and rebuilds the normal ISO in `finally`.

The initial L18S1 guest failed with RF_IO because the disc lacked levels3.vpp.
After archive staging it failed with RF_NOT_FOUND in diagnostic preflight:
this authored level omits both mover and group sections. Preflight now creates
empty owners for absence, matching the shared scene's optional mover handling;
other parser errors continue to propagate.

All four 16-tick idle replays pass with native memory matching the PC reference
for startup counters/gravity, resolved links, player/body, input and animation:

| Level | Native report under artifacts/xemu | Gravity | Available pages at completion |
| --- | --- | --- | --- |
| L18S1 | replay-20260910-064208 | 9.8 | 11448 |
| L17S1 | replay-20260910-064231 | 4 | 9548 |
| L17S2 | replay-20260910-064257 | 3 | 9603 |
| L17S3 | replay-20260910-064322 | 4 | 9656 |

Each run explicitly verifies 64 MiB guest RAM. Page counts are final observations,
not peak campaign memory budgets. These checks establish startup integration only:
unsupported event actions, outgoing propagation and delayed ticking remain open.
No framebuffer capture or PS2 visual-parity claim accompanies this internal fix.
The default L1S1 campaign path also passes its 16-tick regression in
replay-20260910-064400. NXDK build and all five PC CTests pass.


Current-build native regression (2026-09-11)
------------------------------------------

Source commit06d8038 passes the930-frame L1S3 replay with audio enabled:
`artifacts/xemu/replay-20260911-053608/report.json`. The harness verifies
67108864 bytes of base RAM and zero plugged RAM, compares its checked guest
body/controller/camera and campaign state with the current PC executable,
and observes9530 available pages at completion (not a peak memory budget).
The guest DSP ring contains4060 nonzero samples in8192 bytes; the checked
native audio error counters are zero. This is DSP output evidence, not a
host listening test. All eight PC CTests pass.

Reproduce with:
`python tools/xemu_replay_check.py artifacts/xemu/replay-20260911-022235/inputs.bin --level L1S3.rfl --audio-capture --seconds 240`

Test XBE SHA256:9fc02b656f2297a1133a94645404818ad0f20e3a107208127e9098046998c7b1.
PC SHA256:04057475e117915be108b6c0a18ebe38d9117c999056b33eff79553335e58f26.
The harness restored the normal disc flags and rebuilt the normal ISO after
completion. No framebuffer was requested because this checkpoint adds no new
visual behavior. This verifies the existing live diagnostic path after recent
shared-code changes; it does not exercise the newly recovered burn ownership
or sample-control helpers through persistent NPCs or the native device adapter.
