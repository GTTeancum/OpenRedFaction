# Trigger layout and authored door links

Evidence targets the installed RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Original level dispatch 460e1e calls 462150 for event section 0x600;
4610cc calls 465510 for trigger section 0x60000. Both functions are now included
in the Ghidra baseline export. This is reader/layout evidence; runtime trigger
eligibility and event dispatch have not been reconstructed.

`tools/inspect_triggers.py` follows 465510's v180 read order with bounded reads,
finite-float checks, shape validation and exact section exhaustion. It parses
2,367 triggers and 4,471 ordered links across 93 levels. Raw field names remain
provisional. In particular, `enabled_byte` is a preserved byte with unverified
semantics, not a usable runtime enabled flag. It is zero on the Live Mines door
triggers. Version-dependent pre-v180 branches are not supported.

The serialized record contains UID, name, a byte, shape, float timing (v134+
loader converts this to int), a word, flag/script/byte fields, shape geometry,
trailing configuration fields, and an ordered UID list. Shape zero reads a
position and radius; shape one reads position, nine matrix floats, three
dimensions and another flag. Matrix and dimension order remain disk order.
Original 52d640 stores only AL at 52d664, confirming that its field is one byte,
despite the decompiler's local uint declaration. No inference is made from the
uninitialized box-only local flag in the original sphere branch.

Live Mines has 61 triggers. Cross-referencing their link UIDs against the
already inventoried moving-group key UIDs gives these authored door pairs:

| Trigger UID | Ordered links | Matched controller keys / attached movers | Other links |
| --- | --- | --- | --- |
| 8542 | 8593, 8591, 9826 | Door Out 01b key 0 → mover 8543; Door Out 01a key 0 → mover 8544 | 9826 |
| 8522 | 8694, 8603, 8605, 8695 | Door Out 02a key 0 → mover 8524; Door Out 02b key 0 → mover 8523 | 8694, 8695 |

Both are records named `Trigger Door`, shape one, with dimensions [7,18,10]
in disk order and raw timing 1.0. Trigger 8542 is centered near the inspected
door at [-49.640572,-7.125,-18.602852]. Trigger 8522 is centered at
[-40.494873,-0.125,47.600555]. Trigger 8994 has no links, and trigger 20 points
to key 19 of the crate-lid rotation group. These are authored references, not
proof of runtime activation conditions or a reason to discard the other UIDs.

The cross-reference retains link order and all candidate matches, including
non-first keys or duplicate UID matches in other levels. `other_links` means
only that the UID did not match a moving-group key; event/entity resolution
remains open. Reports live in ignored `artifacts/triggers.json`.

The shared C reader in `src/core/level.c` now exposes bounded begin/next/link
access without heap allocation or runtime activation. It preserves raw fields
and ordered links; callers must keep the source archive and level alive.
`tools/verify_trigger_reader.py` compares every field and link against the
independent Python inventory: 93 levels, 2,367 records, 4,471 links pass.
Each record also rejects a one-byte truncation without changing the cursor or
output; out-of-range link access preserves output, and EOF is exact. PC and
NXDK builds and all four CTest checks pass. This verifies the reconstructed
layout, not execution equivalence with the original parser or trigger behavior.
The generated report is `artifacts/trigger-reader-verification.json`.

Next: recover UID-to-runtime-target resolution and trigger activation, then
parse the other event links. Preserve
the complete ordered list when dispatching; do not activate all door groups
globally or silently drop unresolved links. The current visible door test still
uses explicit simultaneous activation of all four translation controllers.


## Event records and remaining door behavior

`tools/inspect_events.py` follows loader 462150 for v180 and reads the 90-name
original event type table at 5a1a3c, guarded by the executable hash above.
Lookup 4bd700 calls 5001d0/57c130; the latter compares ASCII letters without
case, which matters for authored lowercase `invert`. All 93 event sections
exhaust exactly: 4,446 events and 4,901 ordered links. This is an offline
layout inventory, not a runtime event implementation or original execution test.

Read order: UID, type string, position, name string, header byte, delay float,
two bytes, two words, two floats, two strings, link count and ordered UIDs.
Types 4/63/70 (Teleport/Teleport_Player/Play_Vclip) append nine orientation
floats from version 145; type 46 (Alarm) does so from version 152. Finally,
52d170 reads four color bytes from version 176 (call at 462378, threshold at
462370). The inventory supports only v180 and preserves raw field meanings.

The formerly unresolved Live Mines door links resolve to these event records:

| Trigger | Event UID/type | Raw delay | Ordered event links |
| --- | --- | --- | --- |
| 8542 | 9826 Set_Friendliness | 1.0 | 8678, 8324, 8326 |
| 8522 | 8694 Goto_Player | 0.0 | 8696, 8697 |
| 8522 | 8695 UnHide | 0.0 | 8696, 8697 |

These names establish why selecting only door-controller links would omit
campaign behavior. Target objects and the event actions still require recovery.
The report is ignored `artifacts/events.json`.

Additional original evidence: 45ec40 only appends a word to a growable array;
it does not resolve a UID. 4c0210 only sets bit 0x10 at trigger offset 0x2b0;
it is not a call that immediately dispatches the trigger. Trigger construction
4bf970 registers object type 5, copies configuration, and appends it to the
trigger list. Runtime target resolution and activation remain open.


## Original dispatch execution checkpoint

`tools/verify_trigger_dispatch.py` executes unchanged original 4c0320 together
with its real array helpers and handle lookup 40a0e0. Four single-player cases
pass for the two authored door link lists, with and without mover suppression.
The fixture supplies synthetic registered runtime handles and intercepts only
mover activation 46aba0 and event enqueue 4b6760; those actions are not executed.
No load-time UID conversion or reconstructed C equivalence is claimed.

The list lives at trigger +0x2d4. 40a0e0 indexes table 7394cc by the low 16 handle
bits, rejects indices >=1024 and null entries, and verifies the complete handle
against object +0x2c. Thus the dispatcher expects converted runtime handles,
not the raw authored UIDs. It switches on object type at +0x24: type 8 calls
46aba0 unless the third argument's low byte is nonzero, and type 6 calls
4b6760 in single-player. Both receive target handle, source trigger handle,
and activating-object handle, in that order. Event and controller actions
remain interleaved in the authored list order. Suppressing mover calls leaves
event calls active. The generated trace is `artifacts/trigger-dispatch-verification.json`.

Other observed branches still require execution coverage: type 5 clears a
linked trigger's disabled bit via 4c0200; type 4 invokes 410e10 plus sound/timer
work; failed object lookup tries 45afe0/45b040. Multiplayer restricts event
dispatch and is outside this checkpoint.

Activation wrapper 4c0220 calls dispatch before incrementing +0x2a0, applies
count/removal and cooldown behavior, stores the global time at +0x2a8, then
sets flag 0x40. Eligibility 4c06d0 rejects disabled (0x10), already-activated
(0x40), exhausted-count and continuous (0x8) cases before checking cooldown and
actor-specific conditions. Tick 4bf740 clears 0x40. These are decompilation
leads, not yet verified portable gameplay behavior. Next recover the load-time
UID conversion and full activation state path rather than feeding raw UIDs to
the handle dispatcher or globally activating the four controllers.


## Post-load UID conversion checkpoint

The dispatch verifier now starts with the actual authored UIDs and executes
original block 4611a1 through 461231 before dispatch. Its fixtures construct
synthetic object and controller registries; the lookup code is no longer
substituted. Both door lists convert to the expected handles and all four
subsequent dispatch cases still pass. This supersedes the earlier limitation
that conversion was not executed. Eligibility and downstream actions remain
outside the test.

At 4611bf, lookup 48a4a0 scans the object list rooted at 73d890, following +0x10
until sentinel 73d880, matching UID at +0x20. UID -1 returns null. UID -999
additionally skips objects with flag bit 2 at +0x7c. For other UIDs, no such
flag filter is applied. First match wins. On success, the link receives object
handle +0x2c. If trigger flag bit 4 is set (4c0910), an entity lookup can also
write a backlink at entity +0x838; that branch is not exercised by this fixture.

When object lookup fails, 46120a calls 46afc0. It traverses controllers from
64e63c via +0x28c to sentinel 64e3b0, then each controller's key pointer array
at +0x29c in order, matching the key's first word against the UID. The owning
controller's handle replaces the link. The code searches all keys, not just
the first key. If both searches fail, the original UID remains unchanged.
Duplicate precedence, missing targets and special UIDs are instruction-derived
observations here, not covered by the two authored door fixtures.

Next implement this conversion in shared C against explicit owned registries,
preserve object-first and ordered key-owner precedence, and verify edge cases
against these original functions. General object registration, entity backlinks
and the associated event/trigger ownership are still required.


## Shared C UID resolver

`rf_level_link_resolve` in `src/core/level.c` implements the verified lookup
precedence using caller-owned ordered object rows and flattened key-owner rows.
Key rows must preserve controller-list order followed by each controller's key
order. The output records the resulting value, selected registry kind and row
index, allowing a future caller to perform the entity backlink operation without
repeating lookup. Unresolved values remain the original UID. It allocates no
memory and does not register objects, mutate registries or activate targets.

`tools/verify_uid_resolution.py` compares 2,000 deterministic cases against
unchanged original 48a4a0 and 46afc0 on both PC and compiled NXDK (Unicorn).
Cases include ordered duplicates, object-first precedence, missing values,
object UID -1 exclusion with key fallback, and -999 flag filtering. All pass.
These tests use synthetic registries, with each key row represented by one
original controller, and do not claim full registry ownership or backlink
integration. Report: `artifacts/uid-resolution-verification.json`.


## Owned trigger input storage

`rf_level_owned_triggers_open/close` retain every decoded record and raw ordered
UID list in one bounded allocation. The owner preserves authored order and all
configuration, counts its own struct and heap payload against the caller's
budget, and leaves output unchanged on error. The source must remain stable
while opening; afterward no archive or level pointer is retained. Serialized
record offsets are provenance only. This storage does not yet initialize or
register runtime triggers, resolve links or dispatch events.

The expanded `tools/verify_trigger_reader.py` passes all 93 levels, 2,367
records and 4,471 links. The ownership probe closes the archive and overwrites
the level object before emitting records and links, then compares every byte
against the independently inventoried reader output. Exact budgets succeed;
one-byte-short budgets fail without changing output; repeated close clears
the owner. Maximum PC accounted allocation is 55,696 bytes (host pointer sizes
included). NXDK compilation passes; owned-trigger execution in Xbox is not yet
validated. Allocator overhead and bounded stack scratch are excluded from the
reported budget, as with owned mover groups.


## Shared C event reader

`rf_level_events_begin`, `rf_level_event_next` and `rf_level_event_link` now
read v180 section 0x600 without allocation. They preserve type/name/text fields,
raw configuration, ordered link UIDs, optional orientation in disk order, and
four color bytes. Type comparison for the four orientation-bearing types is
ASCII case-insensitive, following 4bd700/5001d0/57c130. Other names have no
orientation payload, including an unknown name as in original type lookup -1;
this does not claim that unknown types have usable gameplay behavior.

`tools/verify_event_reader.py` matches every field and ordered link against the
independent Python layout inventory across 93 levels: 4,446 events and 4,901
links. Each record also rejects a one-byte truncation without changing output
or cursor; out-of-range link access preserves output and EOF requires exact
section exhaustion. NXDK compilation passes. Report:
`artifacts/event-reader-verification.json`. This is layout verification, not
execution equivalence with the original parser. Owned event lifetime, type
construction, registration, scheduling and event actions remain open.


## Owned event input storage

`rf_level_owned_events_open/close` retain all decoded records and raw ordered
links in one budgeted allocation, independently of the source archive and
level object. As with owned triggers, source offsets remain provenance only;
opening requires stable source data and an empty destination. Errors preserve
output, and repeated close clears the owner. Budget accounting includes owner
and heap payload, excluding allocator overhead and bounded stack scratch.

The expanded event verifier passes all 93 levels, 4,446 records and 4,901 links
after archive closure and level-object overwrite. Exact budgets succeed,
one-byte-short budgets fail without changing output, and repeat-close checks
pass. Maximum accounted PC allocation is 212,044 bytes, including host pointer
sizes. NXDK compilation passes, but event ownership has not yet been exercised
in XEMU. This provides persistent inputs, not registered runtime events or
scheduling/action behavior. Integrate both owners in the Xbox level lifetime,
then attach ordered registries and reconstruct event initialization/dispatch.


## Xbox retained level logic checkpoint

The Xbox level path now opens both owners before controller storage, sharing a
512 KiB budget and retaining them for the diagnostic level lifetime. Combined
record/link hashes are checked alongside the existing group lifetime checks,
including after the level archive closes. This integrates owned inputs, not
runtime registration, scheduling, trigger eligibility or event actions.

64 MiB XEMU run `artifacts/xemu/20260909-110830-561169/report.json` passes:
61 triggers, 184 events, 188 trigger links and 199 event links occupy 253,804
accounted bytes. All fields and links match PC hash `0a544e5f` through 66
lifetime checks (initialization, rendered actor frames and archive closure).
The subsequent 600-frame door diagnostic still matches every PC mesh hash,
with trace `22d17eea`; its four controllers remain explicitly activated by the
diagnostic. No new framebuffer capture was taken.


## Original handle pool checkpoint

`tools/verify_object_handle_pool.py` executes original initialization block
486ce9..486d3e, allocation block 486e35..486e92, release block
48684f..486895, and complete lookup 40a0e0. It verifies 1,032 allocations,
eight releases and 1,047 lookups, including full capacity, FIFO reuse, stale
handle rejection and generation-boundary seeds. The blocks exclude object
construction/destruction and therefore do not prove complete registration.
Report: `artifacts/object-handle-pool-verification.json`.

Initialization constructs a doubly linked free list of 1,024 slot nodes at
73a880 (12-byte stride) around sentinel 7394c0, in ascending slot order. The
live pointer table begins at 7394cc. Allocation takes the head's slot, installs
the object pointer, unlinks the free node, and writes object +0x2c as
`generation << 16 | slot`. The global 16-bit generation at 708744 starts at 1,
increments after successful object allocation, and wraps to 1 when the next
value reaches 0x752f. Emitted generations therefore range from 1 through
0x752e. Release clears the pointer slot and appends its node to the free-list
tail; it does not increment the generation. Full capacity is detected by the
free-list head pointing back to its sentinel before object allocation.

Lookup uses the low 16 bits as slot index, rejects slots >=1024 and null
entries, then compares the complete handle against object +0x2c. A stale
handle fails even when its slot has been reused. The next shared registry
implementation must preserve this FIFO/generation behavior and connect it to
owned object lifetimes; existing mover diagnostic handles remain scaffolding.


## Shared object registry

`include/rf/object_registry.h` and `src/core/object_registry.c` now provide
allocation-free init/insert/lookup/remove over 1,024 borrowed object slots.
A bounded ring queue preserves the original FIFO order without copying its
intrusive list representation. Handles use the verified global generation
range and full-handle lookup checks. The caller owns object construction and
lifetime, must remove objects before freeing them, and must not register an
object twice. Fresh initialization is explicit and is not a destructor.
The registry occupies 12,300 bytes on Xbox; it has not yet replaced the mover
fixture handles or been connected to event/trigger initialization.

The expanded original handle-pool verifier compares PC and compiled NXDK
registry operations against the original block results: 2,100 shared operations
pass, including 1,032 allocations and 1,047 lookups. Full-capacity insertion,
null-object insertion and repeated removal preserve registry/output on error
in the NXDK checks; PC output and subsequent operation traces match. Original
constructor/destructor side effects remain excluded. The NXDK initialization
check executes the actual library memset with a sufficient instruction budget.
Both builds and the existing CTest suite pass.


## Resident mover/controller registration

The Xbox membership path now registers resident mover descriptors followed by
controller runtime entries in the shared registry. Collision view IDs, authored
memberships and controller-parent fields use these generated handles. Lifetime
checks resolve every handle back to its resident object. Temporary collision
input IDs are replaced before attachment or queries; failure clears the registry
before freeing descriptors. This removes fixed fixture handles from the resident
Xbox mover path, while the explicit mover-first creation order remains diagnostic
and does not claim original whole-level construction order.

PC registered membership comparisons preserve all 1,421 authored attachments,
flags and rotation signs across 68 levels (1,406 movers, 1,223 controllers).
`tools/verify_registered_memberships.py` compares against the existing attachment
fixture with only handle/parent values remapped. Existing legacy probe modes
remain available for original low-level tests.

64 MiB XEMU run `artifacts/xemu/20260909-111756-906491/report.json` passes with
five registered movers and five registered controllers, including handle lookup
lifetime checks, registered collision-query results and the complete 600-frame
door cycle. Mesh trace remains `22d17eea`. The registry adds 12,300 resident
bytes. Triggers/events are retained but still need registered runtime instances;
activation is still explicitly requested by the door diagnostic. No new capture.


## Original event activation and delayed execution

`tools/verify_event_activation.py` executes unchanged 4b8b70, its timer helpers,
float-to-integer conversion, and propagation predicate 4b8c40. It passes 2,160
activation cases across all 90 types, disabled/enabled states, negative/zero/
positive delays and modes 0/1/2. Another 1,620 cases execute delayed-tick prefix
4b8ce0..4b8d45 before/at/after deadlines, including disabled flags. Three
re-activation steps verify timer replacement and caller updates. Virtual action
methods and link propagation 4b8b00 are intercepted; neither event actions nor
type-specific per-frame updates are implemented or tested here. Report:
`artifacts/event-activation-verification.json`.

4b6760 resolves the handle through 4b6800, which requires object type 6 and
returns the object pointer minus four bytes. Event fields below are relative
to that adjusted event pointer, explaining why event UID is at +0x24 rather
than the base object's +0x20. The wrapper invokes 4b8b70 with on-mode 1.

Activation stores actor at +0x2a8 and source at +0x2ac before checking flag
bit 1 at +0x2b0. Disabled activation therefore changes the caller fields but
leaves the deadline and saved mode intact. Positive delay schedules one timer
at +0x298, replacing any prior deadline, and saves the mode byte at +0x2b4.
The duration is truncated after computing delay * 1000 + 0.5; type 79
(Fire_Weapon_No_Anim) additionally multiplies delay by the original binary32
constant 0.9827237725257874 at 5897b8. Preserve this observed constant rather
than inferring a conventional time unit.

For nonpositive delay, activation clears the timer, calls virtual +4 only when
mode equals 1 (otherwise virtual +8), then conditionally propagates links.
Types 2, 3, 32, 36, 66, 69 and 89 suppress automatic propagation. The delayed
tick does not check the disabled flag: on expiry it calls virtual +4 for any
nonzero saved mode, virtual +8 for zero, conditionally propagates with the
stored source/actor/mode, and clears the timer after those calls. Action side
effects can therefore occur before timer clearing; callback ordering must be
preserved in shared reconstruction. Next implement the common event state path
with these rules, then recover downstream event actions and registration.


## Shared event activation and timer prefix

`src/core/event.c` and `include/rf/event.h` now implement common event state
activation and timer expiry with callbacks for on/off actions and propagation.
The caller supplies the state, clock and actual actions; no heap allocation,
registration or global queue is introduced. Positive reactivation replaces the
one deadline; disabled activation still updates source/actor. Immediate calls
clear the timer before actions, while expired ticks clear it afterward. The
callback may mutate the still-live state; subsequent propagation reads fields
in the original order. Type-specific per-frame update routines remain external.

The expanded activation verifier matches all 3,780 non-mutating-callback cases
against unchanged original code on both PC and compiled NXDK in Unicorn,
including final state and action order. Callback mutation/reentrancy and actual
campaign actions are not yet covered. Invalid clocks, nonfinite active delays
and durations beyond the existing timer contract are rejected before state
mutation. The type-79 computation uses double intermediates with the original
binary32 factor; the tested timing cases match x87 output, but arbitrary
float-rounding boundaries remain a verification item. PC/NXDK builds and the
existing CTest suite pass. Next validate callback-side mutations and wire event
construction, handles and authored actions into this state path.


## Callback mutation and timing-boundary verification

The event verifier now passes 3,922 PC and compiled NXDK comparisons, adding
12 callback mutation cases and 130 binary32 neighbors of selected millisecond
rounding thresholds for ordinary and type-79 delays. No core changes were
needed. Mutation fixtures change type, source, actor, saved mode and deadline
from the action, and write another deadline during propagation. Both original
and shared paths preserve the observed call order and resulting state.

The propagation decision uses the type after the action. Immediate propagation
keeps the activation's original source and mode while reading the changed actor;
delayed propagation reads the changed source, actor and mode. Immediate action
changes to the deadline survive; delayed-tick clearing overwrites them after
callbacks. The mutation cases hash callback arguments as well as action order
and compare the final state. Recursive callbacks, actual game actions and all
possible float values are still outside this evidence. The tested threshold
neighbors cover 13 rounding boundaries per delay class, five adjacent binary32
values each. Proceed to event construction/registration and campaign actions.


## Door-event constructor execution

`tools/verify_event_construction.py` executes complete original derived/base
constructors for the three door-linked event types with zero-filled and A5-filled
storage: six cases pass. The timer and empty-array constructors also execute
unchanged. The allocator, generic object factory, factory overlays, loader and
registration are excluded, so this does not establish the final loaded state.

| Type | Constructor | Bytes | On action | Off action |
| --- | --- | --- | --- | --- |
| 6 Goto_Player | 4be510 | 700 | 4babb0 | 4b9f80 |
| 30 Set_Friendliness | 4be8a0 | 700 | 4bc280 | 4b9f80 |
| 50 UnHide | 4becb0 | 704 | 4bcdd0 | 4bcde0 |

Original allocation dispatch 4b69d0 selects these constructors and sizes;
generic object allocation 487100 returns the event pointer plus four. The
shared base constructor 4bee70 calls base-object construction at event +4,
initializes deadline +0x298 to -1 and the link array at +0x29c to empty, and
sets a base vtable subsequently replaced by each derived constructor. UnHide
also initializes its additional timer at +0x2b8 to -1.

Patterned execution confirms type, delay, actor, source, flags and mode are
not initialized by these constructors. Factory 4b6870 and loader 462150 write
some of them afterward; do not attribute zero defaults to the constructor or
copy a fabricated full initialized state. Next trace remaining initialization
writes and execute the action targets above. Evidence report:
`artifacts/event-construction-verification.json`.


## Door event action effects

`tools/verify_door_event_actions.py` executes complete original Set_Friendliness
4bc280, setter 489f70, type predicate 4895f0, array helpers and actual handle
lookup. Twenty-four cases pass across six values and empty/valid/mixed/duplicate
link lists. Every byte of each synthetic target is compared after execution.
Missing, stale and out-of-range handles are skipped. Each valid object receives
the unmodified 32-bit value from event +0x2b8 at object +0x1f8. Type-0 objects
also receive -1 at +0x560; its precise gameplay meaning remains unconfirmed.
There is no value clamp or entity-only gate in this action. Ordered duplicate
references are visited by the original loop.

UnHide on method 4bcdd0 only sets byte +0x2bc to 1; off method 4bcde0 only sets
byte +0x2bd to 1. Eight patterned-state cases verify the exact single-byte
writes. This requests later processing, not immediate visibility mutation.
The subsequent method at 4bcdf0 checks these flags and a separate timer; its
full behavior remains to be recovered. Common off method 4b9f80 is a no-op
for Goto_Player and Set_Friendliness (two patterned-state cases pass).

Report: `artifacts/door-event-actions-verification.json`. Actual shared action
implementation, target-state integration and deferred UnHide processing remain
open; these checks use original code on synthetic registered target objects.

## Deferred UnHide timing

`tools/verify_unhide_deferred.py` executes the complete original 4bcdf0 method,
including original timer, array and handle-lookup helpers. It passes 108 cases
and a five-tick persistent-state sequence. Only downstream 48a660 (unhide) and
48a570 (hide) effects are intercepted. There is no player in these fixtures.

The method tests request bytes +0x2bc and +0x2bd for equality to one, not merely
nonzero. An expired timer at +0x2b8 permits processing; inactive (-1) and future
deadlines do not. The on branch runs first and sets the shared timer to now plus
500 ms before visiting links. Valid handles are processed in order, including
duplicates; stale handles are skipped. With no player, all valid targets receive
unhide and the on byte clears. The off branch also requires the shared timer to
expire, sets another 500 ms deadline, hides valid targets and clears its byte.
Thus simultaneous requests retain off until a later tick; the persistent sequence
checks the boundary immediately before and at each deadline, plus a new on request
during cooldown. Empty link arrays still reset the timer and clear the request.

The exported original method shows additional player-dependent eligibility for
type-0 targets whose friendliness is not 2. It includes three name exceptions,
predicate 45be80 (global 645320 nonzero), and query 498e80. If any valid target
fails eligibility, the on request stays pending for another timer interval.
These branches and actual visibility effects are not covered by this verifier.
The constructor leaves this timer inactive; its initial arming path still needs
to be traced before integrating an operational shared event. The vtable at
589bec selects 4bcdf0 at slot +12, replacing the base delayed-tick method.

Ghidra's automatic analysis missed the method; the export script now explicitly
creates the function at the entry verified by original-code execution and sets
its thiscall convention. Report: `artifacts/unhide-deferred-verification.json`.
