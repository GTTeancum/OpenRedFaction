# Ordinary unarmed humanoid damage flight

Status: source implementation, source-reviewed; not compiled or runtime-tested by this worker. The parent owns the scheduled stock-64-MiB Xbox batch and startup smoke. Base: c4f9bf2efa76c5eb408adbdc9b399f76b386f91e.

## Missing live behavior and original evidence

Before this change, `campaign_enemy_reaction` returned for an unarmed primary slot and the only qualified unarmed flight owner was L3S1 medic783. Ordinary authored civilians therefore had no consumer of the existing flight/navigation state after surviving player damage.

Installed `RF.exe` SHA256: b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

- 407fb0–408280: reject excluded actions; validate sentient/nonplayer (4270a0), non-Jeep (40a2f0), humanoid (40a150) and the actual resolved source, including self rejection. The player/unarmed branch at 4080f1–408109 and later unarmed branch at 408181–408237 each request 60–120 seconds. Existing reset, shared CRT draw and request ordering is retained.
- 4085f0–4087a0: no-flee flag814 bit10, suppressed Goto, action17 and alarm-request exclusion; actual source lookup, reset/timer and same-target extension; action7 and existing navigation admission. 408dc0 admits unarmed only with both weapon slots -1.
- 404f70 and the existing flight includes own executable route/cower/visibility and ordinary movement dispatch. This change does not substitute generic pursuit, persona, peer propagation or random destination selection.
- 464401/46440c bind the authored Alarm UID. The entity tail has 18 AI/UID bytes followed by 17 creation flags; Alarm is tail+14, no-flee is tail+18+11.

## Bound owner admission

The new family uses actual class flags walk/sentient/humanoid (mask 0x20011), skeletal model kind2, nonplayer use-kind and excludes Jeep/APC. It independently resolves authored class defaults plus instance primary/secondary overrides using existing name rules; both must be unarmed. It also requires authored Waiting, no seat host, no Alarm UID and no no-flee flag. No level or UID routes admit the new family.

Constructor qualification checks the real base motion selection and actual catalog IDs for stand, walk, run, flee_run and cower (states0/2/4/6/13). New owners start at authored Waiting2 without loading extra constructor clips. Historical L3S1 medic783 identity remains solely to preserve its previous receipt/command policy.

At damage onset, the actual registered player object/entity is the sole supported threat. Current selected weapons must still both be -1 before claiming the reaction. Living Waiting/flee owners must have no scripted animation/freeze, scripted physics ownership, held attachment, live seat/link/controller, patrol/Goto/retained route, look/fire/one-shot, combat owner or nonzero AI substate. Cutscenes and unsupported player/visibility forms cannot start this flight. Existing selected pain/death handling remains authoritative.

Before reset, RNG or navigation mutation, fixed local arrays preadmit the five future state clips plus current controller and active-slot demand against the existing 1280-KiB motion cache and 16-slot playback limit. Actual catalog files are demanded; no guessed filenames, extra cache pool or enlarged limit is introduced.

Accepted authored movement/look/fire/animation/AI/teleport/visibility/physics/loadout/death transitions reuse the existing command cancellation sites. Look/Shoot OFF and accepted cutscene entry also release the new-family flight. Cancellation clears outgoing flight commands/navigation and reconstructs inert state before the new command owns those aliases; preserve-incoming cancellation leaves its newly installed state intact. Every accepted cancellation advances the existing lifetime fence, including first-onset callbacks before `entered` is published. The original medic compatibility lane is unchanged.

## Installed authored coverage

Read-only parsing of all 94 installed levels found 252 unarmed walk/sentient/humanoid rows; 127 additionally match authored Waiting/no-seat/no-Alarm/no-no-flee. These are admission candidates, not a runtime coverage claim; scripted/attachment/current-state/visibility/resource gates still apply.

Counts: tech1 35; ult_scientist 23; env_scientist 22; miner1 11; miner2 10; nurse1 9; medic1 7; riot_guard 2; comp_tech 2; miner3, guard2, env_guard, admin_fem, admin_male2 and merc_grunt one each.

All 15 classes have the five required states in their base group, distinct from weapon-specific overrides, and all referenced compiled .rfa clips and .v3c models exist in installed motions.vpp/meshes.vpp. Examples: tech1 uses tech01_run_flee/tech01_cower_loop; env_scientist uses esci_run_flee/esci_cower; miner1 uses Ult2_flee_run/ult2_cower; nurse1 uses admin_fem_run_flee/ADFM_cower. Full read-only inventory and bounded disassembly are external to tracked source under the staged evidence directory.

## Save and memory compatibility

RFNC30 is a version-only admission signal for the widened entered flight family and retains the RFNC29 layout and all current row/world caps. The preexisting flight payload is reused; no new bytes, persistent fields, pools or caps are defined. The shared RNG receipt remains mandatory for entered flight.

New-family genuinely inert owners emit an absent flight receipt. An entered episode uses the existing full receipt; cancellation or ordinary completion reconstructs inert state before omission is possible. Legacy missing receipts rebuild that inert constructor. Action7 still requires a valid typed flight or guard receipt in core validation. Existing medic783 retains its mandatory RFNC25+ constructor receipt. RFNC1–29 decode remains supported, and older profiles reject invented new-family flight receipts.

Candidate admission independently joins authored binding, saved unarmed weapons, candidate base mapping, saved commands/physics/attachment exclusions and the actual restored player. It never substitutes constructor action or selected weapons. RFNS rejects candidate seated flight. The resource plan reserves entered-flight clips within its existing future-owner capacity. Flight aliases and RNG publish only at completed world-restore success. Lazy constructors do not displace the separate RFNC26 ordinary airborne owner.

## Boundaries

This slice does not add NPC-origin threats, arbitrary target acquisition, armed panic, Alarm execution, generic AI, vehicles or GeoMod. Existing navigation/visibility capability limits remain. No worker build, syntax run, feature/campaign test, fixture, emulator session or image operation was performed.
