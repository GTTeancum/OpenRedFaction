# Campaign level transitions

PC and Xbox campaign loading loops now follow shared deferred Load_Level (type22)
requests. The event backend queues on activation, including delayed activation;
off is a no-op. The first pending request owns its destination/entrance strings,
raw words/flags and event/source/actor IDs. No archive I/O or destruction occurs
inside dispatch. Missing backends retain unsupported-action accounting.

The shared scene stops at its next input boundary when exit following is enabled.
Scene cleanup completes before the platform releases world/collision/material and
lightmap owners and the source archive. rf_level_campaign_open searches
levels1/2/3.vpp one at a time, reads metadata without geometry allocation and
transfers the successful archive to the caller. The returned level borrows that
caller-owned handle. Missing/corrupt required archives fail immediately; absent
levels return NOT_FOUND and preserve output state. Multiplayer archives are not
searched. All 68 installed campaign entries resolve in shared tests.

The 464-byte rf_campaign_player_state carries weapon ownership, loaded/reserve
ammunition, health, armor, equipped weapon ID and the supply catalog hash. It owns
all data and contains no scene pointers/handles. Import rejects invalid vitals,
negative ammunition, invalid selection/ownership and catalog mismatch, then restores
state at first-frame combat initialization. Pistol/rifle equipment is supported;
other weapons remain unfinished. Reload/burst timers restart. Dead players have
no export. PC RF_REPLAY_PLAYER_STATE_IN/OUT blobs are build-local test artifacts,
not a stable save format.

Xbox retires its vertex buffer, fallback texture and cached borrowed image pointers
after GPU completion, while keeping pbkit framebuffer/DMA device state alive.
Diagnostic group/mover/controller owners also close before loading the next level.
Membership readiness is explicit: old diagnostic success flags cannot enable checks
against destroyed controller arrays. New membership becomes ready only after
registration validation. This fixes the NULL controller-handle access at 0x35ae5
in membership_check captured in replay-20260914-042558; the kernel bugcheck was
secondary. Startup framebuffer text is limited to boot.

Replay consumption stays global across sections, while simulation counters restart
per section. Xbox reopens the replay at its actual payload offset plus consumed
records, preserving old raw and newer header formats. The diagnostic
rf_xbox_level_transitions contains exit count, last UID, global frames at exit and
free pages after releasing the old section; rf_xbox_load_stage identifies setup
phases. The harness uses the final destination's authored metadata and section
frame count, while still checking total replay consumption/submission counts.

## Verified

- Both builds and 34 CTests pass; authored event tests cover 14 exits, delay,
  request ownership/validation and all 68 destination entries.
- tools/replay_campaign_state.py acquires/fires the L4S5 rifle, then restores the
  exact 39-round inventory and vitals in a separate L1S2 scene run; fresh-game
  behavior and catalog mismatch rejection also pass.
- tools/replay_campaign_exits.py passes automatic forward/backward PC handoffs,
  current-level suppression and exact player-state carry, each over 120 frames.
- Stock64MiB XEMU replay-20260914-043300 passes L1S1 -> L1S2 through authored
  exit9019 dispatched by the process-local fixture at frame60. The handoff occurs
  after61 frames and completes120 total. Destination actor/weapon/camera/player
  state comparisons pass, and framebuffer.png was visually inspected.
  Native telemetry is [1,9019,61,12175]: 47.6MiB free after old-level cleanup;
  the destination renderer reports5583 free pages (21.8MiB).
  Evidence: artifacts/xemu/replay-20260914-043300/report.json and its matching
  main.map/default.xbe. No host input or desktop capture was used.

## Remaining first-pass work

Current-level requests are ignored to avoid paired events masking a neighboring
exit or creating a reload loop. This is a port policy, not verified original
Load_Level semantics. Arrivals use the destination's authored spawn; entrance
alignment, doorway continuity, mission flags and deliberate same-level restarts
remain open. Respawn retains the existing fresh-supply policy. Native backward
and repeated transitions, walking into authored exit triggers, cross-archive disc
coverage and real hardware validation remain to complete. The passing native
fixture proves a handoff, not end-to-end campaign progression or PS2 visual parity.

## Authored arrival anchors

Deferred requests now own the event name and source marker position as well as
the destination payload. rf_level_transition_offset reads the destination's
Load_Level events and requires one matching name whose target is the destination
itself. It returns destination-marker minus source-marker position, without
allocation, placement or mutation of the level. Missing/empty names return
NOT_FOUND; ambiguous matches, malformed coordinates and destination mismatch
fail without changing output. Name comparison is ASCII case-insensitive.

All14 exits across L1S1/2/3 have unique matching destination anchors. L1S1->L1S2
uses (-144,-32,-48), with the inverse for returning. Both independent L1S2->L1S3
routes yield (41,32,-118), and both returns yield its inverse. L1S3->L2S1 yields
(150.25,-176,-121). Same-level marker offsets are zero. These authored-data checks
support a first-pass translation policy; no original Load_Level placement oracle
is claimed. Evidence: artifacts/arrival-anchor-data.log. Both builds and34 CTests
pass, including invalid-coordinate/missing-anchor rollback and known route deltas.

The platform loops still use the destination default spawn. Next apply the marker
translation to the departing player's pose, preserve facing, and verify clearance
and exit-trigger behavior at arrival. The raw marker positions alone do not prove
safe player placement or rotation semantics.
