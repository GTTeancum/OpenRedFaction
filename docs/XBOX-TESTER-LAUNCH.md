# Xbox tester campaign launch

Status: October 15, 2026. Commit
`0d5149c14d2a0ffc33b6c4859f8313ec7d7c1985` passed native stock64
compile/link/package and limited XEMU startup with 12 simulation and
10 presentation frames. See [the receipt](HOURLY-20261015-1600.md).
Actual hardware and broader functional coverage remain unverified.
Owned speech gestures are included in that passing build. Actual gestures and
save/load behavior still require tester coverage.

## Interactive package

The tester package opens the controller-driven frontend. New Game enters
`levels1.vpp` / `L1S1.rfl` using authored player position, facing and ordinary
starting state, with the selected difficulty. It enables live controller input,
interactive pacing, APU audio, NPCs, authored startup events, level transitions
and existing frontend/pause/session controls.

It grants no equipment, stages no actors, forces no events and installs no
campaign replay. The build without `--tester` remains a diagnostic package.

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

## Default controls

- Left stick: move; right stick: look
- A: jump; B: crouch; X: use; Y: reload
- RT: primary fire; LT: alternate fire
- Black: next weapon; White: previous weapon
- D-pad left/right: previous/next weapon
- Hold right-stick click: Fine Aim; left-stick click: Holster
- Start: pause/resume
- Back+Y: quick-save; Back+X: quick-load
- Back+Start: end session

Gameplay buttons can be remapped in Controls. Physical menu A/B, Start,
D-pad navigation and Back+X/Y save/load chords retain their fixed meanings.
Release held inputs before actions in a newly opened menu or session.
The frontend includes New Game, Load Game, Options and a Dashboard Exit
confirmation. End Session returns to the frontend. Save/load admission and
storage checks remain in force.

## Local input and hardware requirements

A source-only checkout needs Installed_Game archives, bluebeard.bty and three
private generated files: `build/data/geomod-template.bin`, `driller-single.bin`
and `driller-double.bin`. Preserve verified existing copies. Regeneration needs
additional private reconstruction evidence and tools, not only RF.exe.
Prepare the pinned NXDK/APU prerequisites through the guarded build scripts.
Set `RF_MENU_OVERLAY_VPP` to the verified overlay source when retaining it.
Original assets and derived templates must not be committed to GitHub.

RXDK Neighborhood deployment commands and actual console configuration still
require local verification. Launch must expose the entire tester package as
`D:\`; copying only the XBE is insufficient. Runtime requires exactly 64 MiB
reported RAM, including on upgraded consoles. Checkpoint/settings owners use
`R:`/`S:` on `\Device\Harddisk0\Partition1`. Verify deployment destination,
existing-data protection and log collection before hardware testing.
The protected emulator HDD is outside this handoff.

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

The source sweep and limited startup do not establish full-route reachability,
actual-hardware behavior, demanding-scene performance or every section's memory
fit. Broader coverage belongs to testers. Existing event and save-admission
limitations remain documented separately; older source-only statements about
missing menus or credits are superseded by dated implementation records.

Two authored destinations remain absent: L12S1/6289 targets L11S4 and
L9S1/2446 targets L9S1A. See [transition limitations](LEVEL-TRANSITIONS.md).
