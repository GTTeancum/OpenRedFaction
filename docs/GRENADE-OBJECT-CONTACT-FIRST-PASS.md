# Grenade object contacts

Player and NPC grenades now distinguish object contacts from world surfaces.
Either grenade mode is consumed immediately on a player, NPC, destructible prop
or vehicle contact. Ordinary object hits apply direct damage, request one radial
blast, and publish the existing impact effect. They do not bounce or remain
alive for another fuse explosion. World bounce and alternate next-step
detonation retain their existing behavior.

An active NPC nano-shield consumes the grenade before ordinary damage, blast,
impact or terrain handling. A normal grenade drains zero shield armor; instance
bit0x10 (alternate) permits its damage amount to drain armor. The breaking hit
still cannot damage health. Remote-charge attachment remains separate.

## Evidence and implementation

Original RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Retained original dispatcher evidence covers `0x4c4b50` and object handler
`0x4c59f0`: direct damage at `0x4c6132`, radial request at `0x4c62f5`, then impact
and consumed return. The shield branch `0x4c5a90..0x4c5b5d` adjusts the grenade
amount and returns before those ordinary effects. The object branch does not
request a terrain cut; timed/world explosions keep their separate cut path.

`rf_grenade_flight_step_objects` accepts a pure scene classifier, stops at its
first accepted object contact and returns an explicit event. Scene adapters
then apply effects. This keeps scene tags out of the reusable integrator and
preserves its legacy wrapper for callers that need world-only behavior.
The existing flight state/save layout is unchanged.

Direct requests use the loaded grenade damage kind and retained thrower source.
Radial requests use kind3 and the full base damage. Actor hit-location scaling
is currently1; exact skeletal hit-location scaling remains open. A0.01-unit
normal offset for radial cover queries is an existing port policy.

## Focused Xbox evidence

- NXDK build passes.
- `python tools/check_xbox_grenade_contacts.py` executes the compiled NXDK
  integrator under x86 emulation with synthetic contact callbacks. Both modes
  consume once, ordinary world contact bounces, alternate world contact expires
  next step, a hit before a same-tick fuse deadline produces only one event,
  and an invalid classifier preserves state/output. Report:
  `artifacts/xbox-grenade-contacts.json`.
- `python tools/xemu_nano_shield.py --grenades` passes on stock64MiB XEMU.
  Authored Capek8359 is isolated by hiding other actors. The fixture stages
  release positions, three reserve rounds and one reserve debit per launch;
  ordinary gravity, collision, contact damage and death code run afterward.
  It does not test throw-button or animation-release timing.
- Armor100 stays100 on a normal grenade; an alternate grenade reduces it to0
  while health stays100. A third normal grenade applies150 direct damage,
  reducing health to-50, with exactly one radial blast and no object-contact
  terrain cut. All three flights retire. The110-frame run retains4878 free
  pages (19.05MiB) and restores the disc flags. Report:
  `artifacts/xemu/nano-shield-grenades-20260930-105156/report.json`.

NPC-source throws, player/prop/vehicle victims and post-save contacts are wired
but not independently checked in XEMU here. Liquid/support behavior, precise
body hit locations, detached-fragment object handling and grenade-specific
effects remain open. No PC game, campaign traversal, images or host input were
used; visual/audio output is unverified.
