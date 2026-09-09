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
