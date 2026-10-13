# Ordinary authored humanoid patrols and node dwell

Status: source-written and independently source-reviewed for integration in the
next parent-owned stock-64-MiB Xbox compile/package rollup. No worker compilation,
tests, runtime, campaign routes or image work was performed. Behavior remains
unverified by execution. This is a bounded first-playable implementation.

## Implemented behavior

Ordinary authored grounded humanoids can now use their existing named Loop and
Ping Pong default patrols, including nodes with a finite wait. The bounded
arrival-facing follow-on also admits finite oriented nodes and original-inert
tag lists; see NPC-PATROL-ARRIVAL-FACING.md. Admission uses the actual class physics flags, resolved grounded/falling
movement mode, skeletal model, live body/seat ownership and selected stand/walk
motion resources. It no longer relies on a guard1/guard2 class-name whitelist.
Missing optional model/motion support leaves the actor outside this adapter;
malformed supported resource metadata remains an error. Existing animation-slot,
motion-cache, navigation and checkpoint limits are retained.

At arrival, the movement owner stops locomotion, advances the route cursor and
shared direction exactly once, and holds for the reached node's authored wait.
Ground/support processing continues while translation waits. Expiry clears the
hold and moves toward the already-selected next node; it cannot schedule the
same node's wait a second time merely because the actor remains at the node.
Loop wrapping and both Ping Pong endpoints retain the existing direction rules.
A command, combat takeover, death, retirement, seat attachment or loss of the
ordinary default-route owner cancels the hold through the existing narrow hooks.

No new node pool, navigation graph, route owner or actor class is synthesized.
One way paths and executable node-tag lists remain outside this slice. Raw
source identities stay unchanged when an original-inert tag list is admitted.

## Original-source basis and live consumer

Read-only input: Installed_Game/RF.exe, SHA-256
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
Addresses below are original image virtual addresses.

- 0x46483c..0x464874 binds actor fields +0x748/+0x74c from authored
  waypoint data before 0x407ee0 enters default action 4 through 0x4270f0.
- 0x4059f0 services action 4; 0x405adf calls 0x403a80 for the existing
  ordinary acquisition/combat path. This slice preserves that ownership.
- 0x407a50 handles node arrival, stops locomotion, consumes the reached
  navigation candidate's +0x24 float wait, and advances cursor/direction.
  0x407b12 is the shared direction-advance path.
- 0x407c92..0x407ca8 multiplies the wait by 1000, converts it to an integer
  and sets the timer at AI +0x41c.
- 0x40b6d0 receives AI +0x2e8; 0x40b71b checks +0x134, the same AI +0x41c
  timer. While pending, it may face an authored vector and returns before
  translation. The arrival-facing follow-on services the first disk vector
  through ordinary steering only during that existing hold.

The port keeps its existing arrival radius, body-target navigation space and
60-Hz simulation clock. Wait conversion is ceil(seconds * 60), capped by
admission at 3600 frames (60 seconds), rather than a claim of exact millisecond
retail timing. Negative/non-finite data is malformed; waits above the bound are
unsupported. Positive sub-frame waits last one simulation frame. Unsigned
bounded deadline subtraction supports wrap; presence is independent of a zero
remaining duration or a numerically zero deadline.

Read-only installed-level analysis identified 60 grounded humanoid Loop/Ping
Pong source candidates across 13 classes with no oriented/tagged nodes. The old
plain guard-only predicate admitted 24 source candidates. Thirteen of the new
source candidates contain authored waits. These are source candidates, not a
runtime-admission count: actual model/clip, seat and current-owner gates can
narrow the set. Examples include L6S1 admin_male UID4964 with 9-second waits,
L6S1 guard2 UID5114 with 16/3-second waits, and L17S2 merc_com UID18608 with a
3-second wait. No campaign route was played to obtain these counts.

## Continuation format and joins

The existing rf_campaign_actor_route owns the receipt. Its in-memory size is
40 bytes, up from 36: flag 64 means a dwell is present and the added uint32_t
holds remaining simulation frames. Presence with remaining 0 explicitly means
due now. Absence requires a canonical zero word. Dwell requires live default
action 4, origin 2, an enabled patrol and active Loop/Ping Pong path.

RFNC32 retains the existing 36-byte fixed route wire lane and appends exactly
4 bytes to the end of each flagged NPC row, after all prior conditional tails.
Unflagged rows gain no dwell bytes. RFNC1-31 decode with zero dwell state and
cannot claim flag 64. Plain standalone codecs refuse this composed receipt,
matching the existing finite alarm/retreat lanes.

RFCH10 retains the existing 140-byte fixed actor rows. After the complete actor
array, it appends one 4-byte remaining value for each flagged actor, in actor-row
order, before carry data. RFCH1-9 decode with zero dwell state. Size arithmetic,
version gates, actor keys, retirement checks and final exact length still apply.
Neither RFNC row limits nor the history/whole-save storage cap increases.

Candidate admission joins the actual authored default path/mode, current-level
waypoint-section hash, next cursor, shared direction, next target, live body,
resolved movement and actual selected model/motions. Inverting the already
advanced cursor/direction identifies the reached node without an extra saved
node identifier, including Loop wrap and two-node Ping Pong. Remaining time must
not exceed that node's own finite positive authored wait. Noncanonical absent,
terminal, combat, seat, scripted-physics and conflicting command lanes fail.

Same-level restore stages the rebased deadline before publication. Outgoing and
incoming actual stand/walk clips enter the existing resource demand/seal path;
copy-based resource enumeration uses snapshot state without comparing a copied
view's address to the retained live registration pointer. Live admission still
checks the exact registration/view identity. The existing full owner, route,
placement, playback, resource and history joins stay binding. Dwell selects the
existing guarded final-storage-close transaction, and publication occurs only
at its successful final publish step.

Section history has a different existing placement policy: a revisit rebuilds
authored placement/navigation rather than preserving the actor's departed body
position. It resumes the saved remaining hold at that reconstructed placement,
with a fresh frame-zero deadline and the saved next cursor/direction. This does
not claim a restored reached-node pose. The same-level placement owner remains
responsible for exact body continuation; an unnecessary proximity-to-reached-
node restriction would also reject supported platform carries.

The arrival-facing follow-on uses semantic RFNC34/RFCH11 reader capability
versions with these same byte layouts. It conservatively promotes every enabled
patrol, including previously supported paths, and adds no retained fields.

## Memory and bounds

The growth below belongs to the earlier dwell implementation, not the
arrival-facing follow-on; the follow-on adds no retained or serialized bytes.

- Live patrol state adds two uint32_t words: presence and deadline, 8 bytes per
  existing actor body. Existing copied owners grow correspondingly.
- Existing route values grow 4 bytes; each 2048-slot history route array grows
  8192 bytes. Existing snapshots remain fixed-capacity and sizeof-budgeted.
- Each restore entry adds a 4-byte staged deadline, in addition to the growth
  of its existing route/record/body copies. Each publication seal row adds its
  exact 40-byte saved route plus 4-byte staged deadline.
- Future stand/walk demand reuses the existing 8-request per-owner allowance and
  56-request publication enumeration; no request-cap increase is made.
- Motion demand retains the existing 1280-KiB ceiling and 16 active slots. The
  local resource scan is fixed at 21 IDs (2 future, 3 controller, 16 active).
- Existing stage/scene/whole-file admission must include the new sizeof values
  and continues to refuse excess usage. Actual peak stock64 fit is unmeasured.

## Integration notes

The external patch is based on bfa3fdf25f727d8e8fb7aa15c34f86af862064e1.
Later scripted explosion-trail changes are additive. Preserve their scene/load
hooks while composing the narrow patrol hunks. The real repository and protected
HDDs were not modified by this worker. External evidence and generated diff
artifacts remain outside tracked source. Testers own functional/route coverage;
the parent retains hourly compilation and a short startup smoke only.
