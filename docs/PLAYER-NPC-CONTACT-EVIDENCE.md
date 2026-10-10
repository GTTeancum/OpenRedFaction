# Player / living-NPC collision source evidence

Baseline inspected: `97e37677e8d4b9ed326ee6b7d5569ecd71ac13ad`.
Original RF.exe SHA256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Read-only source/asset/disassembly investigation, 2026-10-10. Line references below are baseline lines unless stated otherwise.

## Finding

Original ordinary player-versus-living grounded humanoid collision is eligible, including the authored L3S1 guard UID109 under its ordinary living state. The current port has the classifier and contact-response primitives but no ordinary-player movement consumer for these targets. Calling the existing pair response alone would publish metadata, not block player translation. The bounded staged adapter addresses that missing player consumer without claiming the global scheduler or reciprocal NPC simulation.

## Existing live path and gap

- `src/diagnostic/scene.c:9273–9295`, `campaign_physics_body_sweep_for`: world/movers, detached pieces, Driller, passive vehicles and stationary props; no living-NPC composition.
- `scene.c:8713`, `rf_scene_actor_pair_response`: copies qualified actor states, calls original reconstructed response, then publishes body contact/flags. It does not advance or correct position/velocity.
- `scene.c:8821`, `rf_scene_actor_pairs_process`: the concrete callers at 8929 and 8969 are existing opt-in restored-state actor-pair diagnostics. No live scheduler call was found.
- `scene.c:10142–10209`, `actor_tick`: real movement consumes `actor_sweep` fraction, then `rf_physics_contact_advance` and `actor_contact` in its remaining-time loop. `actor_contact` at 10029 selects the existing player/static response, not the positive-inverse-mass dynamic branch.
- Generic sweeps and `actor_sweep` also serve stance/teleport/restore-related probes. The new hook is only inside `actor_tick`, after its sweep; these other queries remain unchanged.

## Original eligibility and response contract

### Object/class/body/link gates

`48be00`, reconstructed at `src/core/collision.c:2563`, classifies kind0/kind0 actor pairs. For an ordinary player-first pair it requires physical body flag `0x20` on at least one body, rejects object `0x40000` and ordinary-single-player hidden `0x4000`, and rejects a target with object `0x20000` (no player collision). For a nonvehicle target (`use_kind != 1`), comparable actual physical extents select flags `0x20`: target extent must be at most twice player extent and more than half player extent. Larger/vehicle cases enter a different model route. The special actor-name/weapon branches are not read on the admitted player-first `0x20` path. The original player-second classifier is asymmetric; it is not rewritten.

`48bb90`, reconstructed at `collision.c:2754`, separately rejects linked handles matching the other full handle and hidden `0x4000` owners. Current integration checks both actual registries and both handle generations, in addition to these source gates.

Original `422b9a` reads class flags2 bit 1 and sets object `0x20000` only if absent. Original `42268b..42270e` supplies physical flags for these classes. Original scheduler `48ca60` uses the active-body `0x40000000` gate and dispatches flags `0x20` with both movement descriptor indices 1 to `49ab00`; other movement modes use `49a420`. The adapter does not synthesize scheduler active flags or schedule pairs: its explicit live nonzero player query provides the narrower entry condition.

### Grounded shape

`49ab00`, reconstructed as `rf_collision_actors_normal_response` at `collision.c:2839`, first requires strict 3D AABB overlap. It then takes the maximum individual radius from each body's actual collision spheres, forms the current-to-next relative ray, flattens Y to zero, and uses `508e40`. Zero horizontal relative motion misses. Initial overlap is accepted only while approaching (direction dot hit-point < 0). It publishes contact time, normal, point, material, inverse mass, full handle and velocity, but does not itself alter actor position or velocity.

Original `49b900` player prequery does not accept ordinary type0/flags `0x20` humanoid pairs: its routes cover flags `0x18`/type3 solid, flags `0x6` plus static-model kind1, then types3/4. Thus the grounded lateral contact policy does not justify adding NPCs to floor/support queries.

### Time/fraction units: verified quirk, not an unresolved assumption

- `508f5e` divides hit distance by ray length and returns geometric normalized time.
- `49ad33` divides that value by length a second time.
- `49ffd2` loads actor contact time; `49ffda/49ffe2` consume `dt - dt*time`.
- `4a002b..4a0056` apply the existing 0.05-distance backoff unless body flag `0x400000` is set.
- `4a0060..4a0077` advance using the full displacement multiplied by this time. No compensating conversion was found.

Core intentionally preserves the double scaling. The bounded live adapter instead uses a clearly documented port-local geometric fraction for nearest existing-world contact composition. It is not exact original timing parity.

### Downstream response

`rf_physics_dynamic_contact`, `src/core/physics.c:514–539`, reconstructs original `49dcf6..49ddef`, the positive-inverse-mass branch. It gates object presence and the original NPC-source/player-target asymmetry, clips the actor's inward normal velocity, takes outward contact-minus-support velocity, and applies `impact * 1.05` along the normal. It corrects the current actor only. Original object dispatch `427550`, reconstructed in `src/core/entity.c:2354–2379`, returns 2 directly for the ordinary player source after lookup/type4 special handling; this narrow living kind0 target does not need pickup/crush callbacks.

## Concrete owner: original L3S1 guard UID109

The named `levels1.vpp/L3S1.rfl` entry was read directly, without launching the engine or scanning other campaign levels. `l3s1-guard109-source.json` records the raw original record, section offsets and decoded values:

- Miner Registration; entity record index1, UID109, class/script `guard1`.
- Position `(-33.6537323, 0.13046247, -17.1305962)`; life85, armor40; seat-host UID -1.
- All 17 creation bytes are zero, giving creation flags0. No authored hidden, no-player-collision or seat attachment switch is set.
- Player start is `(-52.9488831, -2.5273972, -21.9415607)`. UID109 is roughly 20 units away; this audit does not claim that the approved short movement lane reaches it.

`selected-entity-classes.txt` contains the original `entity.tbl` guard1 and miner1 sections. guard1 has run movement (descriptor1), mass100, flesh, flags walk/holds_weapons/sentient/humanoid (`0x20019`), flags2 collide_player (`1`), use none (`0`) and model `ult2_guard.vcm`. miner1 has run, mass100, flesh, humanoid/mouselook, flags2 collide_player and use ai response (`9`). Therefore original class policy does not set guard1 object `0x20000`. Guard initial body flags are `0x80000078`; ordinary player creation1/mouselook yields `0x800000f8`. Both include physical `0x20`. The placed living guard's source object flags `0x06000000` and player's collision view player flag8 contain no rejecting gate, and their unlinked handles do not match each other.

The `$Collision Radius` class scalars (.85 guard, .90 miner) are navigation/class values, not the classifier's body extent or the grounded maximum sphere radius. Named `$Collision Sphere` pairs in these sections are damage scalars, not sphere-radius overrides.

`selected-model-spheres.json` records actual named CSPH rows from `ult2_guard.v3c` and `miner.v3c`. Retained maximum radii are respectively `0.4322106242` and `0.6000000238`; their sum is about `1.032210648`. `rf_entity_class_spheres_build` (`entity_assets.c:69–90`) samples current sphere matrices and zeros humanoid X/Z centers. The physical classifier extent is then `body.state.bounds.radius`, which includes sphere-center height; substituting either navigation radius or maximum sphere radius would be wrong.

`stand-motion-bounds.json` and `eligibility-bounds.json` give a source-only conservative check for the initial unarmed `ult2_stand.rfa` pose. Both model parent arrays match. Position-Bezier control norms along the relevant attachment chains bound translation; root unparented sphere extents provide lower bounds. Miner extent is at least .8315214, guard at least .8244257; uninflated stand upper estimates are 1.0692387 and 1.1057667. Even using a generous 1.2 upper bound for source rotation/float margin, either upper remains below twice the other's lower (>1.64), giving the ordinary comparable-size flags `0x20` branch. These are conservative asset bounds, not runtime-sampled exact extents. The actual staged adapter always reuses the current real body extent and original classifier rather than these estimates.

## Evidence files and limits

The `original-*.asm` files are bounded RF.exe disassemblies for the named gate/shape/time/consumer addresses; `original-49b900-player-prequery.asm` includes following instructions beyond the relevant function end at `49bb6c`. JSON/text files contain only selected named original asset evidence. The implementation contract, combined application patch, unchanged reviewed include/hook and independent review are in `implementation/` and `review/`.

Source establishes the missing live player consumer and the bounded original policy. It does not establish runtime contact, broad campaign compatibility, reciprocal NPC blocking, stationary-player protection, exact original scheduler timing or a new contact/save continuation. No builds, tests, syntax checks, fixtures, injected events, routes, campaign scan, emulation or PC work were performed by this investigation. Parent owns active-repository integration and the scheduled Xbox batch.
