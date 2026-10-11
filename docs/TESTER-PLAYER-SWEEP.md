# Player and interaction tester-readiness sweep

2026-10-11. Read-only source sweep of the parent-supplied `babf35f1` baseline.
No implementation patch is proposed: no newly grounded foundational player
blocker emerged. Existing secondary defects and retail polish are deliberately
deferred. The active repository and TO-DO.MD were not modified.

## Connected ordinary gameplay

- Native controller movement/look, jump, crouch, Use, primary/alternate fire,
  reload and two-way weapon cycling are sampled in `src/platform/xbox/input.c`.
  Save/load chords and Back+Start exit are separate; Start pause freezes the
  input provider, frame clocks and ordinary audio in `main.c:player_poll_paced`.
- `scene.c:actor_player_stance`, `campaign_jump_update`, `campaign_climb_update`,
  `campaign_swim_update` and `actor_tick` connect player requests to stance,
  look-directed ladders, swimming, run/fall collision and support ownership.
  Live impact, liquid and outside damage run after body publication. This is
  substantially assembled gameplay, not a collection of unused core helpers.
- `scene_use_edge.inc` and `scene_use_dispatch.inc` arbitrate one ordinary Use
  candidate before camera/gameplay handling. Occupancy, medic acceptance and
  claimed presses feed `campaign_trigger_contacts`; ordinary no-dwell Use
  triggers are edge-bound while accepted delayed airlock work continues.
- `scene_pickup_class_names.inc` enumerates 32 supported world-item classes.
  `campaign_combat_input_tick` calls weapon-drop, configured death-item and
  world-pickup collection for an on-foot player. Pickup proximity, cover,
  enabled state, finite inventory/vital grants and retirement are connected.
  Full health/ammo leaves ordinary matching items available for later use.
- Medic Use is connected to real actor/tag selection and finite delayed healing
  in `scene_medic_action.inc`, scheduled by the NPC tick; it debits the medic's
  reserve and refuses dead/full-health players. No new healing implementation
  is necessary for first tester exercise.
- Native HUD shows health/armor/ammunition, pickup and save/load outcomes,
  authored subtitles with controller tokens, death and mission-failure prompts.
  `campaign_life_input` owns death recovery versus fresh-section restart;
  `main.c` performs the fresh-scene transition. Unlimited native input mode is
  supported (`limit == 0`), so the replay cap is not an ordinary session cap.

## Remaining scope, without promoting polish into blockers

- NPC Use kind9 is selected and consumes input without a persona-response
  handler. Kinds5/10 monitor/control handlers remain bounded deferrals. Existing
  docs state these limitations; this sweep found no new authored required
  progression owner proving either is a first-playable blocker.
- Holster, message log and fine aim still have no first-playable input bindings;
  authored training text explicitly displays UNBOUND. They are not presented as
  implemented. Ordinary supported weapon zoom remains on alternate fire.
- Ladder top-edge smoothing and general physical-controller feel remain tester
  feedback work. Controller-loss autopause was considered and intentionally not
  pursued as a secondary edge case.
- No generic hover prompt, menus, remapping, speculative NPC talk or save guard
  relaxation is introduced by this sweep.

## What still prevents a broad functionality claim

This pass read source only. No compile, test, fixture, XEMU session, controller
input, image, asset extraction, saved-HDD access, statistics/hash sweep or Git
write was performed. Existing historical native evidence must not be relabeled
as coverage of every current integrated action. Parent-owned scheduled Xbox
validation and ordinary tester use still need to establish the combined feel
and stability of these wired capabilities on stock 64 MiB.

Protected saved loading is separately blocked on the pending backup approval
and existing admission requirements. That blocker does not imply fresh ordinary
movement, interaction, pickup/healing or death-restart exercise is forbidden or
unimplemented. No new save framework or failure-guard change is recommended.

Suggested parent ledger: broad player-loop implementation is complete within
the current first-playable scope (100% of that bounded milestone). Runtime
confidence remains separate: this source-only sweep supplies no new native
action result. Prioritize concrete missing progression/combat work found by
the other sweeps rather than isolated player polish. Overall implementation
estimate remains approximately 90% (historical, low confidence); this negative sweep does not increase it.
