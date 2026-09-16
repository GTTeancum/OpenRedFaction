# Live authored source identity

The ctf06 source loader now captures immutable authored identity before copying
face material indices into renderer slots. It retains the32-byte digest with
the authored owner. The original level geometry and RGB lightmaps are borrowed;
editor bytes, decoded original textures and canonical pixel copies are temporary.
The capture helper owns and releases all of its allocations on success/failure.

Additional requested scratch is bounded at2MiB and measured at1,548,815bytes
for this fixture, including decoder page-rounding and an8KiB stack allowance.
This excludes already retained source inputs and allocator bookkeeping. The
loader includes capture scratch in its phase peak rather than treating it as
permanent gameplay ownership. Native available pages may also reflect allocator
arena retention and code growth; no claim of identical prior free-page count.

The independent installed-data probe and shared runtime capture both produce:
`4c74d0a864ac2f303620f5001c8f748a6a52b5e1bb7f65b510b2a7f40505c3b2`.
Three deliberately insufficient budgets preserve output sentinels; a subsequent
successful capture reproduces the original digest.

The PC840-frame reset/recut control completes. Native
`artifacts/xemu/render-20260916-122031/report.json` passes57 comparisons, including
all8 digest words, capture peak and readiness. Endpoint free memory is4190pages
(16.3671875MiB) on stock64MiB. The native framebuffer was inspected: the upper
post cut, surrounding scene, weapon and HUD remain intact. Staged disc contents
were restored and the owned emulator exited.

This authenticates no external file and restores no save yet. It supplies source
compatibility evidence for the upcoming RFDS2 semantic validator. Publication,
collision, historical maps and player state still need their own restore gates.
