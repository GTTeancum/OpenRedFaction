# Rocket contact with settled fragments

The rocket's ordinary sweep now arbitrates the nearest hit between composed
world/liquid geometry and all retained fragment polygons at current body poses.
Registry queries preserve stable batch/piece order on ties; a world hit wins an
exact tie. No allocation or fake bounding-box target is added.

A real test caught a coordinate-space boundary bug: rocket query4100 includes
bit4 (already-local) because the static world uses world-coordinate geometry.
Passing it to each fragment incorrectly bypassed translation/rotation. The
batch API promises world-space inputs and now clears bit4 before per-body
transformation. Existing52 transformed ray/sphere contacts additionally verify
bit4 produces exactly the same results as ordinary world input.

Fragment contact retains its real face and world hit/normal. Its room is located
from the contact, with the originating terrain room as an outside fallback.
A rocket-local tag80000000|batch<<16|piece identifies the owner; this tag is NOT
passed to the object registry as a handle. Particle effects, positional impact
sound and ordinary blast damage still run. Direct authored terrain cutting now
requires a world owner. This is the current port routing policy; secondary
blast cutting of nearby terrain and fragment subdivision/damage/wake are still
open. The hit chunk currently remains intact and settled after the explosion.

`tools/check_detached_rocket.py` uses an ordinary input replay: first rocket
separates the post, second rocket at frame420 aims at its settled fragment.
PC result:56 sweep calls, exactly one fragment hit (batch0/piece0/face1), two
launches/two impacts, two impact-audio starts and only one terrain edit.
The450-frame native PC capture was inspected for the second impact effect.
Audible quality is not verified. All121 tests pass and NXDK builds.

Evidence is under artifacts/geomod-postedit-re/detached-rocket. Player contact,
hitscan weapons, other-body registration, broad visibility/room scenarios and
additional fragmentation are not supplied by this rocket integration.

Stock64MiB XEMU full550-frame run render-20260917-064253 passes72 checks.
The fragment-contact counters, selected face and world hit-point hash match PC
exactly; native and PC checkpoints are byte-identical. Native final framebuffer
was inspected: the struck tilted chunk remains visible with the room and weapon
rendered normally. The process-local harness restored the disc and closed its
own emulator. This is contact/impact acceptance, not chunk damage or breakup.
