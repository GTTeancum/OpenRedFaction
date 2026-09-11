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
Negative or unordered orientation up-Y+10c clears surface+1380 to-1. Values in
[0,.85) retain the existing surface and do not probe. At or above the exact
binary32 threshold0.8500000238418579, an existing surface other than-1 or
flags1a8 mask18000000 permits probing. Otherwise the old surface survives.
Field+10c is orientation[4], the actor up-vector Y component.

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

## Shared class spheres and body creation

The existing diagnostic actor's class sphere builder is now shared as
`rf_entity_class_spheres_build`: it poses named CSPH records, applies class
X/Z centering and authored sphere overrides, and returns up to eight records.
Callers must provide the original class-initialization pose, not a later
per-actor animation pose. It neither creates nor owns a class cache.

`rf_entity_body_open` shares the existing positive-authored-mass creation
subset: material elasticity/friction, coefficient10, cleared tensor with the
empty-sphere identity fallback, creation flags, fresh body initialization and
copied class sphere installation. It preserves the destination on failure and
closes intermediate ownership. Generated mass and full factory/registry setup
remain outside this helper. The player/miner path now calls both shared APIs.

The PC body-owner probe checks exact versus one-byte-short budget, failed
sphere installation, independent retained sphere copies after source poison,
repeat close and rejection of unsupported generated mass. The180-frame PC
door replay retains body follow hashes2974216416/1833998883 and NPC playback
hashes962482953/1873909465. Xbox builds successfully; this refactor has not
yet received a new native XEMU run. Per-NPC body arrays/class caching remain
to be connected using these shared builders.

## Retained startup NPC bodies

The campaign now allocates one indexed body/support slot per startup actor.
For each skeletal class, it loads physics metadata once, derives class spheres
from that class's first authored startup pose and installs independent copied
sphere records for each instance. Body transforms come from each authored
entity and flags from its retained creation flags. Support handle8ac and
material1380 start at their constructor zero values. No support query or
physics simulation has run on these NPC bodies yet. Non-skeletal entries
remain empty; generated-mass classes still fail explicitly.

Allocation is capped at512KiB including temporary table text. Telemetry
`rf_scene_npc_bodies` records actor slots, initialized bodies, spheres, resident
bytes, peak bytes and a hash of body state/sphere values/support fields.
Residency includes body/support arrays plus owned sphere records, excluding
stack/global diagnostics/allocator overhead. Class config scratch is released
before instance construction; every partial failure and level teardown closes
all initialized bodies and frees the slot array. Per-instance transforms are
retained, but moving class/stance changes and future-spawn class caching remain.

PC L1S1 door180:78 slots/78 bodies/191 spheres,30480 resident/405120 peak,
hash3142020550. L1S2 lift600:39/38/114,15684/389676,hash613443426.
L1S3 startup1:28/25/48,10448/384152,hash262082589. The first two
replays retain their animation gate/playback results; the third checks only
startup, not traversal or collision fidelity. XEMU now compares body telemetry
with PC and checks initialized-body count against skeletal startup actors.

Native XEMU replay-20260911-111650 passes the180-frame door replay on
67108864 base bytes with zero plugged RAM: body counters/hash match PC
exactly, NPC animation and Foley/APU checks remain unchanged. Completion
reports8570 available pages; this single diagnostic is not a whole-game RAM
budget guarantee. L1S2/L1S3 body validation remains PC-only.

The body-layout mapping also resolves the surface-probe gate's input: generic
body base is entity+88, its orientation starts at body+74 (entity+fc), and
orientation[4] is entity+10c. Thus the0.85 threshold is actor uprightness,
not a contact normal or velocity. `verify_physics_body.py` already compares
the retained original fresh-body fields and copied orientations; the
parameter is now named up_y without changing its tested arithmetic.

## Post-entity-update support eligibility

`rf_entity_support_route` reconstructs487f6d..487fc9, after41e4b0.
Entity810 mask0x2 requests4281a0 unconditionally. Otherwise a nonzero low-byte
42a020 falling predicate requests4a0840. Non-falling actors require movement
mode1, linked handle-1 and at least one of: earlier moved byte, body moving
support flag400000, or nonzero4895d0. Other cases do neither.

`verify_entity_support_route.py` executes this original prepared span with
predicate returns supplied, observes query/fall calls and compares PC/NXDK
choices. All3456 cases pass:1212 neither,1380 queries,864 fall transitions;
only36 cases reach the last special predicate. This establishes why a blanket
ground query for all retained NPC bodies would be wrong.

The preceding487f20..487f67 computes movement from object flags02000000/
04000000 and squared distance between previous object position+6c and current
body position+e4, then updates those flags before41e4b0. That phase, predicate
ownership and accepted-contact filtering remain to be connected. A retained
body alone is not evidence that an actor is eligible for a support update.

## Movement flags before entity update

`rf_entity_support_moved` implements487f20..487f67. Object flag02000000
forces a moved result; otherwise flag04000000 requires positive squared
distance from previous object position+6c to current body position+e4.
Moved actors clear02000000 and set04000000 while preserving other bits.
Neither position is written here: history snapshot ownership is separate.
Float component stores match409fa0 before wider squared-distance arithmetic;
the comparison is strictly greater than zero, with no epsilon dead zone.

`verify_entity_support_moved.py` executes the original span and actual4faf00,
409fa0 and40a180 without hooks. All1600 cases match PC and NXDK moved-byte/
flag results and preserve both input positions;1151 report moved. Coverage
includes forcing/bypassing flags, equal and signed-zero positions, subnormal
movement, overflowing differences, infinities and NaNs. This supplies the
pre-update moved input to the already verified post-update support decision,
but does not itself schedule41e4b0 or update position history.

Existing predicate evidence is in XEMU-MEMORY.md and
`verify_actor_support_gate.py`: unchanged42a020/429990/486c90/4895d0
show modes3/8 or category1 with material-1 entering the broad support
predicate;4895d0 reads actor flag8. Do not reduce42a020 to mode3 alone.
Those facts should be reused when wiring retained actor inputs.


### Frame position history (487a40)

`rf_entity_position_snapshot` reproduces487b11..487b2f: copy published
object position+3c into previous position+6c and clear object flag01000000.
The source is NOT body position+e4. Original487a40 completes this list pass
before the next pass calls487cf0 (model/entity update), followed by physics,
487e00 (movement check, entity update/footsteps, support), and4881a0.
Normal physics publishes via487962 before487e00. The later4881a0 pass
resolves linked-object parents; it is not the normal publication step. Snapshot every object
before updating any object; do not snapshot immediately before support checks.

`tools/verify_entity_position_snapshot.py` executes the unchanged original
487aff..487b45 traversal with real409f40/409f70 vector copies, no hooks.
128 lists contain988 objects, including empty lists and arbitrary float bits.
All0x280 bytes of each original object are checked: only+6c and flag01000000
change. Body positions deliberately differ from published positions. PC and
NXDK helper results match exactly. Type8's separate469770 callback is excluded;
types0..7 are covered. This verifies the snapshot phase, not a full frame.
Both platform builds and all9 CTests pass; no live behavior or new visuals.

Creation audit (Ghidra486da0, exports486ede; instructions486f45..486f60)
shows the generic object factory adds06000000 to object flags, and adds8000
when incoming4000 is set. It initializes both+3c and+6c from supplied position.
The existing rf_entity_creation_object_flags helper produces the earlier
entity-specific input to this factory; it must not be mistaken for final
registered-object flags. Preserve that distinction when adding NPC owners.


### Completed physics position publication

`rf_physics_publish_position` implements prepared487962..487973: the original
calls48a230 with its own body position+e4, copying it to published+3c and
pending+f0, rebuilding bounds190/19c using radius180, setting object04000000,
and then clearing body40000000. Other body fields survive. Caller must first
establish substep completion; removal from the active-body list follows at
487976. This is inside487770, before the487e00 movement/footstep/support pass.
Accepted support contacts perform their own publication after normal physics:
4a0b31..4a0c05 copies corrected body position to object3c. Rejected contacts
do not justify an unconditional second publication. See the completed helper below.

`tools/verify_physics_publish_position.py` compares1024 cases against unchanged
487962..487973, including complete48a230 and its vector/bounds callees. Debug
name lookup is disabled. Both compiled PC and NXDK outputs match exactly,
including positive/nonpositive radius and untouched body/public state.
Four port finite-contract failures (nonfinite positions/radius and bounds
overflow) preserve every output. Both builds and all9 CTests pass. This adds
no live physics scheduling and no XEMU runtime claim.

The Ghidra4881a0 export shows a distinct linked-parent traversal: skip objects
already marked01000000, resolve+200, recursively visit the parent, propagate
04000000 and call487630 only when the parent is dirty, then mark01000000.
487630 computes attachment transforms and calls48a230; bone attachments have
additional model dependencies. These linked-object branches are source-audited,
not newly execution-verified by the publication harness. The earlier snapshot
clears01000000 to allow this traversal on the next frame.


### Live retained NPC position and vitals owners

Campaign body slots now retain published and previous positions plus creation
health, armor, object flags and opaque840. Both positions start at the authored
position; all skeletal NPCs snapshot published positions before any NPC model
update. Distance gating and rendering now read retained published positions.
Orientation and room membership remain stationary startup inputs. No physics
publication, support query, damage or AI update is connected by this change.

Startup flags use the verified entity input conversion (class model kind94),
generic486da0 additions06000000 and conditional8000, class728 bit0 fallback
20000 at422b9a, and existing creation-vitals helper. Later script/AI/attachment
mutations and the full factory call chain are not synthesized. The new
verify_npc_factory_object_flags.py executes2048 original generic/class cases;
existing1152 creation-flag and1536 vitals PC/NXDK checks pass again. This is
partial retained startup ownership, not proof of complete actor creation.

The extra40 bytes per slot participate in the existing512KiB allocation budget
and startup content hash. Empty nonskeletal slots stay zero. Failure cleanup
and close free them with the body array. L1S1 now reports NPC_BODIES
[78,78,191,33600,408240,2142580035], an increase of3120 resident/peak bytes.
PC L1S2 reports[39,38,114,17244,391236,1148085170].

Native stock64MiB XEMU report artifacts/xemu/replay-20260911-114140/report.json
passes the180-frame door/audio replay with identical PC body ownership,
playback, animation gate and rendered-geometry checksums. Base RAM67108864,
plugged RAM0. L1S2's600-frame lift replay passes on PC with unchanged animation
gate/playback. Both builds and all9 CTests pass. No framebuffer capture was
requested because this change has no new visual result. The startup body hash
does not measure per-frame position mutations; full moving-body integration
will require dynamic telemetry and original-game comparisons.


### Retained initial movement selection

NPC bodies now retain a movement-table slot selected through rf_movement_start.
It implements4339d0: out-of-range requested indices or a zero low enabled byte
select slot0, even if slot0 is disabled. The mode is descriptors[slot].index;
it must not be conflated with the slot number. The creation422dfa..422e19
adjustment clears body flag10 when that descriptor's mode is10. The campaign's
existing16 loaded descriptors outlive these owners. Named lookup remains the
validated class parser's responsibility; later movement transitions are open.

verify_movement_start.py executes complete4339d0 plus prepared422dfa..422e19,
without hooks, and compares PC/NXDK results across1536 cases. Tests vary mode
independently of slot, enabled upper bytes and invalid indices;1152 select0
and40 change body flags. Descriptor storage and unrelated inputs are preserved.
This does not claim execution of the full entity factory or support query.

The180-frame PC door replay reports NPC_BODIES
[78,78,191,33912,408552,1465601968]; the312 extra bytes store78 selected slots.
Animation gate/playback and rendered geometry remain unchanged. Both builds and
all9 CTests pass. The compiled NXDK helper is execution-verified in Unicorn;
this specific slot addition has not yet received a fresh native XEMU replay.
The previous e2bde86 native run remains the latest complete runtime evidence.
Initial animation selection now uses the same loaded descriptor resolution as
retained body startup (see below). Live support/physics remain open.


### Startup animation uses resolved movement descriptors

rf_entity_poses_start_initial now requires the16 loaded descriptors and calls
rf_movement_start before priority/movement animation selection. It reads the
resolved descriptor mode, not the authored slot index. Creation physics flags
also use the class use_kind and the mode10 adjustment, matching retained-body
startup inputs. The campaign and archive probe supply actual movement.tbl data.
No additional resident allocation is introduced. The body still follows the
first startup pose because class spheres depend on that pose.

verify_npc_startup.py passes against the original authored fixture reports for
78/38/25 skeletal actors in L1S1/L1S2/L1S3 (3087 total bone matrices), including
exact playback and bone/cache bytes and balanced release. The new PC regression
verify_npc_startup_fallback.py disables all nonzero slots using enabled256 and
sets slot0's mode to3: all78 full startup rows match direct selection ofslot0,
and74 differ from normal startup. This specifically detects the old authored
index shortcut; it is a synthetic integration comparison, not a full original
factory execution. The1536 original/PC/NXDK descriptor cases pass again.
Both builds and all9 CTests pass. Fresh native XEMU validation remains pending
for this change and the preceding movement-slot owner addition; the last native
run remains replay-20260911-114140. No new visual capture is warranted.


### Accepted support publishes the corrected position

The retained rf_physics_support_accept API now requires published[3]. After a
successful numeric contact commit it copies the corrected body position into
that owner, matching original object3c, and retains the support handle/material.
The caller must provide disjoint outputs. All validation precedes publication;
errors preserve the body, support and published position together. This helper
does not set an object movement flag or clear body40000000 as the separate
normal physics publication helper does. Those extra operations are absent from
this original support-copy block. Rejected-query behavior remains external.

verify_support_commit.py now reads the actual original3c copy, compares it with
PC and compiled NXDK output, and retains its existing2048 static/moving contact
cases. All pass, including270 upward corrections; all7 failure cases preserve
the published sentinel as well as body/contact state. The probe input remains
412 bytes; its output is now332 bytes (status, body, support, published vector).
No live caller used this helper yet, so no replay behavior changes in this fix.
Both builds and all9 CTests pass.

Separately, native stock64MiB XEMU replay-20260911-115129 passes180 door/audio
frames, closing validation for startup movement slots and resolved animation
selection through9eab386. Base RAM67108864, plugged0. NPC_BODIES is
[78,78,191,33912,408552,1465601968], matching PC; gate, playback and draw checksums
also match. This native run does not exercise the still-unconnected support
helper. No framebuffer capture was requested because there are no new visuals.


### Ground-query contact acceptance

rf_entity_support_contact_route reconstructs4a0a5c's post-query routing with
resolved lookup metadata. A fraction>=1 rejects; a normal/up dot below0.5 or
unordered rejects; a resolved type3 object with body high bit80000000 rejects.
Rejected hits return FALL when the resolved42a020 low byte is zero, otherwise
NONE. Other hits choose STATIC for absent/stale handles and MOVING for resolved
objects. No mutation, lookup, fall transition or landing effects occur here.
The caller computes the original normal/up dot and owns handle resolution.

The original fraction comparison does not reject unordered values by itself;
the dot comparison does. The helper preserves that distinction. Later numeric
support_commit still enforces the port's finite-data contract, so a NaN fraction
is not permission to mutate an actor. Do not replace the original fraction test
with!(fraction<1), which changes its branch behavior.

verify_entity_support_contact_route.py executes the original routing with real
40a0b0 dot,40a0e0 handle lookup,4136d0 and42a020 callees. Hooks only stop at final
route boundaries before effects.2304 cases include missing/valid/stale handles,
modes1/3/8, type0/3, body high bit and threshold/infinity/NaN inputs. PC and
compiled NXDK routes match:1008 NONE,504 FALL,576 STATIC,216 MOVING. Dot fixtures
use normal(0,Y,0) and up(0,1,0); arbitrary vector-dot rounding is not claimed.
Both builds and all9 CTests pass. This helper is not yet called by the NPC loop;
query, contact-owner update and mode transitions must be connected together.


### Shared fall descriptor operation

rf_movement_fall now provides the descriptor/flag part of4281a0 for NPC support
loss and existing player callers: set body bit1, request slot8 when class724
bit400 is set, otherwise3, and fall back to0 when the selected enabled low byte
is zero. It does not perform the creation-only mode10 flag clearing. Callers
install descriptors[returned_slot] and identity orientation85c; it does not
reset cached support velocity/handle or change published positions.

Player force replacement and jump now call this shared operation. The former
supplies actual class flags; jump adapts its already-resolved alternate-fall
predicate to bit400. Existing full-path comparisons still pass:512 force cases
(including original4281a0/40a270/4339d0 and callback ordering) and6144 jump cases
(144 accepted, original jump/fall with documented external predicates/audio).
Both PC and compiled NXDK results match, and all9 CTests pass. Both platforms
build successfully. Native runtime is not newly claimed by these comparisons.
The NPC caller still needs query scheduling, fall descriptor/orientation owner
installation and subsequent physics stepping connected together.


### Post-velocity landing dispatch

rf_entity_landing_finish reconstructs41993a's ordered requests after the
landing velocity adjustment. Action4 requests normal4280b0 when actor810 bit
100000 is set, otherwise slow428030(false), and does not clear body200000.
Other actions first request the special419981..4199c5 branch when class724
bit02000000 is set, then reread actor810 bit400 to choose forced crouch
428030(true) versus normal4280b0. Body200000 is cleared AFTER the stance
callback. Callbacks may modify retained flags; precomputing a complete request
list before the special branch would lose that ordering. Callback effects,
clearance, sound and velocity remain external; this is not full landing.

verify_entity_landing_finish.py compares1024 prepared original41993a executions
with PC/NXDK dispatch. Original40a130 runs unchanged; the special block and
stance routines are explicit effect boundaries. Injected callback mutations
verify request-time flags, rereads, and final state. Both builds and all9
CTests pass. This helper is not yet installed in the campaign NPC loop.

Source audit of428030 additionally confirms that its automatic slow branch
calls428a60 when crouched but continues to speed/descriptor assignment even
when standing is blocked. Normal4280b0's blocked-standing early return differs.
Do not substitute the normal/climb-exit helper for this slow branch. Existing
ordinary-player support code's direct vertical reset is still a restricted
fixture; NPC landing must preserve these class/action-dependent transitions.


### Slow/crouch landing transition

rf_player_slow_enter implements428030 for the shared player/NPC movement
state. Walk-disabled classes only apply slow speed. Walk classes force actor
crouch bit400 when the force argument's low byte is nonzero, or call the
standing adapter when already crouched. A blocked stand does not stop the
remaining slow-speed, slot1/fallback0, identity orientation and vertical-zero
assignments. Both region references remain untouched, unlike4280b0's normal
restore. Standing owns clearance, sphere/contact changes and actor-flag
clearing. It may update movement state; speed calculation happens afterwards.
Its effects are not rolled back if a later port numeric validation fails.

verify_slow_enter.py executes complete original428030 with real427fb0/40a130,
427450 and4339d0. Only428a60 is supplied at an explicit standing boundary.
512 PC/NXDK cases cover walk capability, crouch, force/enable low bytes0/1/256/
257, blocked/successful standing, forced action and network speed overrides.
Callback response mutation verifies speed calculation observes updated state.
Descriptor, actor flags, speed, vertical velocity and callback count match;
original region pointers and shared region references remain unchanged.
Both builds and all9 CTests pass. This does not implement clearance or install
NPC landing callbacks in the campaign. No new visual or native replay claim.


### Normal restore reads post-standing speed state

The existing rf_player_climb_exit (normal4280b0 restore) computed a temporary
speed before invoking the standing callback. That could overwrite response
changes made by standing's nested support/landing update. It now computes speed
after a successful stand, matching the original ordering. A blocked callback
returns before speed calculation and leaves its own effects in place. The API
comment now distinguishes no subsequent writes from rollback of callback effects.

verify_normal_enter.py runs128 complete original4280b0 cases with actual class/
crouch predicates, speed setter and named descriptor lookup. Standing alone is
supplied at a boundary and changes response from2 to9. Shared PC/NXDK output
matches, including blocked/successful attempts, enabled fallback, forced action,
network overrides, region fields and callback counts. This catches the previous
successful-standing overwrite. Existing128 climb-exit and512 slow-enter cases
also pass; both builds and all9 CTests pass. Native gameplay is not newly claimed.


### Retained NPC class movement configuration and current speed

Campaign NPC construction now owns one28-byte rf_movement_config per retained
class and12-byte current movement settings per actor slot. Class speed/slow/
fast/acceleration values are loaded once per skeletal class while the tables
archive is open; the archive scratch budget is reused sequentially. No pointers
to archive text are retained. Per-actor initialization requests normal speed
through rf_movement_set_mode, matching422e19's427450 request for the unforced
SP startup fixture. Initial response comes from body coefficient8c and entity
scale from mass98; updated response is copied back to the body. Forced actions,
class mutations and complete constructor side effects remain outside this
startup projection. Network override values remain zero and are ignored in SP.

Both allocations participate in the512KiB owner cap, startup content hash and
partial-failure cleanup. Class configs are shared instead of copied per actor;
nonskeletal classes/slots remain empty. Counts exclude allocator overhead as
before. L1S1 adds1076 resident/peak bytes and now reports NPC_BODIES
[78,78,191,34988,409628,853829663]. PC L1S2 lift reports
[39,38,114,17952,391944,3203745604]; L1S3 startup reports
[28,25,48,12184,385888,365082509]. L1S3 remains startup-only coverage.

The6000 original427450 comparisons and3 finite-contract rejections pass again;
both builds and all9 CTests pass. Native stock64MiB XEMU door/audio report
artifacts/xemu/replay-20260911-121236/report.json passes180 frames with the
L1S1 owner hash matching PC, base RAM67108864 and plugged0. Animation gate,
playback and draw remain unchanged. No new framebuffer capture. These owners
are ready for support-depth, force and landing settings; NPC physics/support
calls still need live integration, so this does not demonstrate moving AI.


## Retained class stance geometry (2026-09-11)

The campaign now owns one200-byte standing/crouching center cache per class.
Sampling moved from the player-only diagnostic into rf_entity_class_stance_build
in shared entity_assets.c; the player also uses that implementation. Resource
copies now have a heap budget instead of the diagnostic's23-motion stack limit.
Matrices and copied resource counters are temporary; source playback, resource
references and live bone caches remain untouched. Optional eye output retains
the existing player path. Failure preserves outputs and releases scratch.

NPC initialization uses the original428010 gate: actor motion slots at964/974
(class state8/state9) are compared with-1, and sampling is skipped if both are
absent. Eligible classes sample crouch after0.2 seconds using the existing
423bd0 operation sequence and class24000 center policy. Archives remain open.
First standing geometry still comes from the first authored startup actor;
this does not prove the complete original class initializer, its standing
selection/order or missing-model fallback. A selected crouch ID must satisfy
the shared playback API. NPC eye offsets are not sampled yet.

Caches join the512KiB owner cap, content hash and partial-failure cleanup. L1S1
adds1000 bytes and reports[78,78,191,35988,410628,1546073344]. L1S2 lift reports
[39,38,114,18552,392544,1626689487], and L1S3 startup reports
[28,25,48,13384,387088,3245065254]. Across those three PC replays, every output
row except NPC_BODIES and every framebuffer byte matches the preceding owner
build. Thus sampling introduces no observed live animation/audio changes in
these fixtures; this is regression evidence, not full original cache fidelity.

Both builds and all9 CTests pass. The64-frame player stance replay and288
original/PC/NXDK stance transition cases pass; those oracle cases exclude cache
construction. Native180-frame stock64MiB XEMU door/audio report
artifacts/xemu/replay-20260911-122446/report.json passes with the same owner
hash as PC, base RAM67108864 and plugged0. No framebuffer capture was requested.
Standing clearance, ground refresh and live NPC landing/physics still need to
consume these retained caches; no moving AI or additional gameplay is claimed.


## Shared standing transition (2026-09-11)

rf_physics_try_stand now sequences original428a60: prepare the endpoint from
published position, query clearance against the current crouched body, test
only the blocked result's low byte, clear actor400, resolve/clear the optional
player crouch byte, restore standing centers, then refresh ground. Callbacks
supply the existing world query, player registry and ground implementation.
Ground may re-enter stance or landing; neither flags nor spheres are reapplied
after it. Even a lookup callback's flag changes survive the center copy.
Initial validation preserves outputs; later callback errors retain earlier
effects and leave the success output untouched. No allocation or speed change.

The player diagnostic uses this sequence with its existing sweep and ground
callbacks. That fixture still supplies body position because it has no separate
published-position/player-byte owner; full actor lifecycle remains open. Its
caller still owns speed selection. NPCs retain their separate public position
and class caches but have not yet connected world/support/landing callbacks.

verify_try_stand.py compares576 executions of the full original428a60 with PC
and NXDK, retaining the original endpoint/copy callees and supplying explicit
499ed0 clearance,4a3740 player lookup and4a0840 ground boundaries. It checks
all sphere records, flags, player-byte-only clearing, endpoint and callback
order. Cases include zero through eight spheres, blocked values0/1/256/257,
missing players and flag mutations during lookup/ground. The original return
is interpreted as its low-byte boolean. Ten NXDK argument rejections and two
callback errors check preservation and partial-effect behavior. These tests
do not execute actual original world queries, cache construction or landing.

Both builds and all9 CTests pass. The64-frame process-local player stance
replay passes on PC and native stock64MiB XEMU, report
artifacts/xemu/replay-20260911-123205/report.json. It presses crouch at8, moves
at24 and stands after release at40; unobstructed clearance only. Base memory
is67108864 with plugged0. No new framebuffer capture. Blocked geometry beyond
the existing fixture and live NPC landing remain open.


## NPC startup support probe (2026-09-11)

The campaign diagnostic now samples actual NPC collision shapes against the
shared world/mover sweep at startup. It projects487e00 eligibility from the
existing unlinked, cleared-intent startup fixture, using resolved movement
mode, class use-kind, initial material and a copy of the movement flags.
Ground preparation uses each body's pending position and own spheres, class
base speed,1/30 second, and initial zero support velocity. It does not reuse
the player's class, movement mode or collider. Actor collision prepass49b900,
full original scheduling and runtime linked-parent ownership remain absent.

Walkable hits run the shared numeric support commit only on private body,
material/handle and published-position copies. No live NPC is moved, dropped,
landed or given footsteps. Moving-solid hits are reported separately; the
original moving-object acceptance/impact/landing effects are not inferred.
The diagnostic retains112 global bytes (12 summary words and16 first-miss
words), allocates no additional heap and serializes the existing sweep scratch.
The body-owner budget/hash remains unchanged. First-miss contact fields are
undefined when matched=0; only UID, decision and prepared positions are used.

Summary order: sampled, queried, skipped, missed, steep, static walkable,
moving walkable, errors, record hash, first error UID, first miss UID, numeric
position corrections. Each query record hashes UID, decision, falling, matched,
solid/face/material, status, fraction, normal, start/end Y, old/new Y. Records
are observational; a hash match does not prove original-runtime correctness.

PC results:
- L1S1:78 bodies,60 queries,18 skipped,43 static hits,17 misses,14 corrections.
  First miss is env_guard8456 at(-5.314911,1.842198,25.042206), query origin Y
  1.892198 to1.558865. This is not yet evidence that the actor should fall.
- L1S2:38 bodies,38 queries,33 static hits,5 misses,12 corrections. First miss
  env_guard9695 at(33.833450,-0.308716,50.428345).
- L1S3:25 bodies,16 queries,9 skipped,14 static hits,2 misses,1 correction.
  First miss miner18213 at(-29.634155,12.050980,55.636513); startup-only coverage.
All three report zero query errors and no moving/steep hits.

Run python tools/verify_npc_support_probe.py for the three-level report at
artifacts/npc-support-probe/report.json. It checks the pre-probe c560fea live
owner hashes and resolves first-miss records from the installed level. Existing
telemetry and every framebuffer byte match the preceding stance-owner runs.
Both builds/all9 CTests pass. Native stock64MiB XEMU180-frame door report
artifacts/xemu/replay-20260911-123851/report.json matches PC summary3755277132
and first-miss words, base RAM67108864/plugged0. No framebuffer capture.

Next: establish the first misses' original class pose/placement and floor
relationship, then connect the actual NPC physics/support/landing lifecycle.
Do not convert this diagnostic into unconditional per-frame ground commits.


## Floors below the short startup probes (2026-09-11)

Each short-probe miss now receives a separate diagnostic sweep with its endpoint
extended downward by 16 units. It uses the same sphere, flags and world/mover
geometry. The ordinary query is unchanged; this extension must never replace
its original depth. Numeric support is proposed on private state only.

All 24 misses across the three opening fixtures find walkable geometry:

| Level | Short misses | Deeper walkable hits | First UID | Proposed drop |
| --- | ---: | ---: | ---: | ---: |
| L1S1 | 17 | 17 | 8456 (env_guard) | 0.467772 |
| L1S2 | 5 | 5 | 9695 (env_guard) | 0.532832 |
| L1S3 | 2 | 2 | 8213 (miner1) | 0.351269 |

For guard 8456, the lowest sphere has center Y -0.392215 and radius 0.432211.
The deeper sweep contacts the floor at Y 0.5 and proposes body Y 1.374426,
below its authored Y 1.842198. The ordinary query ends at Y 1.558865 before
reaching that contact. The PC verifier checks the first deeper contact in each
level lies beyond the short query's reach. No deeper query reports an error.
This establishes that these shared collision queries can find floors below
all short misses; it does not independently establish original spawn settling,
class pose fidelity, moving-object acceptance or the correct fall/landing time.

The standing-pose hypothesis was checked against the existing original-code
setup slices: inspect_eye_setup.py passes all eight cases. Standing setup
raises standing weight but performs no animation update before sampling the
eye. The 0.2-second advance belongs to crouch and later standing restoration.
No extra advance was added to initial class sampling. Full first-user cache
lifecycle remains outside that prepared trace's proof.

The deeper diagnostic adds 112 global bytes: eight summary words and twenty
first-result words. No heap allocation, live-body writes or gameplay changes.
Summary: attempts, hits, walkable hits, errors, first error UID/status, record
hash, changed numeric proposals. First record: UID, matched, status, solid,
face, material, sphere center/radius, query start/end Y, fraction, normal,
contact point and proposed body Y. Contact fields require a successful hit.

verify_npc_support_probe.py now includes those details and proposed drops in
its JSON report. Existing body hashes, telemetry and framebuffer bytes remain
unchanged for all three replays. Both builds and all nine CTests pass. This
narrows the next task to faithful NPC settling/landing instead of extending
gameplay probe depth or treating the misses as missing floors.

Native stock 64 MiB XEMU also matches the PC deeper summary and first-result
words over 180 door-replay frames: artifacts/xemu/replay-20260911-124440/report.json.
Deep summary is [17,17,17,0,0,0,3049653555,17], with base RAM 67108864 and
plugged memory 0. No framebuffer capture was requested.


## Impact-damage gate needed by NPC landing (2026-09-11)

rf_entity_impact_damage reconstructs 49cd80 through the pre-region decision at
49ce1c (or rejection at 49cf34). It subtracts 7 from impact speed and clamps at
zero, optionally halves the excess, squares it, then doubles the result for
429990's resolved kind-one predicate. Damage at least 10 with object flag 4
clear is eligible for the later region-suppression/damage path. This computes
an amount and request only; it does not change health or deliver an effect.

The halving condition is NOT a movement-mode test: 42a020 must return false
and entity+1d0 must equal material 3. Collision collection loads the material
from 7c6a9c at 49c525 and stores it to entity+1d0 at 49c540. The same field feeds
material coefficient lookup 468810 in collision response. It is distinct from
the cached support material at entity+1380. The installed material table's
index 3 is flesh. The public API names this argument contact_material so a
future NPC caller cannot mistake it for its movement descriptor.

verify_impact_damage.py executes 2,400 original cases with the actual max,
42a020, 429990 and category lookup callees. Only final branch boundaries stop
the original; no predicate or math hooks substitute behavior. PC and NXDK
amount/eligibility match exactly, including adjacent floats around thresholds,
negative/zero speeds, material 3, immunity flag 4 and predicate low-byte inputs.
661 cases reach the region-check boundary. Four finite-domain rejections on
both builds preserve outputs. Complete prepared entity storage remains intact.

Both PC/NXDK builds and all nine CTests pass. NXDK code is exercised directly
in Unicorn; this helper is not yet wired into a native XEMU gameplay path.
The later 45ce50 region-suppression query, multiplayer routing, damage receiver,
death/pain sound and player camera effects still require integration. Neither
the deep diagnostic's geometric drop nor this numeric gate proves actual NPC
impact velocity or landing timing. Live NPC settling remains the next runtime
integration task, with collision material and region suppression kept explicit.


## Force-region impact suppression (2026-09-11)

rf_physics_force_suppresses_damage reconstructs the complete 45ce50 query over
the existing 108-byte runtime force-region records. It requires activation's
low byte to equal exactly 1 and flag 0x40, then checks the supplied published
position. Sphere bounds are strict; axis and oriented box boundaries follow
the shared force-selection geometry. Unknown shapes are skipped. It scans
past earlier overlapping unflagged regions rather than testing only the first
region returned by ordinary force selection. No new allocation or owner.

verify_force_damage_suppression.py executes all original collection and shape
callees for 1,536 cases with no replacement hooks. PC/NXDK agree on 62 positive
and 1,474 negative results. Coverage includes activation 0/1/255/256/257, flag
filtering, overlaps, empty lists, unknown shapes, rotated boxes and boundaries.
Explicit overlapping fixtures prove that an ordinary first match cannot mask
a later suppressing region and that byte 255 is ignored here. Four additional
port cases check malformed geometry/output preservation, inactive invalid
geometry and short-circuiting before a later malformed region. All records
remain unchanged. Only initialized OBB scratch avoids CRT exit registration.

Both builds and all nine CTests pass; the existing 768-case ordinary force
selection comparison also passes unchanged. NXDK code is tested directly in
Unicorn, not through a new XEMU gameplay invocation. The campaign already owns
the compatible campaign_forces.items/count array. The impact caller must first
run rf_entity_impact_damage, then query suppression only for an eligible amount
at the actor's published position. Actual damage delivery, actor registration
and settling/landing ownership remain integration work. Existing shared damage
vitals, credit, sound and dispatcher APIs should be reused for that backend.


## Persistent startup NPC registration (2026-09-11)

Skeletal startup NPC body owners now retain an rf_entity_view and an
rf_registered_entity_view. After class-grouped body allocation, a separate
serialized actor-order pass registers them in both the shared typed object
registry and compact entity registry. Cleanup unregisters each wrapper before
freeing its body. This establishes persistent identities for later damage and
link integration; it does not reproduce original global factory ordering.
The current port creates the player and controller objects before this pass.

The view retains class use-kind, constructor object flags and class base speed.
Action 520 is now read by the animation gate from the persistent view; snapshot
updates mirror object flags into it. Weapons and linked parents remain absent,
with action/810/7d0 initially clear in the existing startup projection. Original
inventory, attachments, AI and death-state initialization remain open.

Registration adds 68 bytes per actor slot, including slots without skeletal
bodies. Live Mines registers 78 actors for 5,304 added bytes; L1S2 registers 38
for 2,652 bytes across 39 slots; L1S3 registers 25 for 1,904 bytes across 28 slots.
Total body-owner residency is respectively 41,292, 21,204 and 15,288 bytes;
peak setup residency stays below the existing 512 KiB budget. This budget covers
these owners and their setup scratch, not total campaign memory.

NPC_REGISTRATION reports registered count, view/wrapper bytes, a pointer-free
UID/view/wrapper hash, first and last handles, and validated count. Every owner
must resolve through both registries, while a mismatched generation must fail.
verify_npc_support_probe.py checks all three opening levels and unchanged body
content hashes. Prior support proposals, animation telemetry and framebuffer
bytes remain unchanged. The existing entity-registration probe also passes
1,025 registrations plus exhaustion, duplicate and stale-owner guards.

Both builds and all nine CTests pass. Native stock 64 MiB XEMU matches PC over
180 door-replay frames in artifacts/xemu/replay-20260911-130502/report.json:
NPC_REGISTRATION [78,5304,3278717895,16843008,21889357,78]. Base memory is
67,108,864 bytes with zero plugged memory. No screenshot was requested because
this change adds runtime ownership without a new visual result. Script/UID links,
non-skeletal actor ownership, live damage delivery and NPC physics remain open.


## Authored NPC event and trigger targets (2026-09-11)

Campaign UID resolution now runs after skeletal NPC registration and before
startup event dispatch. Its temporary ordered object list includes each NPC's
authored UID, generation handle and constructor flags. It uses the existing
rf_level_link_resolve implementation of original 48a4a0/46afc0; see
TRIGGERS.md for source evidence and object-before-key precedence. Every NPC UID
is checked to resolve to its exact registered view before event links are built.
An ambiguous duplicate UID fails setup rather than silently selecting another
actor; original whole-world duplicate ordering remains outside this fixture.

| Opening level | Trigger NPC references | Event NPC references | Unresolved trigger/event references | Temporary UID table bytes |
| --- | ---: | ---: | --- | ---: |
| L1S1 | 15 | 152 | 0 / 0 | 3996 |
| L1S2 | 4 | 43 | 7 / 16 | 2604 |
| L1S3 | 4 | 44 | 13 / 9 | 1944 |

NPC_LINKS reports total UID objects, temporary table bytes, trigger NPC references
and event NPC references. No persistent UID table is added. The PC three-level
verifier checks these totals alongside body ownership and support proposals.
The 2,000-case original/PC/NXDK UID resolver comparison still passes.

Resolution does not implement the referenced actor actions. Existing event
routing continues reporting unsupported NPC targets; it does not cast them to
controller/event owners or invent AI/damage effects. Trigger-bit-4 entity
backlinks, script integration, non-skeletal actors and original world factory
ordering remain open. The table contains no diagnostic player UID alias.

Both builds and all nine CTests pass. Native stock 64 MiB XEMU matches PC over
180 door-replay frames, including NPC_LINKS [333,3996,15,152] and both full
ordered link digests: artifacts/xemu/replay-20260911-131408/report.json. Base RAM
is 67,108,864 bytes with no plugged memory. The PC final framebuffer equals the
previous registration-only replay byte for byte. No new screenshot was taken.


## Retained trigger backlinks (2026-09-11)

Each skeletal NPC owner now retains trigger_handle for original entity+838.
Constructor422360 initializes that field to -1 (near423360..4233a8). Original
load block4611d8..461200, confirmed with disassembly, calls4c0910 to test trigger
flag4, then426fc0 to resolve the object handle as an entity. A successful lookup
stores the trigger handle at entity+838. Key-owner fallback never performs this
write. Later qualifying triggers overwrite earlier ones in traversal order.
The scene now applies this step after resolving trigger links, using registered
typed entities and authored trigger/link order; event links do not write it.

The field costs four bytes per actor slot: 312/156/112 bytes in L1S1/L1S2/L1S3.
Their total body-owner residency becomes41604/21360/15400 bytes, with unchanged
body content hashes and a separate backlink digest. Four telemetry words report
write count, linked actor count, ordered UID/handle/backlink hash and added bytes.
PC records individual fields during frame-zero polling while owners are alive;
reading them after scene teardown would produce no records. No retained trace
array or extra allocation is needed.

verify_npc_backlinks.py executes original4611a1..461231 with real UID, flag,
array and typed-handle lookup callees and no substituted calls. It compares all
147 registered actor fields across four PC levels. Prepared original objects
use observed port handles; authored trigger order and flag mapping feed the
original block. Non-NPC objects and key owners are omitted in this backlink
fixture; the separate UID resolver tests cover their generic lookup precedence.
This does not establish original global factory ordering or duplicate-trigger
precedence beyond these authored cases.

The first three levels correctly retain -1 for every actor: they contain no
qualifying trigger. L4S2 supplies positive coverage: trigger1387 links NPC1287,
producing one write and one linked actor among six registered skeletal NPCs.
Its summary is [1,1,415388634,36], with body residency5256 and peak379680 bytes.
The verifier requires at least one positive write rather than accepting only
empty-field coverage. Trigger eligibility, consuming this backlink during actor
logic, AI and non-skeletal ownership remain unfinished.

Both builds and all nine CTests pass. Stock64MiB XEMU matches the PC negative
case over180 Live Mines frames (replay-20260911-131856) and the positive L4S2
startup frame (replay-20260911-132121). Reports are under artifacts/xemu; both
show base RAM67108864 and plugged memory0. L4S2 native backlink summary matches
[1,1,415388634,36]. This verifies startup ownership, not sustained L4S2 gameplay.
No new visual capture was requested.

## Motion residency after selection (2026-09-11)

The shared scene now ensures each NPC's active clips and current/next/override
state clips are resident after controller application and before pose sampling.
Startup uses the same loader. A later action selection therefore no longer
relies on the startup-only preload list. One immutable allocation is retained
per cache identity; a newly used catalog alias borrows that allocation. An
already bound clip does no archive I/O or allocation during this check.

The existing1MiB ceiling includes payloads and cache pointer/size arrays. Loads
are committed only after archive read and header validation succeed; failures
free the temporary payload and preserve the cache entry and byte accounting.
No eviction policy is added: a selection exceeding the ceiling still fails.
This is a port resource policy, not a claim about the original allocator.

The npc_motion_residency CTest exercises the private scene owner with synthetic
archive payloads: changing active selection after startup, alias reuse with the
archive stream unavailable, repeated access, invalid index, budget exhaustion,
I/O failure, mismatched header and successful retry. It adds no runtime test
hook. This test verifies residency ownership, not action choice or animation
sampling fidelity. All ten CTests and both builds pass. Three-level PC support
and four-level backlink checks pass; retained body hashes remain unchanged.

Stock64MiB XEMU passes180 door replay frames with the added per-step check:
artifacts/xemu/replay-20260911-141015/report.json. This remains the existing
startup animation selection and does not prove a live flinch transition.
Primary-weapon reset, pain gates, action start/sound and reference-aware eviction
remain open. No new visual capture was taken.

## Motion eviction under pressure (2026-09-11)

The scene loader now reclaims unused motion payloads when a new selection would
exceed its1MiB ceiling. It uses rf_entity_playback_cache_references to total
references across every model registration of an identity. Any live reference
protects the entire shared payload, including references through another alias.
All catalog file aliases are unbound before freeing a zero-reference allocation;
cache pointer, size and retained-byte accounting are then updated together.
Future use reloads the archive bytes through the existing validation path.

A full preflight rejects invalid reference counters or insufficient reclaimable
space before any payload is discarded. Reclamation visits cache identities in
stable order until enough space exists; this is a port policy, not recovered
original LRU behavior. It allocates no eviction bookkeeping. No callback or
simulation step runs between reference preflight and release. Successful eviction
is not rolled back if the subsequent archive read fails: freed identities stay
unbound and can be retried. Referenced data remains intact. An active working set
larger than the ceiling still fails rather than freeing an in-use motion.

The existing residency CTest now covers cross-registration protection, negative
reference rejection (including a later candidate), insufficient-space preservation,
complete alias invalidation, accounting, reload, rebinding and failed reload after
eviction. Pressure is prepared by setting retained-byte accounting at the budget
boundary; it is not a representative campaign working-set measurement. Both
builds and all ten CTests pass. No new XEMU pressure run is claimed; the preceding
native replay exercises ordinary residency without exhaustion. Live flinch/AI
transitions and sustained campaign cache behavior remain to be verified.
# Pain sound selection and dispatch evidence (2026-09-11)

Damage41a350 calls AI407fb0 with mode0. For that mode, a missing source or the
actor's own handle returns before any effect or random draw, regardless of the
action/class gates encountered earlier. `verify_ai_damage_noop.py` executes the
original routine and its actual gate/lookups without substituted callees across
4,096 cases: sentinel-1, absent slot, stale generation and self sources, varied
actions, class/object/entity flags and amount bits. Entire entity/class bytes
stay unchanged and effect/RNG entries are never reached. A valid different
source reaches4091d0 in a positive control. The prepared attached-player list is
empty; mode1 forced alerts and full hostile-source reactions are outside this
test. Notably4270a0 checks player selection, entity810 bit10000 and class724
bit10; it is not simply the AI flags7d0 enable bit used by the flinch gate.

`rf_scene_npc_damage_ai` now handles this proven no-op family for registered
NPC damage notifications and explicitly rejects a live different source until
the full AI reaction is implemented. The two-hit event fixture invokes it
instead of merely counting an unimplemented notification. This resolves the
AI behavior for Continuous_Damage's missing-source links and self-source actor
requests; it does not enable ordinary attacker targeting, chase or attack.
PC integration guards, all11 CTests, both builds and damage/flinch comparisons
pass with unchanged health, animation, audio and RNG results.
Stock64MiB XEMU replay `replay-20260911-152544` passes180 frames through this
handler with matching PC event/damage/flinch/pain-sound telemetry. No per-NPC
storage was added.

`rf_scene_npc_event_damage_bind` now supplies the runtime event backend with
generation-checked registered NPC lookup and the retained damage adapter.
Actor exclusions resolve linked entity+200/class1 (4290d0) and flag810 bit1
(427020); the ordinary link path does not apply these actor-only exclusions.
NPCs have no player association, so48acf0 produces no feedback. Missing stale
handles are absent; live player/non-NPC targets fail explicitly until their
own health and feedback services are connected. The caller supplies complete
reactions, clock and difficulty, and checks the retained service status after
dispatch because the original-style damage callback returns void. Earlier
health/reaction effects are not rolled back by a later error.

The two-hit guard fixture now creates and unregisters an explicit diagnostic
type17 event. It invokes the actual runtime event dispatcher with rate40 and
frame_seconds.25, then kind2 and kind-1, preserving the10-unit requests. Event
damage supplies the original final force argument1; the original health
comparison now uses that argument. Health92.800003/88, armor92.199997/87 and
flinch/audio telemetry remain unchanged. This tests the composed event/NPC
path, not an authored mission hazard or repeated gameplay damage. Global
campaign attachment still needs player feedback and complete reactions/AI.

Private scene tests also cover a stale linked handle followed by a live NPC,
health/armor writes, actor-only exclusions, and rejecting a live non-owned
entity. Both builds, all11 CTests, and the original damage/flinch comparisons
pass. AI was observed only at that milestone; the verified mode0 handler above
now covers the fixture's missing source.
Stock64MiB XEMU replay `replay-20260911-151855` passes180 frames through the
new event/NPC route, including the pain sample and guest audio-device checks.
PC health, animation, sound and RNG telemetry match. The temporary event is
removed on both success and error; no permanent campaign event is synthesized.

`rf_scene_npc_pain_sound` now connects registered, living NPC owners to the
recovered4196f0 routing, retained class groups, eye position, sound deadline and
voice identity. It selects through the verified group chooser using the
caller's RNG, loads the selected waveform through the existing bounded idle
cache, then submits spatial playback to the shared mixer/device backend.
Successful playback deliberately leaves entity+808 unchanged. Missing optional
groups make no sound; a corrupt empty selected group fails explicitly. Timer,
RNG and residency effects before an error remain committed. Dead NPCs return
unsupported until death descriptors and their effects are connected.

The explicit guard8456 two-hit fixture now dispatches pain sound after flinch
with the same seeded RNG. It plays `Grd_Smpain_03.wav` once (port sample564,
16,258 resident file bytes), retains sound deadline2000/voice-1, and finishes
with RNG3357800067. The second same-time hit consumes no further draw and plays
nothing. Animation deadlines/action remain1717/2041/22. The fixture is still
pre-frame diagnostic damage at clock1000 with seed1; it does not establish the
full campaign RNG ordering or normal weapon/event scheduling. Its sound starts
with the startup listener and follows subsequent listener gain updates.

The updated `verify_npc_pain_binding.py` executes original428740 followed by
4196f0,434da0 and48a9c0. It supplies only the documented motion/voice boundaries,
nonplayer routing and CRT TLS, and uses the actual five-entry authored Foley
group with ordinal sample IDs. Both hits match retained deadlines/action/RNG,
voice preservation, selected asset name and play count. Actual original device
playback and original loader-assigned numeric IDs are not asserted. The private
scene tests cover stale handles, missing groups, cooldown, missing samples,
shared RNG, malformed groups and unsupported death. All ten CTests pass;
the4,101-case PC/NXDK sound-helper comparison also passes.
Native stock64MiB replay `replay-20260911-150713` passes180 frames with identical
PC pain telemetry and no guest audio-device errors. Five total device starts
match the PC playback count, including the pain sample; the final8,192-byte DSP
snapshot contains4,051 nonzero samples. That snapshot contains mixed campaign
audio and is not an isolated recording of the pain sample or an audibility test.
All three opening-level support/group regression checks also pass.

NPC owners now retain entity+7d4 eye positions, with a shared 80-byte eye
record per authored class and 12 bytes per actor slot. Class startup resolves
the installed skeletal models' eye attachment, retains its local transform and
standing offset, and uses the existing private crouch sampler for crouching
offsets. Class flag20000 centers the offset in X/Z. Missing attachments retain
tag-1 and zero offsets. As with the existing collision stance cache, the pose
comes from the first authored startup actor; original global first-user factory
ordering remains unverified. Bone/virtual eye tags are not implemented here.

Each skeletal NPC refreshes its eye from the retained body publication and
authored orientation after the current animation update/gate. The recovered
4194e0 helper selects body origin, cached stance offsets or animated-tag
placement according to flags728 and controller state. Animated placement uses
the evaluated pose and recovered5034f0 transform; it does not force an extra
animation advance. NPC movement/orientation publication and original full-frame
eye-update scheduling remain open. Pain playback has not yet consumed the eye.

The private scene test covers origin fallback, standing/crouch transition,
flag20, animated pose changes and invalid-parent output preservation. Existing
original comparisons pass400 eye cases and1,000 cached tag-placement cases.
Three opening-level replays preserve the previous body/stance content hashes;
additional residency is1,336/708/816 bytes. PC and Xbox builds and all ten CTests
pass. `NPC_EYES` exposes actor count, retained bytes, class hash and final position
hash for the native replay comparison; these hashes prove agreement, not full
original lifecycle fidelity.
Stock64MiB XEMU replay `replay-20260911-145957` passes180 frames including the
two-hit guard damage fixture, with exact PC class/eye-position hashes and
unchanged pain deadlines, selected action and RNG state.

Campaign audio startup now retains the low/medium pain group pair for every
authored class alongside the footstep bindings, using the same loaded entity
table and Foley owner. The pair consumes eight bytes per class, is included
in Foley residency/peak accounting, and is released with campaign resources.
Missing optional groups stay -1. This retains metadata only; waveform loading
and damage-triggered playback are still open.

`verify_campaign_pain_groups.py` compares retained hashes to the independent
label inventory in first-authored-class order across L1S1, L1S2 and L1S3.
The added storage is respectively 40, 24 and 48 bytes; hashes are 375583174,
2439332939 and 3019285536. PC and Xbox builds and all ten CTests pass.
The native replay harness now compares `NPC_PAIN_GROUPS` against PC and includes
the additional bytes in its Foley memory assertion.
Stock64MiB XEMU replay `replay-20260911-145355` passes 180 frames with the
same 40-byte Live Mines bindings and unchanged two-hit flinch telemetry.

`tools/verify_pain_sound_dispatch.py` executes the original `434da0` selector
with the real CRT random routine, supplying only its thread-storage address.
Across 1,024 cases, invalid group IDs return -1 without drawing; signed counts
at or below one select slot zero without drawing; larger counts consume one
draw modulo the count. Valid selections and resulting RNG state match the
linked NXDK `rf_audio_group_choose` helper. No new selector is necessary.

The same harness executes the complete original `48a9c0` wrapper in 256 cases,
supplying its predicates and terminal audio-device calls. It verifies exact
flat/spatial arguments, including spatial unity volume, and compares all entity
bytes before and after. The wrapper does not save the returned voice ID into
entity+808. The live pain adapter must not invent that store. This does not
verify downstream device ownership or NPC eye-position refresh; both remain
integration work. Local evidence is `artifacts/pain-sound-dispatch.json`.
