# Autonomous opposed NPC combat

The ordinary NPC scheduler now acquires visible actors of the opposing
affiliation: hostile0 targets friendly2, and friendly2 targets hostile0.
Neutral1/outcast3 actors are not automatic targets. This matches the existing
turret opposition policy; it is a practical port behavior, not a recovered
original tactical scheduler.

`scene_ai_opposed_acquisition.inc` uses the existing20-unit/120-degree sight
cone,30-frame stagger and30-frame reaction delay. It picks the nearest visible
eligible actor, validates live generation-checked registry/entity owners,
and checks world/mover and clutter cover. Existing targets, queued shots and
authored orders retain priority. A hostile with a visible player keeps the
ordinary player acquisition path. Catatonic and active Waypoints modes veto
acquisition; Motion11 checks the candidate's actual body velocity.

Acquisition installs existing reactive ownership (`combat_scripted=2`), so
ordinary pursuit, aiming, weapon selection, ammunition, reload, damage and
death handle the encounter. It does not synthesize an Attack event or damage.
The stationary aim gate also permits an independent NPC target after player
death. The helper adds164 diagnostic bytes and no per-frame allocation.

## Scope and remaining integration

Reactive-target ordinary saves now use RFNC12 and retain live reactive mode2;
see [NPC-REACTIVE-SAVES.md](NPC-REACTIVE-SAVES.md) for the passing Xbox
continuation. Broad squad tactics, opposition changes during an existing
engagement, and visual/audio behavior remain unverified. The focused
harness covers two complete guard941 records copied to CTF06, transformed to
face each other, with one ordinary Set_Friendliness event making an ally.
No player fire, scripted Attack, Slay, injected damage, images or campaign route.

The placed Fusion registry correction in the same integration batch appends
`shoulder cannon` at index31, preserving all older class indices. Existing
extra-weapon resource/grant handling owns the behavior. The installed12 placed
copies are multiplayer-map assets; this is general placed-item support rather
than an authored single-player blocker. Its separate native collection harness
is prepared, not run as part of the opposed-combat check.


## Stock64MiB Xbox result (2026-10-03)

PASS: `artifacts/xemu/npc-opposed-20261003-134033/report.json`.
Both actors acquired through sight independently (one0-to2, one2-to0), with
zero authored Attack, retaliation acquisition, forced death or player shots.
Four real shots produced four hits. Ally914201 died at health-20/armor0;
hostile914200 survived at health50.8/armor9.2, released the dead target and
submitted no further shots between the live probe and frame180. Player
health stayed100. The endpoint had6094 free pages (23.80MiB).

NXDK build and original-disc restoration passed. The initial harness expected
a nonfatal outcome and mislabeled the setup event-type word as1 rather than30.
It was corrected to check the actual bilateral damage, death, target release
and cessation of shooting from the retained guest evidence. No second native
encounter was run; the report preserves the original validator failure.
Visual death/aim animation and audible output were not inspected.
