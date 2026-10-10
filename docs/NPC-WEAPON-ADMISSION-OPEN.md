# Remaining NPC weapon admission notes

Source-only bounded audit, October 10, 2026. This is a work-selection note,
not runtime coverage or a claim that every table name has a reachable firing
path. Check exact instance overrides and specialized consumers before coding.

## Concrete fixes and current evidence

- Tankbot Chaingun: original L7S4 UID10696 explicitly selects it. Exact finite
  reserve-fed admission and optional burst onset are implemented at28c62489;
  02:00 compilation passed. Secondary Missile remains independent.
- Reeper Claw / Baby Reeper Claw: original L10S1 UIDs2633/2634 explicitly select
  them. Exact owned/live ammo-free first-playable melee is implemented at68cf1b2c;
  02:00 compilation passed. Original delayed impacts and physical-shield parity
  are not reconstructed by this admission.
- Mutant Attack 1 / Mutant Attack 2: original L10S1 UIDs2623/2624 explicitly
  select them. Both share the claw registry's authored melee flags and require
  no new projectile subsystem. Exact admission is integrated and source-reviewed,
  02:00 compilation passed; see REEPER-CLAW-MELEE.md.

Neutral original startup in the inspected L7S4 and L10S1 cases does not establish
these attacks: owners begin outside ordinary acquisition conditions. Do not
manufacture routes, events, ammo or changed spawns to claim coverage.

## Default-only Drone Vauss: no demonstrated defect

Drone's entity.tbl default is Vauss, but actual instances override it:

- levels1.vpp/L3S4.rfl UID844 selects Drone Smash / Drone Missile.
- levels2.vpp/L7S1.rfl UIDs4041 and4050 select none / none.

UID844's direct event links are When_Dead8345,9469,9701; the other two have no
direct links. No Attack record directly names any of these owners as attacker.
No direct authored Vauss selection was found. Default supply retained for844
is not proof that its gun is selected. Do not add unused Vauss admission based
only on the class default. Drone Smash/Missile need their own source/instance
scope; this negative pass did not implement them.

## Candidate default names requiring separate investigation

The ordinary NPC selector does not admit these names, and the bounded pass did
not find a specialized consumer. Class-table usage alone is not proof of a
reachable firing defect:

- Capek Cane: Capek
- Laser: Meca Turret
- TriBeam Laser: Spike
- Rock Snake Spit: Rock Snake
- Big Rock Snake Spit: Big Snake
- Sea Creature Sonar Attack: Sea_Creature

Do not substitute generic hitscan or melee for a projectile, beam, scripted
boss or terrain-dependent attack. Verify one actual placed/activated owner and
original attack semantics before selecting a practical implementation slice.

## Existing specialized consumers are not missing NPC admissions

Vauss on Stationary Turret / Stationary Turret_Plain / Auto Turret Head uses
shared turret combat. Torpedo, APC Minigun, Jeep Gun, Fighter Minigun and Drill
have vehicle consumers. Their absence from the ordinary NPC selector alone
must not trigger duplicate implementations.

## Drone Smash timing boundary

UID844 has a real selected Drone Smash, authored bash damage 1600 and a
0.4-second impact delay. Do not add it to the immediate-contact registry
without considering that material windup. Existing flying pursuit also stays
horizontal; full three-dimensional reach/contact guards must remain. Source
inspection of a faithful deferred strike is pending; no new Drone admission
is implemented here.

A bounded timing inspection found no NPC action-impact callback. Original
RF.exe 004c3d98–004c3dea reads up to two primary delays into descriptor+150/+154;
0042606f–004260ce starts action 2 and schedules positive delays in milliseconds.
A future Drone-only adapter should parse the real single 0.4-second delay,
retain source/target full handles and service it before combat_due's early
return. It must cancel stale/dead/hidden/replaced orders and clear only after
successful new-timeline/load publication. Existing player shield bash offers
a delayed-contact timer pattern, not a reusable NPC owner. Pending damage is
not represented by RFNC animation rows: adding transient state requires an
explicit save guard or compatible serialization. In particular, NPC row
capture is also used by load preflight, so a save guard there would impose a
brief load restriction too. Do not silently clear a strike during failed
load preparation. Drone timing remains an open implementation slice.

## Post-02 Drone integration

The separate delayed-strike adapter is now integrated and independently
source-reviewed; see DRONE-DELAYED-STRIKE.md. It does not use immediate claw
contact. Its400-ms pending owner and save-only guards await03 compilation and
runtime verification. The timing notes above describe the implementation
constraints retained by this adapter.
