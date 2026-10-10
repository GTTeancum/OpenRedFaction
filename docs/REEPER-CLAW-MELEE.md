# Authored Reeper and mutant melee

Status: the original finite melee registry passed 2026-10-10 02:00 Xbox
compilation at source 5c506c1a. Exact delay metadata and shared delayed-impact
consumption were written afterward and await the parent-owned 03:00 batch;
they are uncompiled and runtime-unverified. No fixture, grant, route, forced
event or original asset change was used. See HOURLY-20261010-0200.md for the
earlier build proof; it does not cover these new changes.

## Original evidence

- Read-only installed `tables.vpp/entity.tbl:4528–4529` allows/defaults
  `Reeper Claw`; lines 4589–4590 allow/default `Baby Reeper Claw`.
- `weapons.tbl:1942–1978` gives the adult primary bash damage 40, AI attack
  range 3.8, Fire Wait 2 seconds, and nominal `12mm` supply with maximum 200.
  `weapons.tbl:1982–2018` gives the baby bash damage 10, range 2.5, Fire Wait
  2 seconds, and nominal `12mm` maximum 100. Both omit Clip Size and Clip
  Reload Time, use one projectile/default primary AI damage scale 1, and have
  `melee`, `underwater`, `from_eye` flags (`flags_264=0x620`) plus
  `no_fire_through` (`flags_268=8`). Adult primary impacts are authored at
  0.4/0.8 seconds; baby primary impacts at 0.3/0.6 seconds. Neither has
  alternate impact delays.
- Original RF.exe `00425b9a` bypasses ordinary ammunition availability for
  melee bit `0x20`; `00426000–00426196` schedules the noncontinuous melee
  impacts/action/audio and returns without the ordinary `004257c0` debit.
  The nominal 12mm supply must not become a firearm-ammo requirement.
  Executable SHA-256:
  `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
- Original `0042607d–004260c6` schedules both impact slots independently
  through `004fa360`, which adds the rounded millisecond delay to current
  game time. The second delay is not added to the first deadline. One adult
  attack therefore uses attack-start +400/+800 ms, not +400/+1200 ms; the
  same interpretation applies to the other authored primary delay pairs.
- Original `L10S1.rfl` hostile `reeper` UID 2633 and
  `baby_reeper` UID 2634 explicitly hold their respective primary claws,
  secondary `none`, affiliation 0. Their entity records start at RFL bytes
  1,895,633 and 1,895,834 and occupy 201 and 216 bytes respectively.
  Existing class startup and instance loadout already establish real catalog
  ownership; no inventory or original-input change is required.
- Adult `fire_stand` maps to `reep_attack.mvf` and `Reeper Attack` action
  Foley (`entity.tbl:4552`); baby maps to `brpr_attack.mvf` and
  `Breeper Attack` (`4614`). Both weapon declarations separately author
  `Riot Attack` Launch. Existing action presentation/Foley remains intact;
  adding that weapon Launch sound is deferred.
- Installed `entity.tbl:4747–4749` and `4809–4811` allow/default the exact
  spaced names `Mutant Attack 1` and `Mutant Attack 2` respectively.
  `weapons.tbl:2236–2263` and `2268–2294` give each attack bash damage 40,
  AI attack range 2.5, Fire Wait 1.5 seconds, nominal `12mm` supply,
  no clip/reload pair, and the same `0x620`/`8` flags qualified above.
  Attack 1 authors impact delays 0.15/0.5 seconds; Attack 2 authors 0.15.
  Neither has alternate impact delays.
  Both separately author `Riot Impact Flesh`; that contact sound is not
  added by this admission. Both mutant Launch and Fly Sound fields are empty.
- Original `levels2.vpp/L10S1.rfl` hostile `mutant1` UID 2623 and
  `mutant2` UID 2624 explicitly hold `Mutant Attack 1` and `Mutant Attack 2`,
  secondary `none`, affiliation 0. Their records start at RFL bytes
  1,894,998 and 1,895,207 and occupy 209 and 213 bytes respectively.
  These original instance weapons and class defaults agree; no alias,
  fallback, loadout repair or inventory grant is needed.
- Mutant primary `fire_stand` clips are `mtnt_move_n_attack.mvf` and
  `mnt2_attack.mvf`. Their action Foley is respectively `mutant swing`
  (`entity.tbl:4772`, `foley.tbl:1338`, `Mutant_swing.wav`,
  `bluebeard.bty:14324`) and `mutant2 attack` (`entity.tbl:4834`,
  `foley.tbl:1346`, `Mutant2_attack.wav`, `bluebeard.bty:14180`).
  Existing accepted action animation and registered positional action Foley
  are reused without new presentation state.

## Bounded implementation

The primary parser now recognizes the authored `melee` flag locally as a
reason to accept a wholly absent clip/reload pair. A partial pair still fails,
and duplicate, required timing/damage, explicit clip size/reload value and
other metadata guards remain unchanged. No definition layout, legacy hash
prefix, inventory format or checkpoint format changes.

`scene_ai_reeper_claw.inc` retains its original helper/hook names but now owns
a shared finite registry of exactly `Reeper Claw`, `Baby Reeper Claw`,
`Mutant Attack 1` and `Mutant Attack 2`. Startup resolves their primary IDs
against the existing supply and reset catalogs, verifies matching IDs and
the installed `0x620`/`8` flags, and retains four authored primary definitions.
It requires zero magazine/reload, a single bash projectile, no burst or
piercing, primary AI damage scale 1, and the exact installed damage, fire
interval and AI range listed above.

Startup also calls the separate `rf_weapon_impact_delays_load` for each exact
name and validates the full primary count/seconds pair with no alternate
delays. It stores these bounded independent attack-start offsets:

- `Reeper Claw`: count 2, +400/+800 ms.
- `Baby Reeper Claw`: count 2, +300/+600 ms.
- `Mutant Attack 1`: count 2, +150/+500 ms.
- `Mutant Attack 2`: count 1, +150 ms; unused second slot is zero.

Each stored millisecond offset is converted with
`int(seconds * 1000 + .5)` and checked against its exact authored integer.
IDs, definitions, counts and offsets are staged in one private candidate;
publication occurs only after all four validate. Failure leaves the existing
registry untouched. The read-only
`campaign_enemy_reeper_claw_impacts(weapon)` helper returns the admitted
`primary_count`/`primary_ms[2]` metadata only for a ready exact weapon ID,
otherwise null. It grants no ownership and performs no attack authorization.
There is no per-shot asset read or dynamically allocated metadata owner.

Both ordinary combat and opposed-NPC acquisition try this selector after the
existing weapon selectors. Admission requires the currently held primary to
be genuinely owned by that same living, allocated, identity-qualified actor
in both registries. Dead, hidden, removed, stale, unowned or unrelated
primaries cannot acquire this melee behavior. The result is `melee=1`,
`ammo=NULL`, `penetrates_world=0`, `reserve_fed=0`, and `slot=UINT32_MAX`.

All four attacks retain the existing pursuit/range/aim, sight, cadence and
accepted fire presentation. The shared delayed owner consumes their admitted
offsets and retains intended-body contact, world/rubble cover and the shared
damage/death path. The Reeper claws' authored two-second
Fire Wait enters the existing 120-frame cadence; both mutant attacks use
their authored 1.5-second Fire Wait (90 frames) and 2.5-unit AI range. Normal
primary damage requests per impact are 40 for the adult Reeper and each mutant attack,
and 10 for the baby Reeper, before target-specific scaling. An empty nominal
12mm reserve cannot suppress a strike; neither loaded rounds nor reserve is
debited and no magazine reload is requested. Existing supply metadata is untouched.

The registry adds no fallback candidate, player weapon slot, inventory grant,
actor class, timer or persistent gameplay state. Delayed scheduling uses the
existing bounded transient shared owner, documented in
CREATURE-DELAYED-IMPACTS.md; inventory and save layouts remain unchanged. Tankbot
Smash, other creatures, bosses and the separate Tankbot primary/missile
adapters are outside this admission. Existing accepted fire animation and
action Foley run without changes; no new animation/audio owner is added.

## First-playable limits and verification

Authored primary delay metadata is reconstructed and the shared delayed
owner replaces the earlier immediate single contact for these four weapons.
The delay slots remain independent deadlines from one accepted swing, with
contact and full primary damage evaluated separately at each due time. Cadence
and presentation advance once per accepted swing. A missed first impact does
not consume the second; target death or source lifecycle cancellation does.
Normal/opposed combat requires on-foot actor targets, full-3D authored reach
and the existing horizontal firing cone for these ground creatures. Existing
scripted point/once orders remain presentation-only. This remains
point-target melee, not a swept weapon volume or animation-marker
implementation. Physical player/NPC shield checks currently reside only in
the nonmelee branch, so this is not shield-complete and makes no retail-parity
claim. Actor contact damage and vehicle melee contact remain excluded.

Parent owns the scheduled Xbox compilation and any authorized bounded
stock-64-MiB verification. Original L10S1 neutral spawn starts about 68.51
units from the adult and 66.28 from the baby, beyond the existing 20-unit
unalerted acquisition range. Neutral startup can establish table/level
admission but cannot establish ordinary pursuit, contact, cadence, damage,
action Foley or save/load runtime. No route or synthetic event is introduced
to overcome that original placement.
