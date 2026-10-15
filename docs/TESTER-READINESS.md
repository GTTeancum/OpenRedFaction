# First-playable tester scope

Status: October 15, 2026, after the 16:00 UTC build.

Commit `0d5149c14d2a0ffc33b6c4859f8313ec7d7c1985` passed native stock64
compile/link/package and limited XEMU startup reaching 12 simulation and
10 presentation frames. Shutdown and neutral-input postchecks passed.
See [the exact receipt](HOURLY-20261015-1600.md).

The frontend, menu motion caches, medic persona responses, automatic
Humming/Timeout and ordinary Use-facing/default-patrol pause are integrated
and compiled. Main, Difficulty and New Game were traversed in the startup
check. Not every menu action or NPC behavior was functionally exercised.
Owned speech gestures and their RFNC42 save integration are also compiled in
this passing build; functional coverage remains tester-owned.

Actual hardware, broader gameplay, visual/audio correctness, performance and
save-route coverage remain unverified and tester-owned. Historical estimates
are not readiness or test-coverage scores.

## Delivery steps

1. Preserve the passing build artifacts and exact evidence manifest.
2. Preserve and publish reviewed source with an exact delivery manifest.
3. Hand off private inputs and the NXDK build recipe for Codex local builds.
4. Verify RXDK Neighborhood tools, console identity, deployment destination,
   full-package D-drive launch mapping and effective 64-MiB RAM locally.
5. Record actual-hardware observations separately from emulator startup.

Verified Neighborhood deployment commands are not yet documented. A source-only
checkout also needs the three private original-derived build/data templates.
Original assets/templates must not be published with source. The protected
emulator HDD is outside the handoff and must not be accessed. Persistence tests
require an approved destination and protection of existing console data.

## Deferred tester refinement

- Rocket steering, lock feel and broad weapon behavior. Ordinary composed saves
  support lock/guided-flight continuation; specific unsupported identities and
  direct live-state replacement still refuse. See [the exact save scope](PLAYER-ROCKET-LOCK-SAVE.md).
- Shared-ammunition auto-reload admission is fixed in source; weapon behavior
  still needs tester exercise.
- Controller feel, demanding-scene performance, event parity and full routes.

Vehicles and GeoMod retain accepted first-playable scope. Reopen only for a
concrete ordinary-play blocker or explicit direction.

See [launch instructions](XBOX-TESTER-LAUNCH.md),
[controller mapping](XBOX-CONTROLLER-BUTTON-MAPPING.md),
[authored menus](AUTHORED-XBOX-MENUS.md), and
[weapon inventory](TESTER-COMBAT-SWEEP.md).

## Historical October 11 source sweep

- **Getting into the game:** the ordinary diagnostic disc did not turn on live
  gameplay. The new opt-in `tools/build-xbox.sh --tester` package starts authored
  L1S1 with live controls, pacing, audio and level following. This closes the
  source-level entry gap; compilation and a real boot still have to succeed.
- **Weapons and combat:** all 17 player equipment slots have connected firing,
  mode, ammunition and damage consumers. The major missing Rocket hold-lock /
  release-to-fire alternate is now source-integrated. Broad weapon exercise is
  still pending; a neutral boot cannot prove lock-on, damage or combat feel.
- **Player loop:** movement, look, jump/crouch, ladders/swimming, Use arbitration,
  32 pickup classes, finite ammo/vitals, medic healing, HUD, pause and death/
  restart are source-connected. No foundational new implementation gap emerged.
- **Mission systems:** objectives, authored triggers/events, dialogue/cutscenes,
  countdown/failure, section handoffs and final defuse/completion have consumers.
  Natural full-route reachability has not been demonstrated. Campaign traversal
  remains outside this development batch.
- **Vehicles and GeoMod:** retain the accepted first-playable scope closures.
  Reopen only if an actual blocker prevents ordinary play.
