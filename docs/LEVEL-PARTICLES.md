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
The resolved nonnegative-owner eligibility path is now available through
`rf_particle_pool_step_resolved`; campaign callers must supply actual lookup
results before stepping these level particles.

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

## Owner eligibility

Original `495120` copies current position to previous position before checking
ownership. Negative handles bypass lookup. Other handles use `40a0e0` (including
generation validation), then take object UID at +0x20, or -1 for a missing object.
`45d630` searches the vector at 0x646080 for the first level entry with that UID.
No matching entry allows simulation. A match calls `497390`: a null runtime at
entry +0x4c freezes simulation, otherwise its enable byte at +0x160 decides.
Freezing still commits the previous-position copy, without aging or expiry.

The shared gate contains resolved entry presence, runtime presence and enable
value. Only the low enable byte matters. Nonnegative owners require a supplied
gate; missing caller information is not treated as a failed original lookup.
The existing unowned API retains its rejection of nonnegative owners. Collision,
swirl, wind and damage remain unsupported for advancing particles.

`verify_particle_owner_step.py` executes full original `495120`, actual handle
and level-vector lookups, enable accessor and simulation helpers against the
native PC probe and NXDK machine code under Unicorn. All 2048 cases match exact
particle fields, live counts and bounds; 768 freeze, 670 expire and 344 expand
bounds. Cases include missing and stale handles, missing level entries/runtime,
enable values 0/1/255/256 and negative-owner bypass with a disabled matched entry.
This is function-level replay, not XEMU gameplay or completed caller integration.

## Frame bounds finalization

`rf_emitter_pool_finish_bounds` reconstructs `497df0`, called after `496480`
simulation in the original `433260` frame loop. With a nonzero global enable
byte it walks active emitters; nonnegative-owner emitters receive
`sqrt(maximum_distance_squared) + max_radius` as their estimated radius and
clear the accumulated distance. Negative-owner emitters retain both fields.
This preserves the original double-precision square root/add before float
storage. `497de0`, called immediately before simulation, is a no-op in this build.

`verify_emitter_finish_bounds.py` passes 2048 original/PC/NXDK single-active-slot
cases (680 updates), including low-byte global gating, signed owners, zero and
negative-zero bounds. Full active-list and campaign integration remain open.
The frame trace also shows level emitter update loops both before and after
simulation; their separate collections must be resolved before claiming the
campaign schedule is equivalent. Simply ticking every emitter once at the end
of the current diagnostic frame is not established by this evidence.
