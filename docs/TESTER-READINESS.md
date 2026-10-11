# First-playable tester scope

October 11, 2026. The target is useful gameplay for testers, not retail parity.
Source coverage alone does not establish that a tester can play successfully.
The next completion marker is an actual stock-64-MiB boot of the interactive
package. The new launch and Rocket alternate changes have not yet been compiled
or run. Historical rough estimates remain 90% overall / 95% weapon-handling
implementation, with low confidence; these are not runtime readiness scores.

## Broad system sweep

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

## Actual blockers and next results

1. Compile and package the combined interactive launch / Rocket alternate code
   in the next single parent-owned hourly Xbox batch.
2. Boot the tester image with the approved baseline disk in temporary snapshot
   mode; establish real live scene/input-loop progress and clean shutdown.
3. Fix a material failure if that run exposes one. Do not replace this outcome
   with more source-proof paperwork or repeated tiny scenarios.
4. Protected saved-game continuation remains separately blocked by compressed
   backup approval, successful protection and capacity. Fresh snapshot startup
   is specifically approved and does not depend on that saved-backup operation.

No runnable tester build or successful new gameplay result is claimed yet.
Private original assets must not be published with source downloads.

## Leave to tester feedback / later refinement

- Exact Rocket steering, target/HUD policy, lock beeps and predictive lead.
  Saving/loading during active lock acquisition or guided flights is explicitly
  refused until those transient owners end; existing saves are not silently
  changed into ballistic flights.
- Shared-12mm surplus can prevent automatic reload for the lower-capacity
  handguns; manual reload remains available.
- Ladder edge feel, optional persona/monitor interactions, unbound holster /
  message log / fine aim, menus/remapping and presentation polish.
- Isolated event parity and completion/credits presentation, broader action
  coverage, demanding-scene performance and physical-controller feedback.

Use [tester launch instructions](XBOX-TESTER-LAUNCH.md),
[weapon inventory](TESTER-COMBAT-SWEEP.md) and
[Rocket controls and limits](PLAYER-ROCKET-LOCK-FIRST-PASS.md).
