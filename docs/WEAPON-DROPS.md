# Live enemy weapon drops

First-pass gameplay integration (2026-09-15). Supported armed NPC deaths now
leave their held handgun, assault rifle, riot stick or shotgun when a floor ray
finds support within four units. The drop grants the weapon and one authored
magazine through the existing bounded pickup/inventory path. Collection requires
a living player within two units and the normal bullet-obstruction check.
Full inventories leave the pickup available. The corpse no longer renders the
same held weapon after emission. Available drops use the existing world models.

This adapter is deliberately practical, not an exact use of reconstructed
`rf_weapon_drop_sp` (original42ae10). That reconstruction includes exclusion
flags, ammo availability, randomized quantity, remote-charge remapping and
surface alignment. Live quantity, floor-ray placement and identity orientation
are first-pass policy. NPC ammunition depletion, authored no-drop restrictions,
unsupported weapons, physical tumbling, exact orientation, moving-floor
attachment and configured non-weapon death items remain open. Missing floor
support currently skips emission; no item is spawned through arbitrary space.

Each actor persistence slot owns a 24-byte record: state, weapon, quantity and
position. All2048 slots cost49152bytes (48KiB), allocated statically, with no
runtime handles or heap allocation. State0 is never emitted,1 available and2
collected. Emission cannot replace either an available or collected drop.
Retiring/reconstructing the actor preserves the record; a new campaign clears
it. This is section handoff state, not an on-disk save format.

## Verification

- `campaign_pickup_persistence`: all authored actor/pickup keys still fit; drop
  records survive retirement and case-folded registration, cannot reappear after
  collection, isolate same UIDs in different sections, and reject invalid input.
- `python tools/replay_weapon_drops.py`: staged at the authored L2S3 shotgun,
  kill rifle guard2114, collect42rounds, cycle to rifle and fire a three-round
  burst;39rounds remain. No inventory or health injection.360frames.
- Native `artifacts/xemu/render-20260915-003510`: PASS, all31 selected comparisons,
  including all8 drop words.4400free pages (17.1875MiB) on the stock64MiB target.
- `python tools/replay_weapon_drops.py --returning`:260frames; kill and collect,
  dispatch authored exits5151/5150 at frames60/180, then return to the same pickup
  area, select and fire the rifle.39rounds remain, guard retirement survives,
  and no new drop is emitted or collected on return.
- Native revisit attempt `render-20260915-003722` reached frame181 but exhausted
  its180-second limit during return loading; it is not counted as a pass.
- Native revisit retry `render-20260915-004109`: PASS260frames/all31 selected
  comparisons with a360-second allowance; 4464free pages
  (17.4375MiB). Disc files restored and owned XEMU closed.
- Existing staged shotgun firing fixture still passes.
- `replay_area3_shaft.py --landing`:10950frames PASS on PC, identical landing
  position(98.489456,3.381521,65.357544) and5health. Six drops emitted, five
  collected,81rounds granted; rifle retained with42rounds. The replay now cycles
  twice past a newly acquired Riot Stick to select the shotgun. This changed
  full route has not yet been rerun natively.

These are staged encounters and dispatched exits, not proof of uninterrupted
campaign completion. Available-drop preservation is covered at the state-owner
level; live collection of an uncollected drop after a revisit and representative
placement visuals remain to be exercised.

## Combat trace

`RF_REPLAY_TRACE` prints the last32 `COMBAT_EVENT` records in chronological
order: local section frame, type (0 player hits NPC,1 NPC hits player), actor
handle, damage-pipeline amount and remaining target health. Amount is not
necessarily final health loss because armor can absorb damage. The same bounded
journal is exported for native memory inspection; no RNG/gameplay mutation.

The pre-drop shaft replay killed lower guard2067 at global11015; upper guard1773
then killed the5-health player at11058. This ruled out damage from a dead guard
and exposed the need for better player supplies/encounter traversal. Reaching
the main shaft and surviving the remaining guards is still open.
