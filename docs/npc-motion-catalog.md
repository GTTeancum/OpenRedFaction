# NPC motion catalog

The campaign now builds `rf_entity_motion_catalog` after base/weapon binding load.
Each shared skeleton has one motion registry; class base and weapon maps refer to
that registry. Per-actor playback remains inactive. This does not add visible NPC
animation, selection, AI or damage behavior.

## Evidence and scope

Original RF.exe SHA256: b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
The recovered entity factory at 0x422360 visits weapon groups before base mappings.
Cache helpers 0x539be0/0x539d00 compare case-insensitive last-dot stems and preserve
the first acquired name. Registry block 0x51cc42..0x51cc93 keys on resolved motion
identity and the exact loop byte. The catalog composes the existing reconstructed
helpers, processing seed classes in order and their retained weapon groups before
base mappings. Classes sharing the decoded skeleton share registry entries.

This is port ownership and stable IDs, not exact original global IDs or reference
counts: original global cache ordering, descriptor loading, named timing markers
and the actor selector remain incomplete. Temporary caches are per model. Do not
infer motion identity from the compiled filename, whose conversion uses the first
dot. Identical resources with different loop bytes remain different entries.

## Lifetime and budget

The catalog owns mappings and exact-count motion metadata arrays. Temporary cache,
registry and resource arrays are freed per model. The budget includes the owner,
retained heap allocations and peak temporary allocations, excluding stack and
allocator overhead. Campaign cap is 512 KiB, in addition to existing binding owners.
Sound labels remain in those binding owners; the new catalog does not resolve IDs.
Catalog maps and file metadata survive closing source owners, but the motions
archive and skeleton index order must remain valid. Motion keyframes are still
archive-backed, not resident animation data. Cleanup is repeatable and failures
preserve the empty destination.

## Validation

`tools/verify_base_action_sets.py` independently derives canonical declarations,
registration keys and weapon-before-base maps. L1S1/L1S2/L1S3 produce 248/237/243
shared resources and retain 63004/59948/62160 bytes on PC; peak construction totals
are 123727/123135/103843 bytes. These are PC ABI counts, not Xbox memory readings.
The probe also checks exact and one-byte-short budgets and file access after source
owners close. A synthetic two-class/shared-model fixture verifies case/suffix
aliasing, distinct last-dot identities using the same compiled file, distinct loop
flags and transactional rejection of an invalid mapping. All nine CTest tests pass.
Both PC and NXDK builds pass. Native replay separately checks loading and existing
door/audio behavior; it does not establish NPC animation or exact catalog bytes.

Stock 64 MiB XEMU replay: `artifacts/xemu/replay-20260911-072311/report.json` PASS (180 frames, door/audio fixture).

## Timing-marker correction

A subsequent instruction-level trace corrected the earlier interpretation of
0x51cd30 as alternate clips. It selects the already-registered cache descriptor,
compares two 16-byte marker names at +0x40 and +0x54 case-sensitively, and calls
0x51ccb0 only when the name is absent. That helper fills the first empty slot,
retains existing bytes after the terminating NUL, and writes an int32 time at
slot+16. A full descriptor is unchanged. No additional clip is registered.

The entity table parser at 0x41cd4e..0x41ce0e recognizes `+Footstep Trigger:` and
stores `footstep_left` then `footstep_right` with two floating-point values.
The factory later sends those declarations to 0x51cd30. The conversion multiplies
by binary32 0.03333333507180214, then 30, then 160 before truncating; it must not be
simplified to multiplying by160 if exact original arithmetic is required.
`rf_motion_marker_register` matches the entire original124-byte descriptor in
1500 cases, including duplicate/full slots, and has five PC/NXDK guard checks.
The original helper and its real conversion callee run unchanged with53-bit x87
precision. `tools/verify_motion_markers.py` records this evidence. Table binding
and consumption by live playback remain open.

The seemingly preparatory 0x5034d0 call at factory0x4231cc simply returns wrapper+4
for model types1..3 (otherwise NULL); it does not initialize animation playback.
The following factory writes initialize logical current0, next-1, duration0 and
elapsed0 before calling selector0x41f270. That selector still must run before any
claim about the initial visible pose.

## Authored footstep binding

The shared catalog now receives base-state `+Footstep Trigger` pairs. The bounded
state reader retains two frames per declared pair and rejects duplicate pairs or
malformed numbers without changing output. Existing state-only callers still
receive the same motion name and IDs. Sparse weapon binding storage does not add
footstep declarations; the recovered factory registration calls occur in the base
state loop. This distinction does not imply weapon-specific footstep behavior is
fully understood.

A temporary cache processes base classes in seed order and states in canonical
order, registering left then right as at factory0x423005..0x423044. First named
registration wins. Marker masks/ticks are then copied to every matching catalog
resource, across skeletons and loop flags. Cache size is capped at800 and included
in peak allocation accounting. No automatic actor motion or footstep audio starts.

`verify_state_markers.py` checks all153 authored pairs plus6 guards; full-table
catalog checks on L1S1/L1S2/L1S3 independently match all masks/ticks (13 marked
resource entries per level). A fixture checks conflicting duplicate declarations
retain the first time and a one-shot registration receives the shared markers.
Catalog PC resident bytes are now65980/62792/65076, peak128719/128127/107755.
The metadata comparison probe compares fields rather than indeterminate padding.

Stock64MiB XEMU loading/door/audio regression: `artifacts/xemu/replay-20260911-073427/report.json` PASS, 180 frames. This does not verify live footstep playback or byte-for-byte native catalog contents.

## Default weapons and startup ordering

The class reader at0x41bf57..0x41bfe8 initializes primary/secondary IDs to-1,
reads required quoted names and only calls4c81f0 when the name is nonempty.
Unknown nonempty names also retain the lookup result-1; they are not parser
errors. `rf_entity_default_weapons_read` now retains these fields in each class
binding. All63 installed classes match an independent raw-table/name-order
comparison, plus three lookup cases and four transactional input guards.
Three-level owner checks confirm the retained IDs after loading. This is not
yet actor inventory ownership or execution of the complete original parser.

Factory422360 initializes the inventory through402c20, assigns class primary
at422d19 onward and secondary afterward, before selector41f270. Ghidra and
instructions identify42ab20 as the effective animation-map overlay: restore base
records from+d24/+e94 into+8e4/+a54, then copy weapon-specific16-byte records
only when their first word (motion ID) is not-1. It also remaps one special
weapon via globals85cd00/85ccd8. This runtime fallback is separate from the
exact-group table readers. Full overlay reconstruction and startup use remain
open; merely storing the default weapon does not justify changing live maps.

Other inspected constructor calls:4243c0 builds numbered interface prop-point
metadata;424520 prepares class/model prop metadata. Neither is a replacement
for running the actor selector. The existing initial-player-motion fixture
covers selected creation fields and both player/nonplayer flags; it does not
prove every authored NPC's initial state or complete factory execution.

PC/NXDK builds and all nine CTest checks pass. Stock64MiB native loading/door/audio replay `artifacts/xemu/replay-20260911-073931/report.json` passes180 frames; it does not verify runtime NPC weapon choice.

## Effective weapon mapping overlay

`rf_motion_overlay_bindings` reconstructs the record-copy portion of42ab20:
copy all23 base states and45 base actions, then replace each complete16-byte
record when its weapon motion ID is not exactly-1. The auxiliary words remain
opaque and are copied with their motion ID. A NULL state/action group represents
the original nonpositive declared-count path. No playback cursor is changed.
The caller must first handle entity/model gating and the weapon alias.

`verify_motion_binding_overlay.py` executes full original42ab20 and its real
40a1e0 predicate, with valid skeletal fixtures and the original alias globals.
All1024 cases match PC/NXDK record bytes; the entire actor buffer is checked for
unexpected writes. Cases include negative and zero declared counts, independent
missing groups, arbitrary auxiliary words, non--1 negative motion words and
in-place output/base alias in the port helper.

`rf_entity_motion_mapping_overlay` applies that rule to compact shared catalog
IDs. It validates matching class/skeleton ownership, accepts a missing weapon
map, preserves output on malformed input and allows output to alias either map.
The installed independent verifier checks21 effective groups per opening level
(63 total). Action sound labels must come from the same selected source group as
the action record; numeric sound resolution and live switching are still absent.

The weapon initializer at4c65ff..4c6622 establishes85ccd8 from `machine pistol`
and85cd00 from `machine pistol special`. Thus42ab20 maps the special variant to
the ordinary machine-pistol animation group. This identity is now traced, but
runtime alias selection is not yet connected to catalog lookup.

PC and NXDK builds, the1024-case CPU comparison and all nine CTest checks pass.
No new emulator gameplay behavior or visual change is claimed for these helpers.

## Weapon ID to selected mapping

`rf_entity_motion_selection_base` creates a compact base mapping with borrowed
pointers to the class action sound labels. `rf_entity_motion_selection_weapon`
implements negative-ID no-op behavior and resolves the machine-pistol-special
alias before finding a sparse group. It restores base mappings and overlays
present weapon records. An absent weapon action preserves its base sound label;
a present weapon action uses its own label, including an explicit empty string.
The binding owner must remain alive while a selection's sound pointers are used.
These functions allocate no persistent storage and do not equip weapons or start
animations. Invalid class/weapon IDs preserve output; non-skeletal base requests
return NOT_FOUND. A missing alias destination is an explicit port error.

The three-level independent verifier checks all44 weapon IDs for each skeletal
class:220 selections in L1S1,88 in L1S2 and176 in L1S3 (484 total). It compares
all68 effective motion IDs, resolved weapon/skeleton IDs and all45 sound labels.
The probe also checks base initialization, negative no-op, out-of-range output
preservation and non-skeletal rejection. PC/NXDK builds and CTest are checked;
this is not native live-actor switching validation.

Startup timing remains separate: the default-weapon42ab20 call precedes the
factory's later base mapping population/copy. Do not apply the default weapon
at an invented point merely because its ID is now available. The original first
41f270 selector call and subsequent503360 update must be reproduced with their
actual constructor inputs before claiming a faithful initial pose.

## First-selector evidence and remaining assumptions

`inspect_npc_initial_selection.py` executes full original41f400 using actual
opening-level base maps, class movement modes/flags and default weapons. The
402d68..402dac scalar initializer span executes unchanged. Thirty-three cases
(11 class/level combinations, three prior values for actor+1380) match the PC
priority/movement composition. env_guard/miner1/run, Grabber/robot-fly and
Cutter/bat/hover retain logical current0/next-1. Fish/swim requests next18
(swim_stand), duration0.25, elapsed0. This is selection only, before41f270's
weight/time update and503360's first playback advance. No pose is rendered.

The fixture explicitly uses a nonplayer with no links, zero velocity and zero
unspecified actor state, kind0 classification and no external events. Actual
class flags, mode and default weapon IDs are loaded; the full allocator/factory
and all prior side effects are not reproduced. Per-case actor read offsets are
recorded to guide the remaining constructor audit. It would be incorrect to
promote these fixtures to proof of all authored NPC startup behavior.

The older `inspect_initial_player_motion.py` now tests actor+1380 at0,-1 and
123456 instead of assuming-1. Its24 cases still match original41f270 versus PC
movement/controller composition. Neither that fixture nor the33 NPC cases reads
+1380 along the exercised path, so changing that value does not affect results.
This is not proof of the field's initial value in other branches.

`inspect_entity_constructor.py` executes40e380 and its actual member constructors
on32 patterned1494-byte buffers. The constructor preserves+1380 and the other
listed selector-related scalar samples; it does not blanket-zero actor storage.
The observed member write ranges are retained in the report. Allocation policy
and later generic/entity factory writes are excluded. In particular, the later
explicit+1380 write cannot be moved earlier without checking its consumers.

No C implementation changed in this audit. Next work is to materialize verified
constructor inputs and run the first controller/playback update against loaded
motion envelopes, then connect pose evaluation. An unconditional idle animation
for every NPC would already contradict the observed fish selector path.

## Loaded envelopes and first weighting

Each catalog resource now retains its track-zero envelope from the validated
motion file. The comparison-track choice matches the existing playback adapter;
the new startup check does not claim a complete original model loader. Raw archive
header/track bytes independently verify all728 envelopes across the three levels.
PC catalog resident bytes are70940/67532/69936 and peak137039/136447/114275.
The synthetic alias fixture now uses one real loaded motion for its deliberately
different cache names, rather than an archive-less file placeholder.

`inspect_npc_initial_selection.py --controller` now materializes the original
loaded-character control structures and calls full41f270, including its selector
and actual weighting callees. It uses the real catalog registry counts, envelopes
and loop flags. Thirty-three creation-field fixtures at zero,1/60 and1/30 second
deltas give99 matching PC results for the entire compact playback/controller state.
The same99 controller inputs execute the compiled NXDK helper with equal results.
A variable-count development probe replaces the old32-resource fixture limitation.

The original fixture uses one cache descriptor for all registrations sharing an
identity, including loop/nonloop aliases. Reference counts therefore compare to
the sum of the port's per-registration counts. These counts are not interchangeable
without aggregation: a future resource release owner must balance the shared cache
identity, not unload it just because one registration has no references. Live
resource lifetime ownership remains open.

This still excludes complete actor construction,503360's first cursor advance,
pose sampling and rendering. It proves more than the logical-selector check, but
must not be reported as animated NPCs or a complete initial pose. Existing native
replay only checks successful catalog loading and its established door/audio path.

Both builds and nine CTest checks pass. Stock64MiB XEMU replay `artifacts/xemu/replay-20260911-080246/report.json` passes180 frames after envelope retention; its coverage remains loading and existing door/audio behavior.
