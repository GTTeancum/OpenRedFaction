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

### Factory arming and player eligibility

The same verifier now executes type-specific factory overlay 4b8880, reached
from loader 462150 case 0x32. Twelve cases cover generic-factory success/failure,
zero/A5 storage and three clocks. With generic factory 4b6870 intercepted, the
complete overlay clears both request bytes and calls the original timer setter
with offset zero. Its deadline therefore becomes the current clock, ready on
the next processing call. All other patterned bytes survive, and allocation
failure returns null without touching the supplied storage. This resolves the
previously open initial-arming question; generic object initialization remains
separately incomplete.

Ninety-six player-present cases now execute the complete deferred method with
real string comparison 5001d0 and global predicate 45be80. Query 498e80 is
intercepted with hit/miss results while asserting its four arguments: player
+0x7d4, target +0x3c, flags 3 and null output. Bypasses are nonzero object type,
friendliness 2, names `masako_fighter`, `capek` or `gryphon`, or nonzero global
645320. Name matching is case-insensitive (`CAPEK` bypasses); `capek_extra`
does not bypass. The fixture counts query calls to verify short-circuiting.
Otherwise a query hit allows unhide; a miss keeps the on request pending.
Both outcomes reset the timer and leave a simultaneous off request pending.

498e80 is the moving/static collision-ray wrapper already reconstructed as
`rf_collision_ray_solids`; see docs/COLLISION.md for its independent original
execution comparisons. A hit here means an obstruction permits revealing the
target, not a clear line of sight. The current event test substitutes the query
result, so it does not yet prove event-to-world collision integration. Visibility
effects 48a660/48a570 and entity registration remain to be reconstructed and
connected before this becomes operational gameplay.

## Visibility effect dispatch and direct writes

`tools/verify_visibility_effects.py` executes complete original hide 48a570 and
unhide 48a660 in 80 synthetic target cases plus two null calls. Original handle
lookup, type filters 426fc0/410c70, array helpers, component setters and recursive
child calls execute unchanged. Seven downstream subsystem calls are intercepted:
505b50, 429770, 41ae70, 42ed20, 502b00, 503390 and 48c9a0. The verifier compares
their ordered argument traces and all 24 KiB of fixture storage. This establishes
the caller's direct writes and dispatch, not the intercepted callees' effects.

Both methods accept null. Hide sets object +0x7c bit 0x4000; unhide clears it.
The entity lookup resolves the object's handle and accepts only type 0. For such
entities, both methods traverse the linked list rooted at +0x268, following node
+0x150 and setting byte +0x140 to zero/one. They also visit the pointer array at
+0x1418, setting component byte +0x28c via 42d8d0/42d8e0. Tests cover two nodes
and two components, preserving all surrounding bytes. These structures' detailed
subsystem meanings still need tracing.

If entity +0x804 is not -1, 505b50 receives that value and float 0/1. Unhide
then calls 429770(entity). Each of the two handles at +0x145c/+0x1460 is resolved
through the type-4 filter and recursively hidden/unhidden; a stale second handle
is skipped in the fixture. Hide subsequently calls 41ae70(handle, entity+0x2a4).
If +0x13d8 is nonzero it calls 42ed20(value,0), then clears that field.

After the entity-specific branch, unhide checks object +0x80. If nonzero, it
queries 502b00; return value 3 triggers 503390(value,0,1.0f). Finally, if object
flags contain 0x8000, unhide calls 48c9a0(object), then clears that flag using a
fresh read after the call. Tests cover both resource-query outcomes, empty/full
attachments, type-0/type-1 targets and five flag patterns. They do not cover
callback mutations, cyclic child references or subsystem internals. Report:
`artifacts/visibility-effects-verification.json`. Recover those dependencies
and map these fields into shared entity runtime before implementing gameplay
visibility; a render-only flag would omit the demonstrated side effects.

The 41ae70 boundary is already reconstructed as `rf_weapon_reset` after entity
resolution (docs/WEAPON.md). Here +0x2a4 is the weapon index. Reuse that shared
reset and its existing subsystem adapters when integrating hide; this fixture
only establishes when it is called and does not revalidate its internals.

## Shared deferred UnHide scheduling

`include/rf/event.h` and `src/core/event.c` now provide `rf_unhide_state`,
`rf_unhide_init`, `rf_unhide_request` and `rf_unhide_tick`. State occupies eight
bytes on PC/NXDK (deadline and two request bytes, with padding). No allocation
is introduced. Initialization arms the zero-offset timer and clears requests;
request methods set only their own byte. The tick implements the separate
type-50 vtable method, not the common delayed-event tick prefix.

The caller supplies ordered handles and a target callback. That callback must
resolve each handle, count missing targets as processed, check eligibility for
unhide, and perform actual visibility effects. Returning zero for an existing
ineligible target retains the on request. Hide ignores the callback result.
All links are visited even after a denial, preserving duplicate visits. The
timer is reset before target callbacks; off is checked against the timer again
after on processing. Links must stay unchanged and live throughout the call.

`tools/verify_unhide_deferred.py` now compares the original state and action
traces with `tests/unhide_probe.c` and the actual NXDK-linked functions executed
in Unicorn. All 223 comparisons pass per build: 209 scheduler cases (including
the persistent sequence), six successful factory initialization cases and eight
request cases. Target callbacks supply the already established eligibility
result and simulate visible effects; no live entity registration, collision or
subsystem integration is claimed. Generic factory failures remain original-only
evidence. The common event regression still passes 3,922 PC/NXDK comparisons;
both builds and all four CTest checks pass. No new XEMU visual test is warranted
until this code is connected to the runtime.

## Shared trigger eligibility (2026-09-10)

rf_trigger_eligible reconstructs 4c06d0 from a stable trigger snapshot and
resolved actor/registry facts. Reject flags 0x10/0x40/8, compare activation
count and limit as signed values (limit -1 is unlimited), then use the
original cooldown predicate. Filter 0 requires 4895d0 unless flag 2 is set;
filter 3 rejects 48aaf0 result exactly one; filter 4 requires entity lookup,
429990 and nonzero 48aaf0. Filter 2 searches allowed handles. Flag 1 requires
a nonzero low input byte; flag 2 requires actor kind 2 and owner 48aaf0;
flag 0x80 requires entity lookup and 4290d0. Attached handle -1 bypasses the
410c70 existence check. Other filter values impose no filter-specific gate.

Predicate field names retain source addresses to avoid prematurely assigning
subsystem meaning. Seven lookup/predicate boundaries are supplied to the
oracle: 4895d0, 48aaf0, 426fc0, 429990, 4290d0, 410c70 and 40a0e0. Original
4c06d0, cooldown and array helpers execute unchanged. The portable helper
does not perform callbacks, registry traversal or mutate trigger state; facts
must remain stable. Input validation precedes evaluation and preserves output
on errors, which is a port contract rather than original invalid-input behavior.

verify_trigger_eligibility.py passes 4096 exact PC/NXDK decisions, with 149
accepted fixtures. It covers signed counts, flags, timer endpoints, filters,
allowed lists, missing entities and low-byte values, and verifies original
trigger/actor storage remains unchanged. Report:
artifacts/trigger-eligibility-verification.json. Both builds and six CTests
pass. This supersedes the earlier unverified eligibility lead, but not the
remaining contact geometry, live actor predicate resolution, trigger firing
or per-frame reset/lifecycle work. No XEMU gameplay or new visual claim.


## Sphere contact (4bf620)

`rf_trigger_sphere_contact` reconstructs the complete sphere predicate. The
original subtracts actor position (+3c) from trigger position (+3c), stores
three float components through 409fa0, then sums their squared values in XYZ
order through 40a180/4faf00. It accepts distance squared <= trigger radius
(+78) squared. It does not add actor radius, test a swept path, or reject a
negative radius before squaring. The port rejects nonfinite coordinates,
radius and overflowing stored differences while preserving the output.

`tools/verify_trigger_sphere.py` executes unchanged original code and all
callees, then compares compiled PC and NXDK decisions: 4016 finite cases and
80 port guards pass. Fixtures cover signed/zero radius, exact and adjacent
float boundaries on all axes, and scales 2^-30 through 2^30, under explicit
53-bit nearest x87 arithmetic. Original trigger/actor storage is unchanged.
This is linked NXDK code execution in Unicorn, not a native XEMU gameplay test.

Next contact evidence: ordinary box branch of 4c0a80 (flag 0x20 clear) calls
508660 with trigger center +3c, matrix +48, dimensions +2c8 and actor vectors
+e4/+f0. 508660 transforms endpoints into local coordinates, creates bounds
at +/- half dimensions, calls verified 508b70, and transforms its output
back even after a miss. Actor vector lifetime/meaning, exact transform
arithmetic and the directional flag 0x20 branch remain to be reconstructed.
Do not replace these paths with center-in-box or generic sphere overlap.
Live actor predicates, dwell/key handling and activation remain open.


## Oriented segment/box contact (508660)

`rf_collision_segment_oriented_box` reconstructs the helper used by 4c0a80
when trigger flag 0x20 is clear. It subtracts the box center from both endpoints,
stores float offsets, then computes matrix-row dots in ZYX order (4fac60).
Full dimensions become bounds at +/- half size. The unchanged reconstructed
508b70 segment/AABB routine supplies the hit and local point; 4facb0 then
rotates the point by matrix columns and 40a350 adds the center.

The final transform runs even after rejection. Therefore callers initialize
point: a trivial local miss transforms its incoming value, while failed plane
attempts transform their last written point. No nearest-hit sorting, matrix
normalization or inverse-matrix substitution is added. The port uses local
scratch and validates finite inputs/nonnegative dimensions, preserving outputs
on errors, including intermediate overflow.

`tools/verify_collision_oriented_box.py` passes 4016 original/PC/NXDK cases
and 80 invalid-input guards. It executes the full original geometry path and
callees with static initialization flags already set (omitting exit-handler
registration only). All hit and output float bytes match: 1851 hits and 2165
misses writing a point. Fixtures cover rotated and general matrices, degenerate
boxes, zero-length segments, boundaries and scales 2^-15..2^15, under explicit
64-bit nearest x87 arithmetic as used by the existing AABB verifier. This is
compiled NXDK code in Unicorn, not an XEMU campaign contact test.

4c0a80 passes actor +e4/+f0 as endpoints. Existing creation evidence in
COLLISION.md shows 48a230 initializes both from the supplied position; mover
evidence updates +f0 from +e4. The campaign actor update lifetime still needs
connection. Flag 0x20 uses a separate directional face-crossing path and is
not covered by this helper. Eligibility, dwell/key handling and natural event
activation must be integrated before claiming playable campaign triggers.


## Directional box contact (complete 4c0a80)

`rf_trigger_box_contact` now includes both ordinary and flag 0x20 branches.
The directional displacement is actor +f0 minus +3c, with float component
stores. Its length must exceed float 0.00001 and its dot with trigger forward
(matrix row +60) must be negative. The plane query starts at actor +e4, not
+3c; substituting either of these positions changes results.

The forward face has corners A=(center+right)-up+forward,
B=(center-right)-up+forward, C=(center+right)+up+forward and
D=(center-right)+up+forward, with basis vectors scaled by half dimensions
and each vector operation stored as float. The plane normal is the original
forward row, and its offset is minus dot(normal,A). 5065b0 calls 506550,
stores scaled displacement before adding the start, then calls 506dd0.
That helper selects the largest absolute normal axis (Z wins ties), uses
sign-dependent projection axes, and tests barycentric coordinates with its
original +/-0.0001 small-component branch and intermediate float store.

The first triangle is A/B/C. On rejection, the original copies D over C and
retries A/B/D: it does NOT switch to the complementary triangle. This leaves
a gap near the upper center. For an identity box of dimensions (2,2,2), the
segment (0,.75,2) to (0,.75,0) misses while neighboring x=+/-.75 cases hit.
The port preserves this observed behavior. Four separate 12-byte allocations
are established by 4bf6d0 at globals 85682c..856838; pointer aliasing does not
explain the gap.

`tools/probe_trigger_directional.py` records the unchanged original's query,
plane and triangle inputs with a read-only hook; named fixtures assert the
gap, second-triangle retry, movement gates and distinct query origin.
`tools/verify_trigger_box.py` compares complete original 4c0a80 with PC and
NXDK code: 8192 cases, including invalid guards, under 53-bit nearest x87.
The corpus includes both branches, rotations around multiple axes, distinct
actor positions, reverse/stationary movement, boundaries and scales 2^-8..2^8.
All original geometry callees run unchanged; scratch storage is supplied.
This verifies compiled routines in Unicorn, not native campaign activation.
Live actor update ordering, eligibility predicates, dwell/key gates and firing
remain open. No contact routine added here mutates trigger or actor state.


## Contact delay before activation (4bfc60)

`rf_trigger_contact_delay` reconstructs the +2f8 seconds / +2fc deadline
stage. Its accepted input is the resolved eligibility-and-contact result;
its ready output means continue to the key gate and activation, not event
success. Rejection clears the timer only when seconds > 0. Nonpositive
seconds bypass waiting and preserve the deadline. For positive seconds,
40a0d0 considers every negative deadline inactive. An inactive timer arms
with trunc(seconds*1000+0.5) through original 4fa360 and returns waiting,
even when the offset rounds to zero. An active timer waits for 4fa3f0,
then clears to -1 and proceeds. Failed contact after arming cancels it.

`tools/verify_trigger_contact_delay.py` runs full original 4bfc60 along the
SP no-key path with eligibility/contact supplied and 4c0220 activation
captured. Both eligibility rejection and contact rejection are exercised.
The CRT float conversion, armed query, timer set/expire/clear helpers run
unchanged. All 1504 fixtures match compiled PC/NXDK deadline and proceed
result, and original trigger/actor storage outside the deadline stays
unchanged. Cases include signed/zero/positive delays, submillisecond values,
inactive deadlines, cancellation, boundary clocks and period wrapping.

The port validates finite seconds, boolean acceptance and the timer clock
range, preserving outputs on errors. The seconds-to-ms rounding here is
different from the existing cooldown initializer's trunc(seconds*1000).
Key-related behavior after 4bfdb7 and general 4c0220 activation remain open;
this helper does not connect a diagnostic actor to campaign triggers.
