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
No matching entry allows simulation. A match calls `497390`: a null room pointer at
emitter +0x4c freezes simulation, otherwise the room byte at +0x160 decides.
This is a room eligibility check, independent of emitter enable at +0x140.
Freezing still commits the previous-position copy, without aging or expiry.

The shared gate contains resolved entry presence, room presence and room visibility
value. Only the low visibility byte matters. Nonnegative owners require a supplied
gate; missing caller information is not treated as a failed original lookup.
The existing unowned API retains its rejection of nonnegative owners. Collision,
swirl, wind and damage remain unsupported for advancing particles.

`verify_particle_owner_step.py` executes full original `495120`, actual handle
and level-vector lookups, room accessor and simulation helpers against the
native PC probe and NXDK machine code under Unicorn. All 2048 cases match exact
particle fields, live counts and bounds; 768 freeze, 670 expire and 344 expand
bounds. Cases include missing and stale handles, missing level entries/rooms,
visibility values 0/1/255/256 and negative-owner bypass with an ineligible matched room.
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
Both original frame-loop passes use vector 0x646080 in authored order:
433374..4333bb before simulation, 433455..433493 after bounds finalization.
Each rechecks the emitter room via 497390 before invoking 4972f0 on the same
emitter pointer. The earlier suggestion of separate collections was incorrect.
Do not substitute emitter enable for room eligibility: phase changes happen
inside 4972f0, while the room check gates entry to that function.

## Source of room eligibility

Original `4d2f80` traverses the room vector at world +0x90 and clears only
room byte +0x160. `431820` calls this reset using the world at 0x6460e8 before
its player-view loop. The reset does not clear the adjacent +0x161 visit flag.

Room traversal `4d4860` sets +0x160 and +0x161 to 1 on accepted visits. It
rejects rooms with nonzero bytes at offsets 0 or 1, records traversal depth
at +0x164 and screen bounds at +0x16c..+0x178. Repeat visits union their
rectangles and retain one room entry in the ordered visible-room list.
Flag 1 skips recursive portal traversal. Caller `4d4760` selects an all-room
nonrecursive fallback or traversal from the supplied starting room.

`verify_room_visibility_lifecycle.py` executes the original reset with 0, 1,
17 and 128 rooms and eight original nonrecursive visits, without stubbing
their callees. It verifies byte-preserving reset, eligibility predicates,
visit flags, rectangle union and duplicate handling. This establishes that
particle room eligibility is render-derived. It does not establish complete
portal traversal or simulation-versus-render timing. The shared diagnostic
renderer currently lacks this room visibility lifecycle; campaign wiring
must recover it rather than substituting emitter enable or camera room alone.

## Shared visibility bookkeeping and timing

`visibility.h`/`visibility.c` provide caller-owned, allocation-free room state
and an ordered visit list. Begin-render clears eligibility; begin-view clears
visited flags, sets depth to 255 and empties the list without clearing render
eligibility. Accepted visits union rectangles and move revisited rooms to the
tail. Portal clipping and room predicates remain the caller's responsibility.

`verify_visibility.py` compares 512 reset/view/visit commands with actual original
functions, native PC and NXDK code under Unicorn. All room fields and list slots
match across eight rooms. Original begin-render uses world vector +0x90 while
begin-view uses +0x9c; this test supplies the same room set to both, and their
membership distinction remains to be reconstructed for scene integration.

The normal `433520` path calls simulation at `43363b`, then rendering at
`433645`; `431820` resets room eligibility before its player-view loop. Thus
that simulation path consumes visibility retained from the preceding render.
Its alternate branch at `433558` simulates and returns without rendering. This
is call-order evidence, not proof of every exceptional game-loop path or startup
visibility initialization. The shared bookkeeping has not yet been wired into
the campaign renderer, and does not on its own implement portal traversal.

## Traversal with resolved portal rectangles

`rf_visibility_traverse` now implements the control flow of `4d4860` using
caller-resolved portal rectangles/rejection flags. It follows authored adjacency
order, intersects the incoming rectangle with each portal rectangle, rejects
zero-area intersections, and uses signed room depth comparisons to avoid walking
back into ancestors. Child depth returns to 255 after each call, including a
blocked child. Detail rooms stop traversal unless they are the supplied special
room. Flag 1 disables traversal while retaining the starting-room visit.

The shared implementation uses 257 caller-owned frames (7196 bytes), avoiding
native recursion and heap allocation. `verify_visibility_traverse.py` compares
256 cyclic four-room/five-portal cases against full original recursion and
PC/NXDK code. All room fields, depth markers, rectangles and ordered list slots
match, including blocked/detail/special rooms and rejected/nonoverlapping portals.
Original portal caches are supplied as already resolved in these fixtures;
projection, cache generation, authored graph loading and native renderer wiring
remain unfinished. This advances the traversal layer, not visual gameplay.
