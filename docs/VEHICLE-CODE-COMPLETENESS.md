# Vehicle implementation checklist

This is a code-path checklist, not a test-coverage or retail-parity report.
Vehicle first-playable code scope is closed by the user's2026-10-08 direction:
playable matters; retail maneuvers, rare passenger cases and polish do not gate
moving to objectives/level progression. The13:00 UTC parent NXDK build passed
checkpoint542cb5e. The blind-pursuit, ownership handoff, independent rider carry and APC combat
batch passed the parent14:00 NXDK build at78c4264 after one stale unused-helper
removal. XBE/ISO outputs were emitted.
Gameplay for these new paths remains unverified; no campaign replay was run.

## Newly written in this source checkpoint

- [x] Authored Fighter shooter resolution for Attack38, including original
  L12S1 event9698/shooter9700/Jeep-driver target7646.
- [x] Independent physical pursuit and muzzle aiming, finite primary ammo,
  authored AI damage/range/cadence and normal cover/interception/damage.
- [x] OFF/replacement/death target release, player-control priority, emitted
  bullet continuation and RFAK ordinary-save UID rebinding.
- [x] Independent submarine Goto/Goto_Player, covering L5S4 event3966/4455
  and submarine2381 alongside separately selected3955.
- [x] Distinct submerged full-hull motion and staged RFSV2 submarine restore.
- [x] Per-instance weapon defaults/overrides/none, initial and switched owners,
  new-fire gates, HUD omission and source-derived restore selection.
- [x] Concrete none/none Fighter8955 cannot fire either gun; dormant900/20
  ammunition is retained as original464010 requires (post12:00 correction).

- [x] Distinct masako_fighter4717/Fighter02/Goto17973, detached lift ownership
  and RFSV2 candidate restoration, integrated from the parallel source slice.
- [x] Typed Make_Invulnerable4697/sub3977 callback, passive RFVA2 preservation
  and selected-host RFPV3 flag continuation, integrated from its source slice.

- [x] Original authored AI enabled/mode/FOV admission, default combat acquisition,
  Goto_Player handback and independent minigun/primary-torpedo/secondary-rocket
  dispatch with source-correct damage and RFAI live-round continuation.
- [x] Original409280 queued-shot consumer and406330/406390 secondary gates.
- [x] Headlamp_State53, actual tagged coronas and RFPV4/RFVA continuation.
- [x] Real generated Masako human phase, vehicle-death handoff, completed-death
  trigger/music semantics and RFMB1 continuation.

## Existing authored movement/control consumers found by source inventory

- L1S3 Goto9517/9639 → independent APC26
- L5S3 Goto_Player4647 → selected submarine3963
- L5S4 Goto3958/Goto_Player4387 → selected submarine3955
- L12S1 Follow_Waypoints9692/Set_AI_Mode9710 → selected Jeep7629
- L13S3 Follow_Waypoints8644 → selected Fighter8955
- L18S2 Follow_Waypoints10626 → selected Fighter10066
- L20S2 four original Goto_Player orders → independent regular Fighters
- L20S2 fixed Goto18457 → two independent regular Fighters

The read-only inventory covered all RFLs in the three original SP archives;
its compact metadata is packaged as`checkpoint/authored-command-gaps.json`.
Parsed event type is authoritative, regardless of editor display name.

## Final written first-playable batch

- [x] Parked APC/sub/Fighter motion loan for authored Goto/Goto_Player/Attack,
  RFSW5/RFSV authority join, and settled empty independent-host promotion.
  New player-owned Goto commands retain real movement and RFVR continuation.
- [x] Independent-host grounded player/NPC roof carry using the actual accepted
  chassis interval, collision preflight and joined support continuation.
- [x] Authored APC26/9627 default AI uses the actual APCMinigun definition,
  primary-only finite supply and selected/player scheduler synchronization;
  no autonomous mortar is inferred from dormant secondary ammunition.
- [x] Original class BlindPursuitTime expiry and RFAI2 sight-age continuation.

The read-only source inventory found45 authored vehicles across21 SP levels,
six authored classes and62 vehicle-targeting events of16 types. Every such
class and event type now has a first-playable code consumer. AI-enabled
APC/sub/Fighter/Masako owners use their real profile and ammunition; source-disabled
Jeep/Driller instances do not acquire an invented autonomous combat mode.

## Deferred refinement, not an open first-playable vehicle milestone

Retail tactical maneuvers and perception timing, non-default autonomous modes,
explicit enemy-list extensions, generalized independent passenger bindings,
and extra cross-section independent-owner persistence remain later refinement.
Additional presentation or performance work is reopened only for an observed
playability blocker. Written code and a successful compile are not proof of
retail equivalence or runtime correctness.

Source rationale and exact boundaries are in VEHICLE-SCRIPTED-ATTACK.md,
VEHICLE-SECONDARY-SUBMARINE.md, VEHICLE-INSTANCE-WEAPONS.md,
VEHICLE-MASAKO-DISPATCH.md, VEHICLE-INVULNERABILITY.md,
VEHICLE-AI-REFERENCE.md, VEHICLE-HEADLAMPS.md,
VEHICLE-MASAKO-LIFECYCLE.md, VEHICLE-AI-BLIND-PURSUIT.md,
VEHICLE-AUTHORITY-HANDOFF.md and VEHICLE-INDEPENDENT-RIDERS.md.
