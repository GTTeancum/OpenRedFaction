# Xbox pause and session controls

Source update, October 12, 2026: the tester-profile startup/New Game frontend now
returns End Session to the main menu and permits a fresh Live Mines session.
This extension is uncompiled and unrun; see
[Xbox startup and New Game](XBOX-STARTUP-AND-NEW-GAME.md). The earlier build result
below establishes only the original pause-menu slice.

Update: the parent stock64 Xbox batch compiled and linked this feature in source commit `7168ad0c4704c1f762c44f612302b37ebbe31a14` at 02:09:47 UTC on October 12, 2026. No runtime was performed; earlier source-only handoff wording below describes the pre-integration state.

Source implementation, October 12, 2026. Compilation and Xbox behavior are
unverified until the parent-owned hourly stock64 batch. This is an operational
first-playable menu policy, not a reconstruction of the original retail menus.
No campaign traversal, gameplay fixture, HDD access or save/load run was used to
implement it.

## Tester controls

Start opens a visible pause screen. D-pad up/down chooses an action, A selects
it, and B or Start resumes. The actions are:

- Resume: return to the suspended scene.
- Quick Save: resume and request the existing ordinary manual save at the next
  simulation boundary. The existing HUD reports its actual success or refusal.
- Quick Load: confirm with a fresh A press to request the existing manual save.
  B cancels confirmation; Start resumes without loading. An unavailable or
  incompatible save is still rejected by the ordinary loader.
- Restart Section: confirm with a fresh A press to reload the current section
  through the same fresh-section owner used by death restart. It starts from
  that section's authored/default player state and loadout. It does not carry
  current health, ammunition, equipment, pose or unsaved mission changes, and
  it does not load a checkpoint. The confirmation states the reset explicitly.
- End Session: confirm with a fresh A press. In the tester-profile package,
  ordinary scene retirement returns to the main menu, where A or Start begins
  a fresh New Game in Live Mines. Diagnostic packages retain the SESSION ENDED
  terminal screen and reboot requirement. Neither path returns to the dashboard.

The existing Back+Y quick-save, Back+X quick-load, Back+Start immediate session
exit, and death X recovery/B restart remain available. Only menu load/restart/
exit add a confirmation screen; existing direct shortcuts keep their semantics.

## Controller loss and input ownership

Open-ended live play pauses if no controller is present, including before its
first gameplay frame, or when the currently opened controller disconnects.
Replacing it with another controller during the same poll still opens the menu.
Reconnection alone never resumes the scene. Use a fresh Start or B press, or
select Resume with A. If the controller disappears during a confirmation, that
confirmation is cancelled and selection returns to Resume.

Attaching with Start, A/B, use/reload, triggers, D-pad, or an exit chord held
cannot manufacture a fresh action. Held gameplay buttons must be released
before acting again, including after a restart reopens the controller. Menu
selection uses separate edge-only controls; the gameplay/replay input record
has not changed. Bounded input diagnostics and replay runs do not acquire the
new automatic no-controller wait.

The pre-existing post-pause policy supplies one neutral gameplay input when
resuming. Release-triggered weapon owners, such as Rocket lock release, retain
their existing response to that neutral sample. This change does not cancel
charges, fire a synthetic input, or alter their save-admission checks.

## Integration and safety

`player_pause_menu` keeps the input-provider call suspended, so simulation,
mission deadlines, AI and game-frame counters do not advance. The existing
native audio pause owner suspends and resumes voices. The frame clock is
rebased on resume, with no accumulated catch-up debt. Restart/exit leave audio
paused until the normal scene close releases its voices.

The session renderer uses the existing pbkit device, solid HUD backend and
procedural glyphs. It draws an opaque screen only when selection, confirmation
or connection state changes. It allocates no world textures, duplicate scene
or per-frame menu buffers. When a pad is missing before the first rendered
frame, the menu initializes the same pbkit device that gameplay later reuses.
World textures are never borrowed by this out-of-frame-sink draw pass.

Quick-save and quick-load enter the same edge-request APIs as existing button
chords. They do not call storage directly, advertise success before the result,
change slot/generation selection, relax compatibility or transient-state guards,
modify autosave behavior, or touch host disk protection. Confirmed restart uses
the existing fresh-section transition sentinel and normal cleanup, rather than
changing live health, teleporting the player, or fabricating a saved state.

## Parent compilation and bounded review

The existing Xbox source list already includes `main.c`, `input.c`, `renderer.c`
and `scene.c`. `scene_session_menu.inc` is included immediately after the HUD
rectangle/text helpers; no Makefile or new object is required. The shared public
header supplies the session enums and function signatures. The source changes
do not add a native saved-state field or change the 48-byte replay record.

Use the sole coordinated stock64 Xbox build to establish compilation/linking.
No PC compatibility build is called for by this Xbox frontend change. Useful
bounded tester checks are visible pause/resume, a cancelled restart, one
confirmed fresh-section restart, and controller removal/reconnection. Save/load
operation remains subject to the existing independent disk-protection approval
and compatibility gates. No campaign route is needed for these controls.
