# Xbox tester campaign launch

Source implementation, 2026-10-11. Not compiled, packaged or run yet. The
parent-owned hourly Xbox batch must establish launch and stock-memory behavior.
No new campaign route, fixture, original-asset extraction or HDD operation was
performed for this change.

## What this completes

The normal build previously copied the assets and XBE but installed no gameplay
switches. With that clean disc, `main.c` ran archive/animation checks and the
static diagnostic preview. Playing required manually arranging several disc
flags. That was a practical tester-entry gap even though the gameplay systems
already had live consumers.

The opt-in tester package now starts the existing ordinary campaign directly:

- `levels1.vpp` / `L1S1.rfl`, using the level's authored player position/facing
- Real controller input, existing interactive frame pacing and APU audio
- The normal scene renderer, NPCs, inventory and authored startup events
- Automatic authored level transitions and their existing state carry
- Existing pause, save/load and death/restart controls

It does not add equipment, stage an actor, activate an event, auto-load a save or
change difficulty. L1S1 begins with its normal unarmed inventory. No title/menu
or complete-campaign runtime claim is made.

## Build and start

Use the normal NXDK/APU/generated-template prerequisites, then:

```sh
RF_GEOMOD_EXPANDED_PROFILE=0 bash tools/build-xbox.sh --tester
```

Outputs:

- `build/xbox/tester-disc/default.xbe`
- `build/xbox/redfaction-tester.iso`

Launch that ISO with stock 64 MiB RAM and an ordinary game controller. Do not
add replay files or diagnostic switches. The package contains original game
assets, so keep it private/local; this is not a redistributable release image.
The normal `bash tools/build-xbox.sh` diagnostic recipe is unchanged.

The tester build rejects an expanded resource profile and passes profile0 to
make explicitly. At runtime, its marker also requires a successful memory
query reporting exactly 16,384 physical pages. No additional resource budget
or RAM allowance is introduced.

## Controls

- Left stick: move; right stick: look
- A: jump; B: crouch; X: use; Y: reload
- RT: primary fire; LT: alternate fire
- D-pad left/right or shoulders: cycle owned weapons
- Start: pause/resume
- Back+Y: quick-save; Back+X: quick-load
- Back+Start: end the session

Release a save/load chord before another request. Saves can still be refused
by existing unsupported-state/storage checks; the previous valid save remains
owned by the existing storage code. This profile does not bypass those checks.

## Isolation and disk space

The tester packager uses its own strict, exact-name allowlist. Unexpected files,
directories or symlinks in `tester-disc` stop packaging rather than being copied
into the image or deleted. Replay input, transformed starts, forced events,
developer-room flags, frame limits and fixture archives are not allowed.
`campaign-level.bin` is not packaged, and tester mode ignores that override.

Archive/BTY and generated-template inputs are hardlinked read-only into this
separate staging tree. Packaging never writes through these links. It replaces
only generated directory entries, its owned marker and its ISO. The diagnostic
disc/ISO, original input bytes and HDDs are untouched. No full asset-tree copy
fallback occurs when hardlinks are unavailable; same-filesystem support is then
a packaging requirement. The completed ISO still needs approximately one full
asset image of free space. Existing generated-artifact cleanup can remove these
borrowed `.vpp` links without deleting the original file's other directory entry.

NXDK `Makefile` lines39/59/99/107 use `OUTPUT_DIR` for `default.xbe` and the
source directory for `GEN_XISO`; explicit command-line values select these new
paths. Project objects/main.exe remain shared because the executable reads a
runtime marker, not a different compile-time gameplay definition. The existing
profile stamp invalidates project objects when returning from an expanded build.

## Source-level launch trace

`tools/build-xbox-tester.sh` writes exactly `RFTEST1\n` to
`tester-profile.flag`. `main.c` validates the full eight-byte marker and EOF,
selects the scene path and suppresses separate startup particle/animation
self-checks. It does not bypass world/collision/resource construction.

The first level still goes through `rf_level_open` with the ordinary default
`L1S1.rfl`; no selection override is opened. `scene_preview` calls
`rf_scene_set_campaign_spawn(level)` both before and after live-control setup,
so the latter cannot revert to the old preview camera. That shared function
copies the loaded level's finite player position and orientation directly into
the campaign player owner and enables `campaign_spawn`.

Tester mode takes the existing input-open branch with `limit=0` and
`player_replay=NULL`. It therefore selects live SDL input, `player_pacing=1`,
ongoing scene streaming and the existing interactive audio branch. Finite
frame-limit/replay files are ignored in tester mode even if somebody manually
constructs an invalid package. `rf_scene_follow_level_exits` is enabled through
the same scene setup, and subsequent handoffs reuse it.

The old post-session moving-door diagnostic is disabled for tester mode. No
additional actions run after the tester ends the session.

## Campaign sweep result and remaining boundaries

The source already connects authored objective counters and conditional gates,
auto/contact/use triggers, mission dialogue, cutscenes, countdowns, failures,
section transitions and final defuse/completion. Relevant owners are
`rf_runtime_goals_initialize`, `campaign_trigger_contacts`,
`rf_runtime_events_tick`, `campaign_show_message`, `campaign_cutscene_tick`,
`campaign_load_level` and `scene_script_defuse`; Xbox handoff is in `main.c`.
These systems should not remain described as entirely missing merely because
all natural routes have not been played.

The sweep did not establish full-route reachability, performance or every
section's memory fit. Existing unsupported event parity, two legacy missing
level names, save-admission restrictions and the placeholder completion/credits
presentation remain separate limitations. Delayed Reverse_Mover admission was
noticed but deliberately deferred as an isolated defect outside this broad
entry-to-gameplay slice. No new runtime observations are claimed.
