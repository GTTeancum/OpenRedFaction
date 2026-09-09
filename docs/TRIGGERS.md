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
