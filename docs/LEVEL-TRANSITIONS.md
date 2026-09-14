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
Load_Level semantics. Named anchors translate departing position and preserve facing; missing anchors
fall back to the authored spawn. Broader arrival coverage, mission flags and
deliberate same-level restarts remain open. Respawn retains the existing fresh-supply policy. Longer repeated-transition stress tests, additional walking routes, cross-archive disc coverage
and real hardware validation remain to complete. The passing native
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

The shared rf_level_transition_place operation now applies an anchor offset to a
departing position and copies its facing matrix into destination spawn fields.
Finite-input and overflow checks preserve the previous spawn on failure. All14
opening anchor cases verify placement arithmetic and facing retention, with invalid
facing rollback. rf_scene_campaign_pose_get copies the current physics position
and combined body/eye orientation while the completed scene state remains valid;
PC exit diagnostics now emit LEVEL_EXIT_POSE before teardown of platform owners.
Both builds,34 CTests and four PC exit replays pass (arrival-pose-*.log).

Automatic placement now runs in both platform loops for normal campaign exits.
The loops copy departing position/facing before closing the source and apply the
matching anchor translation before initializing the destination player. Missing
anchors retain default-spawn fallback; malformed placement remains an error.
The remote RF_REPLAY_EXIT_UID dispatch fixture explicitly retains default spawn,
since it fires from an unrelated location. Walking fixtures use production placement.

## Walking trigger and doorway validation

rf_scene_stage_exit is a process-local fixture: it finds a unique directly linked
trigger box, chooses its thinnest horizontal axis and starts outside the volume
on the authored-spawn side. It never directly dispatches the exit. Input waits30
frames, then walks forward for the rest of180 frames; ordinary collision and
trigger code determines activation. PC uses RF_REPLAY_EXIT_START; Xbox uses
campaign-exit-start.bin through harness --exit-start-uid. This fixture does not
claim to navigate the entire preceding campaign section.

Both directions pass tools/replay_walk_exits.py with one transition at frame62,
facing preserved and continued movement after arrival:
- L1S1->L1S2: (112.159729,18.864132,-48.439453) becomes
  (-31.840271,-13.135868,-96.439453); the player continues to x=-21.093378.
- L1S2->L1S1: (-39.995773,-13.135904,-96.439453) becomes
  (104.004227,18.864096,-48.439453); the player continues to x=93.115570.

Stock64MiB XEMU replay-20260914-045205 passes the forward walking case, matches
PC destination state and renders the inspected framebuffer. It consumes all180
records with one exit9019 at frame62. Cleanup leaves13470 free pages (52.6MiB),
and destination rendering reports5589 free pages (21.8MiB). Evidence:
artifacts/walk-exit/report.json and artifacts/xemu/replay-20260914-045205/report.json.
Both builds,34 CTests and four legacy forced-dispatch PC replays pass.

This proves clearance and trigger behavior along these tested walking paths, not
all possible arrival positions. Longer transition stress tests, other anchors,
velocity/stance carry and mission-state continuity remain to complete.

## Return crossing and revisit

tools/replay_roundtrip.py now walks L1S1->L1S2 at frame62, reverses direction and
returns to L1S1 at frame264, then continues to480 total frames. It requires exactly
two transitions, verifies both translation offsets and retained facing, and checks
that pistol ammunition remains16 loaded /125 reserve. No forced event dispatch.

Stock64MiB XEMU replay-20260914-045904 passes the same round trip: three scene
loads, all480 inputs consumed, final PC state comparisons and inspected framebuffer.
Handoff history records13470 free pages after L1S1 cleanup and11917 after L1S2
cleanup. Renderer readings are6656 pages in the first L1S1 visit,5589 in L1S2 and
6667 (26.0MiB) in the returned L1S1 visit. This cycle does not show a net loss in
that renderer-phase reading; it is not a general leak-free or endurance claim.
Evidence: artifacts/roundtrip/report.json and
artifacts/xemu/replay-20260914-045904/report.json. The harness now saves per-handoff
memory/stage history instead of only the last handoff. Mission flags, world-state
persistence, velocity/stance carry and broader route/long-run testing remain open.


## Shipped legacy level names

The campaign contains273 Load_Level events.48 retain a development `.d4l`
suffix; those now resolve to the matching installed `.rfl` basename, including
L4S5 exit861 and L4S4 return592. Bare names and `.rfl` remain supported. Other
suffixes, directory traversal and path separators remain rejected.
The shared C inventory test parses every authored exit and resolves271 target
records against the installed archives. Two records have no installed target:
L12S1 event6289 names L11S4; L9S1 event2446 names L9S1A. These are tracked gaps,
not silently redirected; their reachability and intended destination need work.

The pickup return fixture dispatches authored exits at global frames60 and180.
For the second return it stages the player outside the original pickup using the
existing shared staging routine, then replays ordinary movement into collection
range. This tests persistent state through real scene/resource teardown but does
not claim the player navigated the intervening route.
