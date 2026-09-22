# Enemy weapon collection

The death-drop policy now admits implemented player weapons by catalog name, independently of the weapon-table index. It supports conventional guns, rockets, grenades, remote charges, flamethrower, Fusion launcher and the Riot Stick. Shields, vehicle weapons, the separate detonator and Machine Pistol alternate resource are excluded.

Gun drops contain at most one magazine, capped by the NPC's actual remaining loaded-plus-reserve ammunition. Grenades and remote charges expose at most one remaining unit. An exhausted weapon still provides ownership without inventing rounds. Riot Stick uses an explicit ammo-free acquisition path; an already owned empty weapon remains available rather than being consumed without benefit. This is a practical first-pass quantity policy, not an assertion of exact retail drop quantities.

Resource demand covers every supported owned fallback weapon before gameplay begins. Collected persistent drops request no resources; an outstanding saved drop requests its recorded weapon even if the dead owner's reconstructed inventory differs. Existing Machine Pistol and remote-charge companion views expand through the normal planner. Demand never grants player inventory.

## Verification

The focused actual-scene test passes policy admission, finite/empty drops, unsupported exclusions, early persistence lookup, fallback resource demand, emission and collection for sniper/rocket/grenade/Riot Stick. PC builds pass.

`python tools/check_enemy_weapon_drop.py --run` builds a local ordinary L1S1 fixture with guard8456 and an explicit Rocket Launcher instance override. A delayed Slay_Object event takes the real damage/death path; a repeated request does not publish another drop. The player then walks forward, collects the weapon, equips it and fires once. No persistent-drop record or inventory is injected, and no DEV flag is used. This verifies scripted death and acquisition, not player-fire lethality.

PC endpoints pass: one available ground weapon; then one collection with six rockets; then one rocket fired with five remaining. All three captures inspected: dropped launcher by the dying guard, equipped launcher, and visible wall-impact smoke/flame. The fixture retains original tables and geometry; other game archives are read-only hardlinks.

Native `artifacts/xemu/render-20260922-181344` passed300frames with identical drop, collection, ammo, selection, launch and impact state. The stock64MiB Xbox had5195freepages (20.29MiB). Native capture inspected: equipped launcher and wall-impact smoke/flame. Disc restored and XEMU closed. PC and NXDK builds pass.

## Remaining integration

Authored no-drop restrictions, richer drop physics/presentation, general campaign disk persistence and wider weapon-specific live encounters remain. The existing stock64MiB residency limits are unchanged; broad levels with more distinct weapon views still require integration coverage. Exact native ammo-free Riot Stick presentation and non-rocket drop encounters are not yet inspected. Audio remains unauditioned.
