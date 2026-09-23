# Scripted Look_At first pass

`Look_At` (event type 7) now dispatches ON/OFF, including delayed actions, to its linked NPCs. The scene keeps a bounded per-NPC gaze command. An idle, living NPC turns toward the event's target without taking priority over combat or scripted movement. OFF clears a command from the same event. This uses the existing NPC steering, angular motion and model-pose publication path.

The five installed L1S1 records target another NPC (UID 8322 or 8323) or use the `-999` target sentinel; a zero target appears elsewhere. For this product-first pass, `-999` follows the living player, a positive UID follows a living NPC's eye position, and zero uses the event position. These target interpretations are inferred from authored data and have not been verified against original binary execution. Exact eye-only versus body-turn behavior remains open.

Focused dispatch checks cover ON, OFF and delayed ON. A 90-frame process-local L1S1 replay of UID 8646 activated the linked UID 8323, applied 89 turn steps toward UID 8322, and changed the published model basis on 15 frames. The same replay without the event applied no look commands. PC and NXDK builds pass. A campaign inspection-camera attempt failed to load in both control and activated runs, so the visible turn and native runtime remain unverified.

Active gaze is not restored by event checkpoints or ordinary saves. Other scripted effects and campaign GeoMod remain separate open work in `TO-DO.MD`.
