# Guest memory evidence

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
