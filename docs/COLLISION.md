# Collision reconstruction

`src/core/collision.c` starts the shared collision subsystem with the complete
segment/axis-aligned-box routine at RF.exe `0x508b70` and its outcode helper
`0x508dc0`. Evidence uses the installed binary fingerprint recorded by the
verifier. This primitive does not implement world collision or actor physics.

The routine checks the start, then the end, for inclusion before testing the
start's outside planes in X/Y/Z order. Bounds are inclusive. An endpoint inside
the box wins before any boundary intersection; callers must not interpret the
output as a nearest intersection. Shared outside bits reject immediately.
Otherwise failed plane attempts overwrite the output point. The C API retains
those writes while separately returning a hit flag. Invalid nonfinite input or
inverted bounds is a port error that preserves outputs. Inputs and outputs must
not overlap. The x86 implementations preserve extended intermediates, round at
the original float stores and restore the caller's x87 control word.

`python tools/verify_collision_box.py` runs 15,678 complete original-code cases:
5,711 hits and 9,967 misses, including 1,749 misses that write the point. It also
checks 322 invalid-input guards. Every hit value and output float byte matches
the PC implementation. All 16,000 fixtures additionally execute the actual
NXDK-linked function from `build/xbox/main.exe` in Unicorn, with unchanged
callees and the symbol address resolved from its map. The report fingerprints
that linked image. This checks compiled CPU behavior, not XEMU world behavior.
Fixtures include boundary endpoints, zero-length segments, zero-width boxes and
coordinate scales from 2^-15 through 2^15; they do not prove every float input.
Report: `artifacts/collision-box-verification.json`.

## Call graph and remaining work

- Crouch eligibility `0x429ae0` resolves target position via `0x48a8d0`, builds
  a ray origin from the entity position and transformed class offset, then calls
  `0x498e80` with mask `0x27`. The target helper uses a valid type-zero entity's
  stored eye at +0x7d4; other objects use position +0x3c.
- `0x498e80` iterates the moving-geometry list rooted at `0x64e96c` until sentinel
  `0x64e6e0`, testing each box with `0x508b70`, and invokes `0x4df1c0` in local
  coordinates. It also queries static world geometry. Its flag conversion is
  `0x499190`; transforms, traversal and hit records still require reconstruction.
- Stand-up `0x428a60` tests from entity position toward a point whose Y is
  position Y plus info +0xf74 plus constant `0x5893c4`, using `0x499ed0` and
  the entity's physics object +0x88. A collision prevents standing. Successful
  standing clears bit 0x400, updates an associated record, refreshes collider
  vectors, then calls `0x4a0840`.
- Enter-crouch `0x4289d0` sets bit 0x400, refreshes collider vectors, calls
  `0x4a0840`, then stamps entity +0x7b4 from global `0x6460f0`. The refresh uses
  40-byte class entries selected by `0x42da40`, copying their +0x18 vector.
- `0x4a0840` performs a ground/support query through `0x499ed0`, then may change
  vertical position, support-object state, bounds and related actor state. It
  cannot be replaced with only a collider-height change.

These call-graph observations come from Ghidra and original instructions;
only the segment/box routine above has been reconstructed and verified here.
The export script now retains the named collision functions for further work.
