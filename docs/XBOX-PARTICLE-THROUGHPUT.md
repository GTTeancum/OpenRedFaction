# Xbox particle submission throughput

Source-written 2026-10-08 for the 15:00 parent batch. No build, test, new fixture
or runtime measurement was performed. No FPS improvement is claimed yet.

## Concrete source costs removed

`rf_xbox_particle_draw` previously called `upload()` for every textured fan.
Despite that name, image pixels are already physically contiguous GPU-ready
level resources. The call nevertheless walked width × height texels through
`rf_image_pixel` just to classify transparency. Only the ordinary material
sorting path uses `gpu_texture.transparent`; particle blend/depth behavior is
chosen explicitly from its mode. The new `classify_alpha` argument leaves
material/fallback handling unchanged and skips that unused scan for particles.
For a128×128 particle frame this removes16384 pixel lookups per emitted fan.

The same particle routine also waited on `pb_busy()` and called `pb_reset()`
after every fan. The private streaming world-particle/corona pass now queues
up to64 fans before draining and drains again at pass exit, including error
exits. Immediate vertex values are copied into push commands, so callers may
reuse their stack arrays immediately. Referenced images belong to retained
particle, impact, explosion, clutter-break, glare or bolt resources; the draw
passes do not retire or modify those images. The pass finishes before HUD,
simulation/resource mutation, level shutdown or return to the caller.

The current four-instruction vertex program, fragment/state setup and at most
12 vertices per fan keep64 fans below128KiB of the512KiB pushbuffer. Individual
begin/end command blocks are unchanged. Shader, blend, depth, texture and fan
order are still explicit per draw: mixed normal/glow/corona modes are not
reordered or merged. Public standalone particle draws remain synchronous;
existing solid HUD batching is unchanged.

No additional GPU image copy, per-particle allocation, image cache, persistent
resource owner or rendering approximation is introduced. Retained added state
is two counters plus16 bytes of telemetry. `rf_xbox_particle_batch` exposes the
last pass's fan count, GPU drains and skipped alpha-classification texels for
the parent's normal profiling read. The mesh-upload and CPU scene hash fixes
belong to parallel, separate patches.

Follow-on2026-10-08: consecutive identical mode/address/format setups now reuse
state only inside that same bounded pass, invalidated on every drain. Mode or
texture changes still install full state. See XBOX-RENDER-STATE-REUSE.md;
this follow-on remains source-written pending16:00.
