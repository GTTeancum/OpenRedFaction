# Original extracted-object birth and physics branch

`tools/probe_geomod_piece_birth.py` executes466440 and the4130b0 descriptor
construction through the call to generic allocator486da0. Real descriptor
constructors and vector/matrix helpers execute. String handling and the sound
lookup are supplied, and execution stops before allocation/physics/registration.
All25 piece-descriptor words and38 generic-descriptor words are checked against
independently constructed expectations across30 radius/position/sound cases.

Verified fields:

- Supplied placement, identity orientation, zero linear/angular velocity.
- Piece descriptor lifetime word at48 is-1, field4c is1, field50 is-1,
  flags54 are40000018, and fields58/5c are0.
- Radius strictly greater than3 requests `big debris bounce`. Its supplied
  nonzero lookup result occupies field60; smaller pieces leave it0.
- Generic creation kind is3, with supplied IDs-1/-1 and zero trailing arguments.
- Generic descriptor08 is the non-null extracted-solid pointer,0c is the
  float-rounded radius times stored float0.2,10 is material index1,14 is
  float-1,3c is placement,48 identity,6c/78 zero,84 radius,94 flags8000003f.

Source/disassembly follow-up identifies a critical integration distinction.
486da0 calls49ec90. With flags70 present, nonpositive initial mass and an empty
sphere list,49edfa tests descriptor08. Extracted objects take the non-null-solid
branch through4d1700, rather than the existing no-model sphere fallback.

The branch iterates a4x4x4 byte grid at solid30c, using each cell's low nibble.
Table589cfc maps the nibble to popcount(low4)/4. Grid spacing is solid34c and
origin350..358. Nonzero occupancy contributes a generated collision sphere;
mass/tensor and center adjustment come through4d1700. This grid-to-body stage,
including how the grid is populated and how spacing/center are updated, still
needs execution/reconstruction. These observations do not establish body motion
or contact response, and do not justify substituting one bounding sphere.

Report: artifacts/geomod-postedit-re/piece-birth.json; original binary SHA is
asserted in the probe. No gameplay code or native build changed in this step.
