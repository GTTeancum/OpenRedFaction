# Recovered vehicle AI dispatch

Source: original RF.exe, SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Read-only PE disassembly on 2026-10-08; no original executable execution.
New code awaits the parent-coordinated13:00 UTC build; runtime is unverified.

## Implemented shared boundaries

- `rf_level_entity_ai_read`: full-span v180 record validation, exact1 authored
  vehicle AI switch (`46423b/464246`, then `46498d..46499d`), two raw AI mode
  bytes (`4643ad/4643bf`) and signed FOV degrees (`464319`). This preserves
  existing spawn ABI. `4270a0` requires either actor810 bit10000 or class724
  sentient bit10; affiliation alone cannot enable acquisition.
- `rf_weapon_scheduled_tick`: `409280..40933e`, primary then secondary, at most
  one request per channel per tick, signed-positive raw counts, wrapped due
  timers, bypass1, and descriptor448 seconds read after the firing call.
  Counts decrement regardless of whether firing produced a projectile. Adapter
  period errors consume the attempt and disable its clock, preventing replay.
- `rf_weapon_ai_secondary_admit`: `406330..406543`, original global/fire-enable,
  Hendrix/animation, acquired-target, deadline, visibility/override, approximate
  range, blast clearance, body-forward alignment and LOS ordering. Geometry
  uses `4faf30/4fa7a0`: max+.375*middle+.1875*minimum absolute delta. Alignment
  rejects below the original double .95. Flag1000 skips visibility/occlusion
  refusal, not range or alignment. Optional humanoid speech timing after406543
  is outside this boundary; vehicle classes do not carry humanoid bit20000.

## Actual initiation chain

`4babb0` Goto_Player ON sets action10/state12, resets secondary motion and
applies stance. `404dc0` action10 checks the live player (using their occupied
host when appropriate), every500ms. For opposed actors it requires FOV
`4020c0` and clear LOS `4991c0`; only then does it clear navigation and return
through `407ee0` to the authored default action. Friendly approach returns
inside distance squared4. No fire request occurs in Goto_Player itself.

Default action2 uses `403d10` acquisition admission, `403a80`, then `401de0`
target selection. Selection rejects self, dying/removed/hidden/disabled actors,
requires opposition through the593b70 relation table, a range limit, FOV and
LOS, and chooses the closest accepted target. `409050` assigns it; `408ac0`
changes to action3 only when armed. Action3 `405af0` dispatches Fighter class
bit800 through `406cc0`; its tail calls primary401580 then secondary406330.

Original relation table593b70 rows are `0,0,1,1`; `0,0,0,0`; `1,0,0,1`;
`1,1,1,1`. Explicit enemy lists extend it. Full-word friendliness writes remain
valid storage; values outside the four-row AI table must not index out of bounds.

Primary `401930` uses actual Euclidean range, acquired visibility, live target,
attack-style/class range policy, and eye-forward alignment at least double .9.
The secondary instead uses approximate range, blast radius plus both physical
radii as minimum distance, body-forward at least .95, and an additional LOS ray.
Torpedo is the submarine's PRIMARY, never a secondary merely because it is a
missile. Fighter uses Minigun primary and Fighter Rocket secondary.

## Authored distinctions

Static asset inspection confirms AI enabled for L5S3 sub3963, L5S4 subs2381 and
3955, and the combat Fighters in L7S3/L12S1/L20S2. Many parked/friendly hulls
lack it. L13S3 Fighter8955 has no AI and explicit none/none weapons. L12S1
Fighter9700 has an instance360 FOV despite its class180 default. Masako4717
has instance180 despite class360. The instance value must win.

The exact recovered caller chain does not make the existing practical flight,
homing, target ranking or combat movement implementation retail-equivalent.
Those integration boundaries must remain explicitly identified.

## Integrated live consumer

The selected submarine/Fighter and independent submarine/Fighter/Masako owners
now acquire through authored AI enabled/default-mode1, actual relation-table,
instance FOV, covered nearest-target checks and finite weapon admission.
Goto_Player retains priority until its visible-opposed handback; fixed Goto and
scripted Attack retain their own owner. Independent movement is claimed only
with real class hull and wet/dry start clearance. No-event RFSV2 hulls require
source-enabled AI and reciprocal RFAI state on restore. Parked switch banks,
attachments, riders, player control, frozen/hidden/dead owners stay authoritative.

Torpedoes use the primary channel; Fighter miniguns and rockets use their actual
muzzle_1/secondary_1 tags. AI damage uses the table scale (.8 Torpedo, .2 Fighter).
Source generation handles survive direct and radius damage, including real
occupied-hull first contact. Live missiles keep normal liquid policy and the
existing provisional homing; miniguns use the swept AP path. Actual Torpedo and
DrillMissile01 renderers are loaded even without player projectile demand.

RFAI1 stores UID/target keys, ammunition, poll and firing clocks, and emitted
rounds, including exact double remaining lifetime. Restore rebinds fresh handles
before publication and joins Attack ammunition/ownership and autonomous RFSV2.
Invalid target weak references are cleared at tick/capture; emitted rounds coast
without guidance. Attack takes control synchronously, so a same-frame save
cannot describe simultaneous autonomous and scripted engagement. Player takeover
shares finite ammunition and accepted cooldowns rather than refilling.

The actual player selection function4a40f0 sets global5cb054 and calls489f70
with friendliness2 at4a4109. Fresh player damage initialization now preserves
that original default instead of zero0; later Set_Friendliness remains live.
This is required for the recovered relation table, rather than an AI-only hack.

Remaining fidelity boundaries: practical array-order nearest selection and hull
muzzle/center sight origin rather than the full retail perception service;
fixed normal-difficulty450ms reacquisition; direct75%-range steering rather than
retail tactical maneuvers; full blind-pursuit expiry, explicit enemy-list extension
and non-default autonomous modes are not implemented by this slice. Physical
flight, cover, damage and persistence are working-code paths, not runtime proof.

## Authored APC primary consumer (final14:00 batch)

The same original default-combat caller405af0 dispatches non-fighter actors
through the ground branch before401580 invokes primary and secondary gates.
Read-only SP source inventory identifies AI-enabled L1S3 APC26 and9627 with
APCMinigun primary and explicit secondary none; later player APCs disable AI.
The written consumer now admits profile2, loads its own999-round APCMinigun
and class range/blind-pursuit metadata, and uses original401930's forward-dot
threshold0.9 with practical bounded chassis steering. Actual muzzle_1, swept
round contact, damage attribution and finite AI-scaled ammunition are retained.
Selected and independent APCs share their own primary scheduler rather than
Fighter gun state. Dormant mortar inventory stays with the existing player/
parked owner and cannot create an autonomous mortar order. RFAI persists
primary state and rejects an APC missile record. Independent rider transport
now covers AI/Attack-owned motion too; promotion still requires an empty host.

The earlier paragraph describing blind-pursuit expiry as missing is superseded
by VEHICLE-AI-BLIND-PURSUIT.md. This final written batch received a brief
material ownership/supply source review with no remaining blocker. No build
or gameplay test was run by the implementation helper; root owns14:00.
