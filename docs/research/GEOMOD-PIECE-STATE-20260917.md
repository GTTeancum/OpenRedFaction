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

Live scene checkpoint encoding/decoding is not connected yet. Neither moving
fragments nor native save continuation is claimed by these core checks.
