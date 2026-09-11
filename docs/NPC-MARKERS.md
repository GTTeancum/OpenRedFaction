# Named NPC animation marker consumption

Original `RF.exe` SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

`51c420` reads the dominant active-slot index at model instance +1d48, resolves
its motion ID through the active slots at +12d4 and descriptor array +f5c, then
searches two names at motion descriptor +40 and +54. Each name is 16 bytes with
a following tick value. Empty names are skipped and comparison is case-sensitive.
The first exact match returns false if its event byte is clear. If set, it clears
only that byte (+1d44 or +1d45) and returns true in AL. A duplicate second name
is not consulted after a first match, even if only the second event is pending.
No dominant slot or absent name returns false without clearing any event.
Upper EAX bits are not a boolean contract; the reconstruction returns a normalized
result via `fired`.

`rf_motion_consume_marker` in the shared motion core implements this behavior
using resource-indexed `rf_motion_marker_names`. The port validates bounded slot,
resource and string views. Failure preserves playback state and output. Metadata
must describe the resolved dominant resource, not merely active-slot order.
This function does not play a sound or choose whether an actor should poll it.
The caller's footstep dispatch/gates and catalog name retention remain open.

`python tools/verify_motion_marker_consume.py` executes the complete original
routine unchanged and compares PC and NXDK machine code. It covers 512 cases
with 47 true events: selected slots/resource IDs, exact-case and mismatched names,
empty/missing/duplicate names and all two-bit event combinations. Four invalid
port input cases preserve state/output. Result: PASS in
`artifacts/motion-marker-consume.json`. Both builds and all nine CTest checks pass.

## Entity footstep request routing

The only direct call to51c420 is501d3d inside the kind-two model wrapper501d30;
503420 forwards to that wrapper. Its two entity call sites,42f999 and42fa4e,
belong to42f940. `rf_entity_plan_footsteps` reconstructs that routine through
the48a930 sound-dispatch boundary:

- Entity+200 must equal -1; otherwise markers remain untouched.
- Flag8 at+7c, a nonzero+1430 player record and view mode zero (player+c4,
  then view+8) route to42fb20, bypassing model markers. This alternate route
  is reported, not executed by the planner.
- Poll left then right. Resolve the class group at+294/+178 indexed by entity
  +1380, falling back to index zero for a negative group. The table's declared
  footstep material groups support naming this selector `surface`; runtime
  assignment/ground-query integration remains to recover.
- If both selected/default groups are negative, return immediately. A consumed
  left marker can therefore leave the right marker pending.
-434d40 returns the sound-group count;434df0 returns its sample-list pointer.
  Left selects the first half, right the second half, using signed division by
  two. For valid nonnegative counts, odd trailing samples are outside both halves.
- Requests copy entity+3c and subtract+180 from Y. Their final scalar arguments
  are 1.0 followed by global62f980. Their deeper audio meaning is not assumed.

The port returns sample-array indices instead of process pointers, and validates
resource bounds. It does not choose the random sample or run the audio backend.
48a930 performs selection and 2D/3D dispatch; those paths, sound-group loading,
marker-name retention and the calling entity-update schedule remain open.

`python tools/verify_entity_footsteps.py` executes the complete original42f940,
including its real marker wrappers, group getters and vector-copy calls. Only
42fb20 and48a930 are intercepted to observe the two dispatch boundaries. PC and
NXDK machine code match640 cases, including85 alternate routes,213 sound
requests and4 left-failure/right-pending cases. Report:
`artifacts/entity-footsteps.json`. Both builds and all nine CTest checks pass.


## Foley group parsing evidence

`foley.tbl` owns the named groups referenced by entity footstep declarations.
Original `434880` opens that table; `434960` writes 44-byte group records at
`6300f8` (32-byte name, material, signed sample count, sample-array pointer).
The decompile shows a default count of one when `$Sounds:` is absent and a
loop over exactly the declared count. Missing material leaves the existing
record field untouched; a reconstructed owner must account for initialization.

`python tools/verify_foley_parser.py` executes original `511fc0` and `5125c0`
without hooks, including their whitespace/comment and CRT callees. Eight
primitive cases verify that forward search is case-sensitive and can skip
arbitrary intervening rows, while optional-token consumption is case-insensitive
and only accepts the current token after whitespace/comments. A supplied stop
token prevents searching past it; `434960` supplies no stop token for `$Name:`.
The search advances CR-based line accounting across skipped rows.

Two checks using the installed Foley table confirm that searches starting at
the fifth sample of Default Footstep and Solid Footstep skip both surplus rows
and land on the next group. Each declares four samples but lists six. Do not
silently add those two samples when reconstructing the group owner: this would
change the left/right halves consumed by `42f940`.

Evidence: `artifacts/foley-parser.json`, original executable SHA256 as recorded
above. This verifier covers parser primitives and real table tails, not the full
`434960` loader, allocation or registration. The candidate per-sample reader
`434620` reads a quoted filename, returns -1 for an empty name, otherwise reads
two floats and calls `5054b0` with an additional 1.0 scalar. Registration argument
semantics and runtime ownership still require direct verification before hookup.


## Shared Foley declaration reader

`rf_foley_table_read` now retains entity-group names, material IDs, declared
counts, contiguous declaration offsets and sample filename/near/volume/rolloff.
It uses fresh zero-initialized material fields, preserves empty sample names,
and permits odd and zero counts. Roll-off is 1.0 for Foley declarations, matching
the `434620` call site and the already verified `5054b0` registration signature.
Negative counts, overlong names and invalid numbers are rejected as bounded-port
validation. This is a declaration adapter, not yet a persistent group owner or
live registration/footstep hookup. It does not parse the separate lowercase
collision-sound section.

`tools/verify_foley_table.py` compares all 497 groups and 1,140 retained samples
against a separate table inventory on PC and NXDK machine code. Eleven groups
list surplus samples. Eight inputs cover the installed table, odd/zero counts,
empty filenames, overlong names, negative counts, nonfinite volume, missing end,
embedded NUL and an empty table (some combined in one input). Two additional
NXDK checks reject group/sample capacities one short while preserving outputs.
PC and NXDK builds and all nine CTest cases pass. This adds no PCM residency and
does not establish original registration order or full parser fidelity outside
the bounded adapter's accepted grammar.


## Foley resource lifetime

`rf_foley_open/close` retains group records and signed sample IDs in one owned
allocation. It parses into temporary declarations before any registration,
then calls the supplied registration function for nonempty names in table order.
Empty names store -1; negative backend IDs remain unchanged. Closing releases
only table storage, leaving the caller's audio resources under caller ownership.
This is a bounded port lifetime adapter around the recovered declaration order;
original global registration order and live campaign hookup remain open.

All installed groups require 26,452 resident bytes and 113,092 peak bytes,
including the 24-byte owner and temporary declarations, excluding source text,
allocator overhead and backend allocations. No PCM is allocated by this owner.
`tools/verify_foley_owner.py` checks PC and NXDK registration arguments/order,
empty filenames, negative IDs, exact/short budgets, source/temporary disposal,
empty tables and repeated close. NXDK execution injects both allocation failures
and checks no leaks or callbacks; malformed input also fails before allocation.
The harness supplies allocation/free and registration boundaries; shared owner
and parser machine code execute directly. This is not an XEMU/device audio test.
Both builds and all nine existing CTest cases pass.


## Class footstep material bindings

Original `41c781..41c7e8` initializes ten class slots at +178 to -1, then loops
on `$Footstep Sound:` (string59539c). `434cb0` resolves each name through a linear,
case-insensitive first match among 44-byte group records; an empty name or absent
match returns -1. `434d70` returns the group's material. The loader writes the
index to class+178+material*4, replacing any earlier binding for that material.
This is material-based selection, not the order of the class's sound lines.

`rf_foley_bind_materials` reconstructs that assignment with bounded owner/names
and ten output slots. Missing groups return RF_NOT_FOUND and preserve output
instead of entering the original fatal branch; malformed names/materials are
rejected. `tools/verify_foley_binding.py` executes the original assignment block,
lookup, material accessor and CRT comparison across160 cases; only parser/string
access and the missing-name fatal boundary are supplied. All129 successful slot
arrays match PC and NXDK;31 missing-name cases verify the port error result and
unchanged output. The full entity-table reader and persistent class integration
remain outstanding. Both builds and all nine existing CTest cases pass.


## Authored class footstep reader

`rf_entity_footstep_groups_read` scans the first case-insensitive class match in
`entity.tbl`, reads its `$Footstep Sound:` declarations, and applies the verified
material bindings. It allocates nothing, borrows no input after return and
preserves output on missing classes/groups or malformed declarations. Unrelated
class fields are skipped; this is a bounded field adapter, not the complete
original entity parser or lifecycle.

`tools/verify_foley_classes.py` independently inventories all63 installed classes
and compares all ten slots for each against PC and NXDK machine code. Five more
cases exercise a missing class, case-insensitive selection with commented-out
text and material replacement, an empty class, a missing group and an overlong
name. All68 cases pass. The source entity table SHA256 is
`cc512c9213fc87908cd792ed318f14e66f820ff16ff07c5a312c8293ff6827ea`.
Results are in `artifacts/foley-classes.json`. Both builds and all nine existing
CTest cases pass. Persistent campaign class storage, original registration
ordering, surface selection and footstep dispatch scheduling remain open.


## Startup registration ordering

`tools/verify_foley_startup_order.py` executes original startup segment
`4b22f4..4b2366`, including actual `4346f0` and its `506270` audio gate. Other
callee bodies are intercepted. Gate values0,1,255 confirm global sounds load
only for nonzero audio availability, followed by Foley unconditionally, finalizer,
exit cleanup registration (`434c80`), then `41b730` entity initialization.
`41b730` calls `41b830`; the latter opens `entity.tbl` and calls class loader
`41b910`. This establishes the order within this startup segment, not later
level loads, registry resets or every intervening loader's side effects.

`tools/inspect_foley_registration.py` combines the independent declaration
inventories in that order:88 global declarations plus1140 Foley declarations
contain1136 distinct case-insensitive names and92 duplicate Foley declarations.
Seven refer to global sound names. For example `death_hit1.wav` must retain
near20/volume0.8 from sounds.tbl rather than Foley's near8/volume0.9; `Alarm_01.wav`
retains near15/volume0.8 rather than near10/volume0.9. The inventory stores exact
binary32 parameters for integration comparison. It does not establish presence
in audio.vpp or successful registration; actual metadata-bank hookup still
needs that check, and PCM must remain separately resident on demand.


## Campaign Foley registration and class storage

Campaign startup now registers global sound declarations, then Foley metadata,
then ambient/controller/event sounds. Foley registration uses directory-backed
`rf_audio_bank_declare`; it neither loads PCM nor changes the bank's allocation
size. Startup checks both that allocation size stays unchanged during Foley
registration and that all88 global parameter records retain their hash. One
installed Foley declaration, `fp_shotgun_reload.wav`, is absent from audio.vpp;
its stored ID is -1. Other registration failures abort startup for cleanup.

The campaign retains Foley tables and one ten-slot binding array per seed class.
Temporary Foley/entity text and declarations are freed before playback. Closing
campaign state frees class bindings and Foley tables before the audio bank.
The `FOLEY` telemetry reports group/sample/missing counts, retained and peak
owner/table bytes, bank count after Foley, global parameter hash, sample-ID hash,
class count and class-binding hash. Peak excludes the preallocated audio bank,
archive directory storage and allocator overhead; it is not total game memory.

Stock64MiB XEMU `replay-20260911-101516` passes the180-frame door/APU replay.
Native and PC Foley words match
`[497,1140,1,26652,401292,1135,1274782420,3317977305,5,4209518190]`.
The independent archive inventory predicts1135 successful distinct names and
sample-ID hash3317977305. The bank ends with1147 entries and140808 metadata bytes;
its six resident waveforms total284918 bytes, unchanged from the prior replay.
Additional retained storage is127680 bytes of bank capacity plus26652 bytes of
Foley/class tables. The final PC framebuffer SHA256 remains
`3f008e45b98484177e9b17e467477908a7a27300df0e68507e0f565641933c82`.

The600-frame L1S2 PC lift replay also passes, retaining26572 Foley/class bytes
for three classes with class hash1050328222. This latest L1S2 check is PC-only;
it is not an updated native proof. Both builds and all nine CTest cases pass.
Marker-name residency, entity-update scheduling, live surface selection and
footstep sound dispatch remain unconnected. Global registry persistence across
level transitions remains separate work.


## Entity-update scheduling boundary

`tools/verify_footstep_schedule.py` executes original `41e4b0` through the return
from its footstep call at41e6e9, or an earlier return. Across256 combinations,
28 reach footsteps and16 exercise the player/network rejection flag write.
Actual40a110 tests object flag0x4000. Its true path returns early (the fixture
has no ambient handle). Additional early exits include missing model+80,
entity identity matching global5afb70, and the mode-byte64ecb9/64ecba path with
flag8 and a failed4a3740 lookup. In that mode, flag0x40 clears object+34; failed
lookup also sets object flag2. These are verified field effects, not new labels
for the object's lifecycle state.

For the accepted fixture path, ordered calls are421240,421170,429620,41f160,
421720,41f070,4194e0,4895d0, optional409280/409340, then42f940. Other callee bodies
are supplied, and auxiliary sound handles are inactive. This does not establish
the internals of those callees, ordering relative to model advancement, or the
writer of entity+1380 used for surface selection. Do not trigger footsteps from
the render loop or assume the entire entity update has been reconstructed.


## Ground-contact material source

The footstep surface selector at entity+1380 is populated by accepted support
contacts. In original4a0840, the499ed0 contact result begins at stack+48; its
material at contact+1c is stack+64. After numeric support position/bounds commit,
4a0bfa reads that word and4a0bff writes entity+1380, before the42a020 landing
predicate at4a0c05. This is the contact's material value, not its texture index.

`tools/verify_support_commit.py` now executes through4a0c05 for all2048 static and
resolved-moving support cases, varying the material across0..9 and poisoning the
prior entity field. Every original transfer matches. Existing PC/NXDK numeric
position/flag/support-handle checks still pass with their original scope through
4a0bfa; the new material assertion is original-code evidence, not an added C
contact/lifecycle owner. Query acceptance, lookup and landing effects remain
outside that fixture.

Additional decompile/disassembly candidates identify creation422360 setting the
field to zero,49fe40 clearing it on one zero-distance path, and4a03b0 clearing it
for negative entity+10c or updating it from a successful contact. These paths
need their own behavioral verification before defining persistent airborne or
miss behavior. The current NPC preview has no per-NPC support update, so it
must not assume the player's material applies to every actor.


## Resident marker names

Each `rf_entity_model_motions` now owns a contiguous `rf_motion_marker_names`
array indexed by the same local motion ID as its resources. It shares the model's
allocation; catalog creation copies both16-byte names from the temporary cache
before freeing it. Catalog memory/peak budget checks include the added storage.
This preserves actual registered names rather than synthesizing them from event
bits, and supplies the array required by `rf_motion_consume_marker`.

The extended base-action verifier checks names for all728 resources across
L1S1/L1S2/L1S3 (248/237/243 entries,13 marked entries per level). The probe closes
base motions before reading and consuming catalog-owned names. Exact/short
catalog budget and repeat-close checks continue to pass. L1S1 adds7956 bytes
(32 bytes per resource and one32-bit array pointer per model). The512-case
original/PC/NXDK marker-consumption oracle and all nine CTest cases pass; both
builds succeed. A fresh180-frame PC door replay retains the previous playback
words and framebuffer hash. This turn does not add a new XEMU runtime proof or
activate live footstep consumption; entity scheduling/support work remains.


## Main-model advance versus footstep phase

Static call sites in original487a40 place the object pass (`487b60 ->487cf0`)
before the later entity/support pass (`487c33 ->487e00`). Actual487cf0 dispatch
for object kind0 calls41daf0 and then4868c0. Within41daf0, prepared block
41dd16..41dd49 advances the main model when the retained BL condition is nonzero
or427020 returns exactly1. It forwards elapsed global5a4014, zero, and1 through
503360/501ab0 to51ba80 for a type2 model wrapper. This call is conditional; the
upstream calculation of BL includes state/distance logic that remains unported.

`tools/verify_npc_animation_order.py` verifies the static phase call sites,
executes the object dispatcher, and executes eight prepared advance-gate cases
with the real model wrappers. It intercepts51ba80 after checking model/arguments;
it does not execute the complete outer frame or animation body. The later
487e00 calls41e4b0 (whose footstep boundary is already tested), then decides
whether to run4a0840 support. Thus that later support query does not precede
this pass's footstep consumption. An earlier4a03b0 support path also exists in
487a40, so this alone must not be described as always using last-frame ground.

Keep model advancement, entity update and accepted-support writes as separate
ordered phases when connecting NPC lifecycle. Current preview playback still
advances every skeletal actor; conditional skip/state logic and the full
per-actor support lifecycle remain outstanding.

## Conditional main-model advancement

`rf_entity_animation_should_advance` reconstructs the decision in original
41dbea..41dd49. A present kind-2 model is required. A missing descriptor
disables the ordinary path. With a descriptor, entity flags +814 bit80000000
bypasses the state/distance restrictions; otherwise descriptor +160 byte zero
rejects states1/2/13, and signed class +13b8 values above2 reject distances
greater than45. Predicate427020 returning low byte exactly1 forces advancement
after the model-kind gate. The descriptor is returned by40a490 from entity[0];
it is distinct from the class pointer at entity+294.

`python tools/verify_npc_animation_gate.py` executes the original prepared block
with actual descriptor/model-kind getters across11520 cases (1956 advances),
including binary32 neighbors of45, signed detail values and low-byte semantics.
PC and NXDK machine-code decisions match. The harness supplies5182f0 distance
and427020 predicate results and observes503360 dispatch. It does not execute
alternate-view distance calculation, earlier actor-entry gates or animation
advancement. The helper requires a resolved finite distance and remains outside
the live campaign loop pending actor-field/distance ownership reconstruction.

## Camera metric and gate input ownership

Class loader41bad7 initializes +13b8 to zero. The loop41baff..41bb30
counts up to four floats following `$LOD Distances:` (string594f78), storing
them at +13bc. This field is the authored LOD-distance count, not a selected
detail index; the helper now calls it `lod_distance_count`.

The ordinary distance branch calls5182f0: render mode global17c7bcc other
than66 returns zero; mode66 calls5479b0, taking Euclidean distance to camera
1818680, multiplying by1818b50 and dividing by1818b48. The existing
`rf_model_lod_metric` already reconstructs that calculation. Predicate427020
returns entity+810 bit0 for non-null actors.

The animation gate now accepts a double metric. Passing the metric through a
float first loses decisions near45: position(45,.001,0), camera(0,0,0) and
unit scale produce a metric just above45, which rounds to45 in binary32.
`verify_npc_animation_camera.py` executes the complete original metric chain
and actual flag predicate within41dbea..41dd49, intercepting only503360.
All1200 finite fixtures match composed PC/NXDK metric and gate calls, including
60 boundary regressions. The11520 prepared supplied-metric cases still pass.
Double arithmetic remains an approximation to original x87 precision, not a
universal equivalence proof. Alternate-view distance, descriptor flag ownership,
actor entry scheduling and live gate integration remain open.

## Retained class LOD declarations

`rf_entity_lod_distances_read` reads the selected class's authored list, retaining
the first four values in order and consuming surplus values, as the count/store
loop41bad7..41bb30 does. Absent lists yield count0. Bounded parser errors preserve
output; duplicate/malformed lists are rejected as port validation. This is a
bounded adapter, not the complete original entity.tbl parser.

Seed classes now own the20-byte count/threshold record, loaded while their
existing entity.tbl scratch is available. Allocation/peak accounting uses the
expanded class size. No additional table allocation or archive pointer survives.
`verify_entity_lod_distances.py` passes all63 installed classes plus14 fixtures
on PC and NXDK machine code. The first three levels retain5/3/6 classes, adding
100/60/120 bytes; the PC probe verifies exact-budget success, one-byte-short
failure and retained fields after archive closure. Native XEMU has not yet
validated this expanded seed layout.

Room +160 is the shared room eligibility/visibility byte already traced in
LEVEL-PARTICLES.md:4d2f80 clears it before player-view processing and4d4860
sets it on accepted visits. Connecting the NPC gate still requires the correct
frame's room state, actor state/flags and camera values; current campaign
playback continues advancing all skeletal actors.

Both full builds and nine CTest checks pass. The180-frame PC door replay
retains playback179/78/1689 and Foley metadata checksums; final PPM SHA256
remains3f008e45b98484177e9b17e467477908a7a27300df0e68507e0f565641933c82.

## Native validation and remaining actor input

Stock64MiB XEMU replay-20260911-105003 passes180 frames after the retained
marker-name and class-LOD layout changes. Guest memory reports67108864 base
bytes and zero plugged memory. NPC playback remains179 ticks/78 actors/1689
bones, state hash1023684323, pose hash1674115745 and10536 clip bytes.
Foley/class metadata and APU checks pass; no new framebuffer was requested.
This validates the diagnostic's use of the expanded seed layout, not direct
guest inspection of every LOD value or live application of the animation gate.

The gate field is now explicitly named `action_520`: it is the entity action,
not `rf_motion_controller.current`. Existing creation evidence402d68..402dac
clears this action and behavior554; see CAMERA.md. Mapping the controller's
selected idle/locomotion state into this field would be incorrect. The field
rename does not change the wire layout or gate behavior.

## Live startup-actor gate

The campaign now calls `rf_entity_animation_should_advance` before advancing
each startup-selected skeletal pose. It supplies retained class LOD count,
cached stationary room membership, that room's retained visibility byte, and
the last rendered camera origin/scale. The metric uses scale[2]/scale[0],
corresponding to original1818b50/1818b48. `verify_visibility_camera.py` now
also checks that original547150 copies the supplied origin to1818680, the
LOD distance origin, across256 camera setups.

This is explicitly the existing stationary startup-actor path: action520 and
the relevant810/814 flag bits remain at their creation-cleared values. It does
not pretend controller.current is an entity action. Persistent AI/action, death,
script overrides and moving room membership still require live actor owners.
Controller application retains its existing scheduling; skipped advances leave
pose playback advancement/cache refresh untouched. Full original frame ordering
and marker consumption remain open. No extra per-actor allocation is added.

`rf_scene_npc_gate` reports cumulative considered/advanced/skipped decisions
and the last decision hash. PC's180-frame L1S1 door replay reports
13962/7308/6654, hash2606724109; playback hashes become962482953/1873909465.
The600-frame L1S2 lift reports22762/9255/13507, hash3778810013.
Nine CTest checks pass. The XEMU harness compares gate counters/hash with PC
and checks considered=(frames-1)*skeletalActors and advanced+skipped=considered.

Native stock64MiB XEMU replay-20260911-105533 passes180 frames with
exact PC gate counters and playback hashes above; Foley/APU checks also pass.
L1S2 remains PC-only for this change. No new framebuffer was requested.

## Sound-group selection and audio request routing

`rf_audio_group_choose` reconstructs48a930 through its audio-call boundary.
Signed counts<=1 select element0 without drawing random values; larger counts
consume one original CRT draw modulo count. It retains the selected signed
sample ID, including-1. The supplied capacity must cover max(count,1), so an
invalid group span fails before mutating the random state or output.

Actual48acf0 identifies object kind+24 zero with a non-null player+1430.
When player view+8 is zero,505560 receives sample, group0 and the two supplied
parameters in reversed order (pan/volume becomes volume/pan). Other cases
call5056a0 with position, volume1, shared vector173c378 and group0. The
compact request records scalar and position arguments; original5056a0 ignores
the vector argument, as already verified in controller-audio.md.
No sound is played by the chooser and no global RNG is introduced.

`verify_audio_group_choice.py` executes full48a930, actual48acf0/40d740 and
57312d, supplying only CRT thread storage and observing505560/5056a0 calls.
1024 original cases yield128 flat requests,896 spatial requests and651 random
draws. Together with one capacity-rejection case, all1025 match PC and NXDK
code, including sample IDs, parameter bit patterns and final RNG state.
Live marker timing, support surface ownership, shared frame RNG ordering and
backend playback remain open. Both builds and nine CTest checks pass.

## Surface clear/retain/probe gates

`rf_physics_surface_probe_gate` executes the decision from4a0406..4a046e.
Negative or unordered contact field+10c clears surface+1380 to-1. Values in
[0,.85) retain the existing surface and do not probe. At or above the exact
binary32 threshold0.8500000238418579, an existing surface other than-1 or
flags1a8 mask18000000 permits probing. Otherwise the old surface survives.
The API does not assign unverified velocity semantics to field+10c.

`rf_physics_surface_reset_gate` covers49feb6..49fedf only, after the earlier
flag4000 branch and its preceding updates. A zero or unordered field+1b0
clears the surface unless flag10000000 is set. It must not be applied to
all actors merely because the scalar is zero. Other49fe40 branches and later
physics updates remain separate.

`verify_surface_gates.py` executes both original prepared instruction spans
and compares action plus material mutation with PC/NXDK code. All1008 cases
pass, including signed zeros, .85 neighbors, signed material values, flag
combinations, infinities and quiet/signaling NaNs. There are136 probe requests
and90 reset requests; surface values actually change180/75 times respectively.
These gates do not execute contact queries or supply live NPC support owners.

## Accepted support contact owner

`rf_physics_support_contact` retains the support handle and signed material in
eight bytes. `rf_physics_support_accept` extends the numeric commit through
original4a0c05: after support position/bounds/flag/handle updates succeed, it
transfers the accepted contact material into that retained record. Static
support clears the handle but still copies material; resolved movers retain
their supplied handle. Negative material values are transferred unchanged.

`verify_support_commit.py` now compares the material on PC/NXDK as well as in
the original, across2048 static/rising/falling support cases. Seven additional
port validation cases verify that body and support both survive bad fractions,
non-finite inputs, negative radius and late bounds overflow unchanged. Each
failure fixture starts from a fresh valid input. The legacy numeric API remains
available for callers not yet owning contact metadata.

The owner has no archive pointers or allocation. It does not create per-NPC
physics bodies, run queries, initialize contact history, apply landing effects
or activate footsteps. Those remain the next integration requirements.
