# Entity views and locomotion predicates

`include/rf/entity.h` exposes compact, caller-owned views of the original fields
required by the current predicates. They are not binary-compatible RF.exe
objects or fully initialized gameplay entities. The registry contains 1,024
pointers (4 KiB on the supported 32-bit targets); no predicate allocates memory.
All views, seat arrays and attachment snapshots must remain stable during a call.

Object lookup follows 0x40a0e0: -1 is invalid; the low 16 handle bits must be
below 1,024; the indexed pointer must exist and its complete handle must match.
Negative handles other than -1 may be valid. Entity lookup adds the type-zero
requirement from 0x426fc0. Entity class is info +1b4, as used by 0x486c90 for
type-zero objects; the view does not implement other object classifications.

Weapon presence follows 0x408dc0. Either weapon index at entity +2a4/+2a8
differing from -1 is sufficient. Otherwise the separate owner pointer at +2a0
must refer to an owner whose class base speed (+294 -> +50) is zero, matching
0x40a2a0. 0x427da0 supplies the first occupant handle differing from -1 from
the owner's +8cc seat list. Only that handle is resolved, then weapon presence
is checked recursively. A stale first occupant does not cause a search for
another seat. The original repeats failed traversal twice; stable views allow
the same result from one traversal. Non-finite owner speeds are rejected;
cyclic chains return RF_FORMAT after the registry bound instead of overflowing
the original call stack. Null input has no weapon in the shared API.

Readiness follows 0x41f950. Entity +810 bit 0x10 immediately succeeds. Otherwise
a weapon is required; +7c bit 8 or an attached type-zero entity whose linked
handle (+200) equals this entity's full handle succeeds next (0x48aaf0).
The attachment input is an ordered snapshot of handles from the original
0x7c75cc circular list, not a new ownership system. With no such attachment,
actions +520 equal to 7/16 or +7d0 bit 4 reject readiness. Bit 2 succeeds for
a living entity; otherwise bit 0 supplies the result (0x408e90).

The full combat gate also requires +810 bit 0 clear (0x427020), and rejects
+810 bit 0x800 unless a valid linked type-zero entity has class 4 (0x428e60,
0x429f90). Ready and eligible are distinct outputs: a forced-ready or attached
entity can still fail the complete gate. Outputs remain unchanged on malformed
views. Slot allocation, generation changes, seat/list mutations, class loading
and gameplay entity creation are not implemented by these APIs.

`tools/verify_entity_predicates.py` executes all original callees unchanged for
2,007 fixtures, comparing object/entity lookup, weapon presence, readiness and
eligibility. It includes generation mismatches, slot 1,023/1,024 boundaries,
negative valid handles, null/stale pointers represented through the registry,
seat chains and attachment links. A separate C-only fixture rejects a cycle.
The local report is `artifacts/entity-predicates-verification.json`.

The shared animation diagnostic now uses an equipped-weapon index and +7d0 bit 2
to derive eligibility, replacing its forced combat flag. It checks both outputs
against original instructions in all 64 frames and includes them in its state
hash. It remains a scripted miner-rig fixture; later physics/state selection,
reset/audio adapters and actual gameplay initialization remain open.


Factory vitals assignments (2026-09-11)
--------------------------------------

rf_entity_creation_vitals reconstructs two numeric blocks in422360, using a
compact caller-owned state. Original422a80..422a9e copies class+48 to entity+38
(armor) when the low network-mode byte is zero; otherwise it writes positive
zero. Original422cb4..422cea compares class+44 health against zero with x87,
copies class+764 to entity+840, and assigns health/object flags. Negative or
unordered health produces100 and ORs bit4 into object+7c. Nonnegative health,
including negative zero, copies its original bits without clearing existing bit4.
The class+764/entity+840 field is retained without a guessed semantic name.

These blocks occur at different points in the factory with other calls between
them. The helper composes only their numeric writes using caller-supplied class
values; it does not reconstruct those intervening calls, generic allocation,
class parsing, later overrides or a finished entity. It does not reset unrelated
damage state. Actual source metadata and persistent NPC ownership remain required
before attaching the existing damage/burn runtime. The current scene registers
only its compact player view; raw level NPC records are not live NPCs.

verify_entity_creation_vitals.py executes both original blocks and checks every
other byte in the0x1500-byte actor remains unchanged. It compares1,536 cases with
PC and actual NXDK-linked C, spanning initial object flags, signed zero, positive/
negative health, infinities, quiet/signaling NaNs and low-byte network values.
This includes bit-preserving SP armor copying. PC rf_entity_probe
--creation-vitals consumes32-byte cases and returns16-byte projected states.
Report:artifacts/entity-creation-vitals.json. Both full builds and nine CTests pass.
No native XEMU actor construction or new visual behavior is claimed.


Authored health, armor and FOV binding (2026-09-11)
------------------------------------------------

rf_entity_vitals_config_read/load now projects the required $FOV, $Envirosuit
and $Life fields from entity.tbl into the creation-class input. Original
41bcba..41bd0d calls5126a0/512920 for these three fields in that order. It stores
Envirosuit at class+48 and Life at+44. FOV is multiplied by binary32 constant
3c8efa35 (pi/180 approximation), then0.5, converted by x87 FCOS and stored at+764.
This identifies the previously unnamed word copied to entity+840: its bits are
the cosine of half the authored FOV, not an unrelated opaque setting.

The port reader selects the first ASCII-insensitive class name and requires all
three fields, rejecting duplicates and malformed numbers without changing output.
FOV0..360 is the supported domain. It uses the existing bounded decimal parser;
no NXDK strtod stub. Loading borrows the archive, allocates one scratch block
bounded by the supplied budget, and releases it before return. Returned fields
retain no text/archive pointers. No defaults or inherited values are fabricated.
This is a selected-field port parser, not the complete original table reader.

verify_entity_vitals_config.py reads the actual tables.vpp member and covers all
63 installed classes. Original41bcba..41bd0d executes with only token/number reads
supplied; actual FOV arithmetic/FCOS and field stores are retained. Every class
field matches the PC archive loader and NXDK text reader bit-for-bit, while all
other original descriptor bytes remain unchanged. Each PC archive case also
checks an insufficient scratch budget preserves output. Five malformed/missing
fixtures check the PC and Xbox reader guards. Installed FOV values are60,90,120,
180 and360; no claim of full original parser behavior or every possible FOV value.
The Unicorn verifier explicitly checks completion before inspecting outputs.

Report:artifacts/entity-vitals-config.json. Both builds/nine CTests and the1,536
factory-assignment cases pass. These values are ready for the persistent NPC
constructor, which is still pending together with registration, model/physics
ownership and live damage/burn scheduling. No new native XEMU gameplay is claimed.


Persistent construction inputs (port ownership)
rf_entity_seeds_open retains full validated v180 entity records, their recovered
spawn projections, and one vitals/physics entry per ASCII-insensitive class.
Class names borrow only owned record storage. It reads entity.tbl once, releases
table scratch before return, and leaves no archive pointers in the result.
The explicit budget covers owner, all retained bytes and peak table scratch;
stack and allocator overhead are excluded. Errors preserve the empty destination
and release provisional allocations. Close clears the owner and is repeatable.
Campaign loading now opens this owner under a1MiB budget before closing tables;
campaign cleanup releases it. These are construction inputs, not registered
NPCs: model/material ownership, actor initialization and behavior remain open.

PC --seeds probe checks exact-budget success, one-byte-short failure with empty
output, repeatable close, class mappings/vitals and raw access after archives
close. Installed L1S1/L1S2/L1S3 pass: records/classes/resident/peak bytes are
78/5/103309/477949,39/3/51997/426637,28/6/37463/412103 respectively (PC ABI).
Both builds and nine CTests pass. Native stock64MiB door/audio replay
artifacts/xemu/replay-20260911-062422/report.json passes after integration.
That replay verifies loading and existing scene/audio behavior, not an independent
comparison of every retained class field or live NPC gameplay. No new visuals.


Model kind recovered and retained
Original5143f0 returns the final-dot extension (even in a directory component),
or the empty string when absent. Original41ba4d..41ba9b compares it through
5001d0/57c130 against .vfx then .vcm, storing class+94 as3,2,or1 respectively.
ASCII case is ignored. An empty model still classifies as1; it does not prove
a model exists or that a loader should be invoked. Kind3 drives creation flag
10000 in422360, and the kind is copied into its physics construction descriptor.
rf_entity_model_kind uses bounded63-byte input and preserves output on failure.
Each retained seed class now includes its authored model name and this kind.

verify_entity_model_kind.py executes actual original extension extraction and
comparison callees; only the intermediate rf_string wrapper is supplied. All86
ASCII fixtures/installed names match PC and NXDK, with other descriptor bytes
preserved. A64-byte name tests the port guard. This does not verify full model
loading, texture/animation ownership, or live actor construction. Both builds,
nine CTests and native64MiB replay-20260911-062852 pass.
Updated PC seed resident/peak bytes: L1S1 103649/478289, L1S2 52201/426841,
L1S3 37871/412511. All three lifetime/budget probes pass.


Shared campaign skeleton ownership
rf_entity_skeletons_open owns decoded immutable bones once per compiled V3C
name (ASCII-insensitive). A class-index array shares resources across classes;
non-kind2 classes explicitly map to UINT32_MAX. It uses the existing validated
model directory and BONE decoder, rejecting duplicate/missing BONE sections and
zero or more than256 bones. No original cache/refcount equivalence is claimed:
this is a bounded port lifetime owner. Model names and bones survive closure of
mesh archives and seed records. Temporary directory/payload allocations are
released between models; peak budget includes those plus all retained arrays.
Stack/allocator overhead remains outside accounting. Errors release provisional
resources and preserve the empty destination; close is repeatable.

Campaign construction opens this owner with256KiB after seed loading and closes
it during cleanup. It is not yet an actor pose or a geometry/material/animation
cache. Non-skeletal resources, per-instance pose/playback, and actor registration
remain open. PC probes for L1S1/L1S2/L1S3 check exact/insufficient budgets, a
late missing-model failure, class mappings and valid hierarchy access after
source closure. Class/resource/resident/peak counts (PC ABI):5/5/5504/15236,
3/2/3252/13448,6/4/4140/14336. Both builds and nine CTests pass.
Native stock64MiB L1S2 lift/audio replay-20260911-063337 passes600 frames
with PC state parity and nonzero guest DSP output. This establishes successful
loading and preserved replay behavior, not independent native bone-byte proof.


Per-actor playback and pose storage
Constructor chain:486da0 ->489fe0(kind2) ->5029c0 ->501050 ->51ae90. The
skeletal factory strips the last extension before loading. 501050 obtains a
shared descriptor through51d780 and separately allocates the1d5c-byte character
instance. Full51ae90 clears state, sets motion selections at1cfc/1d00/1d48
to-1, generation lowword1cf8 to1, phase1d04 to0 and no active slots/events.
Its bone caches use50 entries. This precedes later entity motion registration;
it does not establish a standing motion or bind-pose evaluation.

rf_motion_playback_initialize reproduces the compact playback-field projection.
verify_motion_initialize.py executes the complete original constructor and all
its real callees, with an empty instance-list root and caller-supplied descriptor
address.128 randomized prior buffers match PC/NXDK exactly. Original global
instance-list ownership and full instance layout are not recreated by this helper.

rf_entity_poses_open now retains distinct playback, matrix and generation storage
per authored skeletal actor, indexed alongside seeds. Matrices/stamps start zero
and therefore invalid against generation1. No active motions, pose evaluation
or NPC rendering is claimed. Skeleton data stays shared and its index order must
remain stable. More than50 bones rejects character construction. This owner is
for new, inactive instances; releasing live motion references must precede array
cleanup once playback is connected. Campaign uses a1MiB budget and releases
pose arrays on exit. Budget excludes stack/allocator overhead.

L1S1/L1S2/L1S3 budget and ownership probes pass, checking exact-budget success,
one-byte-short failure, distinct matrix/stamp slices and playback initialization.
PC actor/bone/resident-byte totals:78/1689/106002,39/950/58288,28/448/30152.
Both builds and nine CTests pass. Authored motion/resource registration and
original initial state/action selection remain open before any live NPC update.
Native64MiB door/audio replay-20260911-064148 passes180 frames after pose
ownership integration, with PC parity and nonzero guest DSP output. This is
loading/regression evidence, not active NPC animation validation.


Retained base state motion bindings
rf_entity_base_motions_open reads entity.tbl once and retains canonical23-state
mappings/file descriptors for each skeletal class. It shares the exact parser
and local registration path with rf_entity_state_set_open. Files borrow the
caller-owned motions archive, which campaign loading now holds until cleanup.
No table/seed pointers remain. Budget includes owner, class arrays and temporary
table text; no entire motion payload is loaded. Exact/insufficient budget probes
and post-table-close track reads pass for L1S1/L1S2/L1S3. PC class/local-file/
resident/peak values:5/38/32696/407336,3/31/19624/394264,6/36/39232/413872.

This is port-owned base binding, not complete original registration. 422360
first registers weapon-specific state/action maps across64 groups, then skeletal
base23 states and45 actions;51cc10 deduplicates on resource pointer AND looping
byte. Base alternate clips additionally go through51cd30. Our per-class local
indices are not asserted equal to original shared descriptor indices. Weapon
mappings, actions, alternate clips and global registration ordering remain open.

After registration,4231d1 calls5034d0, initializes current state0,next-1 and
blend duration/age0, calls41f270 (which runs selector41f400 or vehicle logic),
then503360. Therefore state0 alone does not establish the first visible standing
pose; initial selector inputs still need a complete actor. Playback stays inactive
in the port owner. No newly animated/rendered NPCs are claimed.

Both builds/nine CTests pass. verify_entity_state_sets.py checks181 groups,
180 resolved,972 local motions and1137 state references; edf_ship retains its
known missing-resource failure. Full original registration-loop equivalence
and disk-free runtime sampling remain unverified.
Native64MiB door/audio replay-20260911-064631 passes180 frames after base
motion-owner integration, with PC state parity and nonzero DSP output. This
proves loading/regression coverage, not live NPC animation.


Base action motion mappings and sound labels
rf_entity_action_read parses exact +Action triples (name,motion,sound label)
within a base or weapon group, ignoring ASCII case for selection. Empty quoted
fields are retained; missing groups do not fall back. Duplicate selected actions,
missing sound fields and overlong retained names fail without publishing output.
This is a selected-field port reader, not original table-parser equivalence.
verify_entity_actions.py independently extracts1168 installed declarations and
compares PC full-table and NXDK class-block reads, plus malformed/group guards.

Retained campaign class sets now contain23 base state and45 base action mappings.
Canonical action order is4181d0/table5caee0. Local registries first contain the
base states (loop byte1), then actions (loop byte0). They deduplicate on cache
identity AND loop byte, matching the recovered51cc10 registration key; a shared
filename in both roles has distinct local entries. Sound labels remain strings;
numeric sound-class lookup is still pending. rf_entity_state_set_open continues
to return only its requested state group, with action mappings absent. Internal
capacity is68 resources, enough for23 states plus45 actions. Motion payloads
remain archive-backed; disk-free sampling is separate.

verify_base_action_sets.py independently computes canonical local indices,
filenames and sound labels for495 action slots across L1S1/L1S2/L1S3. Budget
and post-table-close probes pass. Updated PC class/resource/resident/peak totals:
5/100/111336/485976,3/89/66808/441448,6/95/133600/508240. Existing181 group
state checks still pass (180 resolved, known edf_ship missing resource retained).
Both builds and nine CTests pass. Original weapon-first/global registration order,
weapon actions/states, alternates, sound IDs and initial selector remain open;
no active NPC playback or new visual result is claimed.
Native stock64MiB replay-20260911-065339 passes180 door/audio frames after
base action integration, with PC state parity and nonzero DSP output. This is
loading/regression evidence, not live NPC action playback verification.


Weapon IDs for NPC animation groups
Original4c6570 clears the loaded weapon count before4c67a0. The latter processes
primary records in order, saves their count at87211c, then appends secondary
records.4c81f0 scans names at85cd08,stride550,up to872448, returning the first
ASCII-insensitive match through500190/57c130, or-1. Entity table weapon-specific
groups use that lookup at41cfad; group IDs must not be guessed from group order.

rf_weapon_names_read/load retain up to64 names of63 bytes, requiring primary
and secondary section delimiters. This is a selected-field port parser, not the
weapon statistics loader. It preserves output on error and uses bounded table
scratch. rf_weapon_name_find reproduces first-match lookup, including empty and
duplicate names. Installed names resolve44 entries:40 primary,4 secondary.
verify_weapon_names.py compares PC/NXDK names/order and140 lookups against
actual original4c81f0/string callees. It executes4c6843..4c68d9 sequencing with
record parsing/end tokens supplied. NXDK stack probing is supplied on a mapped
fixture stack; native kernel stack handling is outside that CPU-only verifier.

The campaign motion owner now retains this weapon-name catalogue. Each class
also owns a two-word mask of declared +Weapon Specific IDs, resolved from the
loaded table. Unknown names and duplicate groups fail without publishing output.
The independent three-level check compares masks with raw table group names and
loaded weapon order, alongside495 base action slots. Both builds/nine CTests
pass. Updated PC motion-owner resident/peak bytes: L1S1 115480/490120,
L1S2 70936/445576, L1S3 137752/512392. Scratch lifetimes do not overlap.
Weapon-group state/action resources are still not registered; this establishes
their IDs and ownership before that next step. No live NPC animation is claimed.
Native stock64MiB replay-20260911-070422 passes180 door/audio frames after
weapon catalogue/group-ID integration, with PC parity and nonzero guest DSP.
This verifies loading/regression behavior, not weapon-group animation playback.
