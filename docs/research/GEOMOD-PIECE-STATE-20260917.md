# Detached fragment body snapshots

The registry now provides size, encode and atomic decode for a pointer-free
RFPB version1 snapshot. This is a port save format, not the original save format.
It must be paired with the outer authenticated terrain history; it is not an
independent geometry or asset identity proof and contains no separate checksum.

All integers and binary32 values use explicit little-endian encoding. Header:
magic RFPB, version1, total bytes, record count (four words). Each320-byte record
contains extraction prefix, extraction ordinal, batch piece index, then308 bytes
of explicitly enumerated state fields. No struct padding or allocation pointers
are serialized. Records follow committed batch/piece order. Empty state is16
bytes; each retained body adds320 bytes. No allocation occurs in the codec.

Float fields are coefficients, mass, local/world tensor, current/next position,
current/next orientation, velocity, angular velocity, momentum, force, torque,
radius/minimum/maximum bounds, vector138 and scalar144. Integer fields follow:
flags, state124, reference15c, word164, word168. References15c and word168 must
match the rebuilt body; the codec does not resolve foreign runtime references.

Decode requires exact size/count/history keys. It rejects nonfinite values,
invalid mass/radius/material coefficients, invalid contact fraction, reversed
bounds, non-orthonormal/reflected bases and a world tensor inconsistent with
local inertia/current orientation. Immutable mass, local inertia, radius,
drag and friction must match the rebuilt owner. In-flight worklist membership
and unsupported rigid/random-normal routes reject. Decode validates all records
before writing any. Buffer aliasing, concurrent mutation and an open registry
edit are not supported. The caller must commit its privately reconstructed
registry before decode, then publish that private owner only after all outer
checkpoint gates pass.

Tests use actual extracted batches, change position/velocity/momentum/force/
torque/elasticity, rebuild another terrain and registry from history, restore
state and demand byte-identical re-encoding. Negative cases cover later-record
identity corruption and NaN, immutable mass mismatch, degenerate basis, bad
magic, truncation and decode during an edit. A changed first live body remains
unchanged after a later-record rejection.

## Scene integration

RFDS profile2 now uses header word12 as an optional RFPB trailer byte length.
Zero preserves the prior exact layout. Nonzero length must be16+320*n; the
trailer follows the face-map array and its own magic/version/length/count are
checked before offsets are exposed. The existing RFCP/RFSG envelope remains
unchanged, including transport integrity checks and size ceilings.

The scene writer includes snapshots when retained batches exist. Restore
reconstructs a private registry from terrain history, commits that private
registry, then validates/applies its snapshot before any scene publication.
Later failures discard the private bodies with the rest of the candidate.
Legacy saves without a trailer intentionally reconstruct the old birth state.

Scene tests alter body position/velocity/elasticity, prepare/commit restore and
require exact re-encoding. Corrupted body data rejects without publishing any
candidate terrain or changing the live serial. Full121-test PC suite passes;
the additional altered-body/corruption scene test also passes. NXDK builds.

Real middle-shot replay: saved/reloaded and uninterrupted2752-byte RFCP files
match SHA256209b38bcb45ebbd3133aeec3561f1c06081ada5971de277a150ba5e675eaff3c.
All5320 fixed-camera post-region pixels match; both native PC output images
were inspected. Room, weapon/HUD and post presentation are retained. A prior
body-less saved checkpoint also loads and produces that same new continuation.
Evidence: artifacts/geomod-postedit-re/piece-checkpoint-restart.

Motion is not scheduled in the live scene yet. This verifies stationary real
fragment continuity and modified-body scene fixtures, not moving-fragment
native continuation or new destruction visual quality. Xbox runtime acceptance
remains open.
