# Level particle runtime loading

`rf_level_particles_open` joins the bounded A00 reader, template conversion,
room locator, persistent materials and fixed particle/emitter pools. Both PC
and Xbox builds compile this shared C module. It is a port-owned loader, not
a claim of equivalence to the complete original loader at `45fcf0`.

The state owns 1600 particle records, 128 emitter slots and 133 list headers.
Heap allocation keeps internal pool pointers stable when the output owner is
returned. Materials retain decoded pixels after the source archives close;
the collision world may also close after room handles have been resolved.
Room handles use index + 1; texture handles are local deduplicated slots.
Original level owner 0 is retained, with no supplied parent object.

The original conversion leaves template age-to-finish unassigned. This loader
explicitly initializes it to zero; original stack contents are not reproduced.
Creation follows verified `497ca0`/initializer behavior, including authored
initial emission, but the module does not yet tick or draw campaign particles.
The original nonnegative-owner eligibility path must be recovered before
using the existing unowned simulation for these level particles.

`python tools/verify_level_particles.py` checks all 87 emitters in 20 installed
levels against the reader, room and material reports. It checks bindings,
enable state, active-list links, initial particle counts, deterministic repeat,
exact-budget success and one-byte-under failure. The native PC probe also
checks internal heap pointers, image access after archives close, empty output
on failure and repeated cleanup. Generate prerequisite reports with
`inspect_level_emitters.py`, `verify_level_emitter_binding.py` and
`verify_level_particle_materials.py`.

Measured 32-bit residency is 238932 bytes for L1S1 and at most 484908 bytes
among these levels. This includes the owner, fixed state, material arrays and
decoded pixels; it excludes caller archive/world storage, stack and allocator
metadata. These are shared PC loader measurements, not native XEMU page
measurements or proof of whole-campaign memory fit.
