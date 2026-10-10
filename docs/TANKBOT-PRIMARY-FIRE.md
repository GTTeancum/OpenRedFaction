# Authored Tankbot primary fire

Status: 2026-10-10 02:00 Xbox compilation passed at source 5c506c1a.
Build-only evidence: changed attack/audio/parser execution and save restoration
remain runtime-unverified. No fixture, grant, route, forced event or original
asset change was used. See HOURLY-20261010-0200.md for exact build proof.

## Original evidence

- Read-only installed `tables.vpp/weapons.tbl:1771–1830` defines the exact
  `Tankbot Chaingun` primary as `continuous_fire` / `from_eye`, bullet damage,
  `turret_ammo`, capacity 170, and no Clip Size / Clip Reload Time pair.
- The same declaration authors AI range 50 (line 1782), Fire Wait 1 second
  (1785), AI spread 1.5 degrees (1788), a 16-shot burst at 0.1 seconds per shot
  (1790–1791), and damage 100 with primary AI scale 0.2 (1794–1796). The current
  shared damage consumer therefore requests 20 before target-specific scaling.
- `entity.tbl:4028,4046–4049` names Tankbot and allows default Tankbot Chaingun,
  Tankbot Missile and Tankbot Smash. Its `fire_stand` animation already maps to
  `tbot_fire_gun.mvf` with blank action Foley at line 4071.
- Original `levels2.vpp/L7S4.rfl`, entity UID 10696, has class/script Tankbot,
  explicit primary Tankbot Chaingun and explicit secondary Tankbot Missile.
  The record begins at RFL byte 2,116,931 and occupies 223 bytes.
- Existing `rf_entity_startup_weapon_bindings_sp` and `scene_npc_loadout_apply`
  already establish ownership and fill the gun's real mapped reserve to 170.
  Missile uses the separate `15cm rocket` reserve with capacity 36. Neither
  loadout nor original inputs need changes.
- Original RF.exe `004257c0` / `004c86e0` distinguish loaded rounds for positive
  magazines from mapped reserve for clipless weapons; reconstructed
  `rf_weapon_consume_shot` already implements that debit choice.

## Bounded implementation

`scene_ai_tankbot_primary.inc` resolves the exact catalog name at existing table
startup, parses its authored primary metadata, and admits only a primary with
finite mapped ammo, zero magazine / reload, one projectile and bullet damage.
It retains one definition and its actual catalog ID; it adds no player slot,
resource grant, generic unsupported-weapon fallback or per-shot asset read.

An explicit `reserve_fed` selection field distinguishes this admission from
ordinary magazines. Every legacy/extra producer initializes the field to zero.
Both ordinary combat and the existing opposed-NPC acquisition gate try the
exact Tankbot selector after their current selectors. Existing acquisition,
affiliation, aim, visibility and targeting policy are unchanged.

Before the existing shot deadline / aim gates, Tankbot readiness requires
actual ownership, zero loaded rounds and nonnegative mapped reserve. It clears
any old magazine-reload state, never transfers ammunition, and emits the
existing exhaustion event when reserve reaches zero. The existing explicit
Attack veto and ordinary fallback order remain unchanged; Tankbot is not added
to the generic fallback candidates.

Only an accepted reserve-fed shot calls `rf_weapon_consume_shot`, once, at the
existing debit point after cadence acceptance and before fire presentation and
contact resolution. Conventional magazine debit and the separately owned
rocket / grenade / Tankbot Missile paths remain unchanged. Misses and cover
hits consume a round; blocked prefire attempts do not.

The shared port cadence is deliberately unchanged: 16 accepted shots are six
frames apart; their 90-frame span already exceeds Fire Wait's 60 frames, so the
existing scheduler applies its one-frame minimum after shot 16. This does not
claim a retail scheduler reconstruction or a new one-second post-burst pause.

Shots use existing spread, full-ray cover arbitration, physical shields,
clutter, vehicle, actor damage and animation consumers. Authored piercing
metadata is retained. The subsequent source-only adapter documented in
[Tankbot actor penetration](TANKBOT-ACTOR-PENETRATION.md) now consumes authored
body power without enabling world penetration. Scripted secondary Missile
and Smash handling are untouched.

Accepted identity-qualified shots also reach the optional bounded burst-onset
audio helper, with the existing pre-cadence count / queued-single policy.
Startup residency and teardown hooks are integrated; audio failure cannot
reject a shot. See [Tankbot burst audio](TANKBOT-BURST-AUDIO.md) for its exact
authored declaration, shared-bank attenuation limitation and unverified status.

## Remaining verification

Parent owns the scheduled Xbox compilation and any bounded stock-64-MiB
validation. Natural primary fire, finite exhaustion and save/load continuity
remain unverified. Existing inventory and NPC checkpoint layouts are unchanged;
this slice adds no save fields, routes or synthetic gameplay fixtures.

Original neutral L7S4 startup is not a primary-fire runtime proof: the player
starts about 48.69 units away behind the Tankbot, while existing unalerted
acquisition uses a 20-unit forward-cone gate. All 20 directly linked Shoot_Once
events request secondary mode 1. No route traversal or synthesized primary
event is authorized to overcome those authored conditions.
