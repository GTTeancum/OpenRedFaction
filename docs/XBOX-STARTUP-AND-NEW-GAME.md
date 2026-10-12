# Xbox tester startup and New Game

Source implementation, October 12, 2026. This slice is uncompiled and unrun;
compilation and any bounded Xbox check belong to the parent-owned hourly batch.
No campaign route, emulator session, saved-game load, HDD operation, test fixture
or generated image was used for this implementation.

## Tester controls

The stock64 tester package now opens a controller-driven main menu. Press a
fresh A or Start to choose New Game. A loading screen appears while the ordinary
campaign owners open Live Mines, L1S1, with its authored/default starting state.

Start still opens the existing pause menu. Confirming End Session returns to the
main menu, where New Game starts another genuinely fresh session without a
reboot. Back+Start retains its immediate end-session shortcut and now reaches
the same menu in the tester package. Held buttons and triggers must be released
before they can act in the next menu or gameplay session. Connecting a pad with
A or Start held never starts the game by itself.

Ending a session discards unsaved progress. New Game clears mission/section
history, inventory carry, player form/disguise carry, terminal mission state,
queued save/load requests and pending section transfers. It never loads a save
implicitly. Existing guarded manual Quick Save/Load and ordinary section
restart remain available during play. Saved files, slot/generation selection,
compatibility rules and independent disk-protection requirements are unchanged.

This is a first-playable tester front end, not the original retail menu. It has
one New Game action; settings and a save browser are outside this slice.

## Ownership and failure behavior

The existing scene transaction retires animation, AI, vehicles, audio voices and
scene-local resources. A shared resident-close path then drains renderer work,
drops cached world/model/texture borrowers, and frees the resident world,
lightmaps, materials, collision, movers and diagnostic level records. The
process-owned pbkit device and framebuffer remain available for the menu.

Only after those owners are gone does a full-session reset release the retained
player physics body and in-memory checkpoint-export buffers, clear campaign
carry/history and reset session counters. This reset is separate from section
handoff/restart and manual load. Normal per-scene constructors continue to own
combat/RNG/resource initialization. The menu uses the existing solid HUD and
procedural glyphs, with no new image assets, duplicate world or per-frame buffer.

An actual load, runtime or cleanup failure is shown as an error with reboot
required. It does not become a successful End Session or automatically attempt
another load over possibly incomplete ownership. The previously silent
retirement exits for glare, medic children and clutter now latch a failure that
prevents re-entry. End Session cancels a waiting section handoff before ordinary
retirement, rather than unexpectedly proceeding to the next section.

The startup menu is scoped to the validated tester-profile package. Existing
bounded/replay/diagnostic launches keep their direct launch and terminal
behavior, including optical startup snapshot fixtures. New Game explicitly
skips those implicit snapshot loaders; later deliberate manual loading still
uses the unchanged admission path. Existing stock64 limits are unchanged.

## Integration

Changed source files are Xbox main.c and renderer.c/.h, the shared scene.c and
scene_preview.h, and scene_session_menu.inc. Existing build source lists already
cover them. No new object, save ABI, replay record or external dependency is
introduced. The existing input attach/rearm policy is reused without changes.

Useful proportional follow-up in the normal parent batch: compile stock64, then
verify startup, one End Session return, and one fresh New Game re-entry without
campaign traversal. Frontend state/start/end/error counters and available pages
after resident release are exposed in rf_xbox_frontend_diagnostic. These are
observability fields, not a claim of measured memory stability or runtime PASS.
