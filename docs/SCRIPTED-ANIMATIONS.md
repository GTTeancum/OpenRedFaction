# Campaign custom animation events

Play_Animation records with a custom filename in texts[1] now reach linked
living NPCs through shared event dispatch. Delayed requests use the existing
event timer. The initial L2S1 event7201 starts Ult2_cower.mvf on actor7192
after its authored0.5-second delay; this case needs no forced event.

Original evidence targets RF.exe SHA256
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
Loader462150 routes an authored type11 with a custom filename through4b7eb0
as runtime type12. Disassembly46226a..46228e and462464..462478 maps flags[0]
to freeze and flags[1] to action;4b7eb0 stores them at+2c8 and+2c0.
Original4bafe0 assigns a state override for a looping request, or calls5033b0
for an action with the freeze byte. Named action/state requests without a
custom filename use4b7e10/4baec0 and remain unsupported by the live adapter.

The port registers custom clip metadata before playback resources and poses
are created. Existing model-local indices stay stable. Identity plus loop
mode deduplicates registrations; one immutable file/cache identity can back
both loop and action descriptors. Motion payloads still use the existing
reference-aware residency and eviction path. Catalog peak remains512KiB,
playback metadata256KiB, with no new unbounded per-frame allocation.
The append API preserves catalog/output on missing files and budget failures;
relocation preserves old marker names and gives new clips empty marker slots.

A state request replaces looping playback and suppresses the normal looping
controller while active. An action uses normal non-loop playback; completed
unfrozen actions release the scripted ownership. Freeze requests suppress the
base controller and remain owned until interrupted. Goto/Follow and explicit
Attack cancel the prior custom ownership before continuing. Event requests
stop the actor's current scripted navigation/combat intent. All of this is a
practical shared adaptation, not the complete original AI state machine.

Open work: named state/action selection, custom sound/marker declarations,
root motion and scripted movement interactions, automatic combat/death and
stance interruption coverage, exact AI priorities, and revisit persistence.
The longer freeze test verifies the retained request, not the final bone pose
or exact end-frame timing. Unsupported actors/clips are counted explicitly.

SCRIPT_ANIMATION contains10 words: registrations, starts, loop starts, action
starts, skipped, last event UID, last actor UID, active ticks, completions,
and cancellations. Registrations count successful authored event-target binds,
including deduplicated resources; they are not unique file counts.

Validation:
- CTest script_motion_catalog passes actual-file alias/loop deduplication,
  preserved output on missing/budget failure, marker relocation and shared
  payload-cache identity with independent playback registrations.
- tools/replay_script_animation.py passes four PC fixtures: natural L2S1
  delayed cower; L2S2a one-shot8495 completing; loop5459 canceled by Goto8481;
  and end-freeze request8539 retained for600 frames. The latter three inject
  existing authored events into a stationary spawn replay.
- Stock64MiB XEMU render-20260914-183857 passes120 frames of the natural L2S1
  case, with all selected gameplay comparisons matching PC. Animation words
  are[1,1,1,0,0,7201,7192,90,0,0]. Final free pages5432 (21.219MiB).
- PC actor-staged captures at20 and120 frames show guard7192 standing before
  the delayed event and seated/cowering afterward. Artifacts are local only:
  artifacts/script-animation/actor-before.png and actor-cower.png. This is
  pose-change evidence, not PS2 parity or a full campaign traversal.

An initial explicit-event test failed RF_FORMAT at frame0 because the focused
setup whitelist did not include animation events. It was extended to types11/12;
this was a harness rejection, not a motion decoding failure.

```text
ctest --test-dir build/pc -C Release -R "^script_motion_catalog$" --output-on-failure
python tools/replay_script_animation.py
python tools/xemu_render_check.py --input artifacts/script-animation-replay/natural-cower.bin --spawn --level L2S1.rfl --seconds 360
```

## Combat/death interruption

The pre-fix L2S1 replay could slay actor7192 at frame60 while its custom cower
ownership continued for150 active ticks through frame179. The death pipeline
entered normally, but the script owner remained active. Accepted positive
NPC damage and death entry now release custom playback; sight acquisition and
hostile alarm alerting also cancel it before starting combat. Zero/ignored
hits do not enter the positive-damage cancellation branch. This is practical
first-pass priority behavior, not a reconstruction of every original AI gate.

`tools/replay_animation_interrupt.py` passes the natural180-frame control
(150 active ticks, no cancellation) and explicit authored Slay7196 case
(31 active ticks, one cancellation and one death entry). Stock64MiB XEMU
render-20260914-184519 completed180 frames and passed all selected checks:
SCRIPT_ANIMATION=[1,1,1,0,0,7201,7192,31,0,1],
COMBAT_DEATH=[1,5,126,0,0,0,0,0],5432 free pages (21.219MiB).
The run had completed before its extra emulator instance was closed.

Two exploratory NPC-versus-NPC Attack fixtures (5668 and8496 targeting5458)
registered the attack but produced no shots in600 frames. They do not verify
nonlethal damage interruption. Natural sight/alarm interruption and immunity
controls also remain required coverage; the code paths are connected, but
this turn's native evidence proves the death case only.

The focused native harness now refuses to launch while a Red Faction XEMU
process already references this project, and checks again before process
creation. It neither closes that session nor blocks another project's emulator.
On the current machine the guard identified the manual PID34780, ignored the
Perfect Dark process, and rejected a launch before creating a fixture/build.
Use PC checks while the manual Red Faction session remains open.
