# Mission counters and persistence

The authored campaign contains Goal_Create (35), Goal_Check (36) and
Goal_Set (37). `tools/inspect_mission_goals.py` compares their names, flags and
words with the current shared C level reader and reports cross-section references.
For example, L8S1 declares VAT while L8S2 and L8S3 check it. L5S2 has four
ReadyToBlow setters and a threshold of four. These are gameplay dependencies,
not just text for an objectives screen.

Static inspection of the verified original RF.exe establishes these actions:

- 0x4bca30 increments the named local and/or retained goal counter.
- 0x4bca70 decrements the named local and/or retained goal counter.
- 0x4bc970 and 0x4bc9d0 compare the signed counter with the threshold and
  propagate on/off respectively only when the counter is at least the threshold.
  An absent goal does not propagate, even with a zero threshold.
- Checks look for a retained goal through 0x4b8680 before the local goal through
  0x4bd740. Local storage uses offset 0x2c0; retained storage uses offset 0x0c.

These addresses were read as disassembly; this milestone does not claim an
original-code execution oracle or completed loader-field mapping.

The shared campaign module now provides a fixed 64-entry owned counter store
(16,900 bytes). Names are copied, lookups fold ASCII case, counters support
increment/decrement and signed thresholds. Missing checks return false.
Explicit section cleanup retains persistent counters, and declaring a retained
counter again preserves its value. New campaigns start with zeroed storage.
Capacity and signed overflow return errors rather than allocate or invoke C
undefined behavior. These are explicit first-pass policies.

The dispatcher and shared scene now use this store. Goal declarations run before
startup triggers; checks conditionally propagate both on and off, including timed
checks which read the counter when their delay expires. Setters update counts and
retain ordinary outgoing link propagation. Missing setters are harmless. A pending
campaign handoff retains persistent counters and drops section-local counters;
other scene starts initialize fresh state. Both platform loading loops use this
shared scene path. Separate save/load and restart policies remain open.

Loader disassembly at 0x462707 -> 0x4b85b0 establishes that Goal_Create uses
words[1] as the initial count and flags[0] == 1 for persistence. 0x4b85b0 restores
an existing persistent count on revisit. 0x4626d1 -> 0x4b8540 maps Goal_Check's
threshold from words[0]. The original persistent table at 0x4b8610 also has 64
slots; our combined local/persistent budget remains a first-pass policy.

`mission_goal_dispatch` fires the actual L8S1 VAT setter, closes its archive and
events, initializes L8S2, and tests its authored VAT check against a contained
trigger observer. It covers false/true thresholds, delayed counter changes, off
propagation, nonzero initial counts and revisit retention. This is shared runtime
coverage, not an end-to-end traversal of the laboratory or native XEMU evidence.

Goal_Create factory case35 at 0x4b7073 calls constructor 0x4beac0,
which installs vtable 0x589b0c. Its on action 0x4bcab0 and off action
0x4b9f80 are no-ops; ordinary propagation remains active.

Still to complete: native cross-section goal replay and a fresh-game/respawn
policy integrated with save files. Goal HUD text, killed actors, pickups and general world-state
persistence remain separate work.


## Live lab replay

`tools/replay_mission_goals.py` now runs L8S1 -> L8S2 in the shared PC renderer:
actual setter8898 at frame30, exit5625 at frame60, destination loads at61.
VAT retains value1; a separate fresh campaign keeps value0. Section-local
sample1 is retired. This uses process-local event dispatch, not a played route.
`xemu_replay_check.py --goal-uid 8898 --exit-uid 5625 --level L8S1.rfl
--archive levels2.vpp` supplies the same fixture and compares every retained
name/value/lifetime field directly from guest memory with the PC output.

The lab route exposed loading limits: NPC textures now try64px only after the
existing full/128px attempts exceed4MiB; prop render/collision/tag storage has a
bounded1MiB allowance; the combined texture index table allows512 slots on both
platforms. Pixel-memory budgets remain separate. Generic VBM material loads use
frame zero for animated sources (L8S2 console texture has four64x64 frames);
the explicit static-only VBM API still rejects animated inputs. World bitmap
animation clocks and restoration of full texture detail remain deferred.

`rf_scene_campaign_load_stage` identifies resource setup failures in PC logs and
is addressable in XEMU memory. Stages1-18 cover NPC materials, events, triggers,
groups, forces, registration, movers, alpha, audio, clutter, pickups, prop models,
prop materials, glare resources, prop bodies, mover binding and navigation.
Stages19-32 cover base motions, motion catalog, playback, initial poses, model
owners, NPC bodies, weapon models/materials/hands, glare instances, link resolution,
motion residency, NPC geometry and player skin. Stages33-35 mark combined texture
setup, NPC append and prop append. A value records the last reached stage; it is
not proof that every later unstaged operation succeeded.


Native evidence: `artifacts/xemu/replay-20260914-052507/report.json` passes on
stock64MiB with120 inputs and handoff `[1,5625,61,12296]`. All five destination
goal records match PC; VAT=1 persists. The native framebuffer was visually
inspected. Destination rendering leaves6910 free pages (26.99MiB). This covers
controlled event/exit dispatch, not natural interaction with the lab objectives.
The PC fresh-game control separately retains VAT=0. Both builds and35 CTests pass.


## Section-local values on backtracking

The shared scene now snapshots local goal values before a successful section
handoff and restores them after the destination's authored declarations load.
Local goals stay absent from other sections' active goal store. A same-named
counter in another section has a separate value; retained/global counters still
use the existing campaign owner. A new campaign clears both local snapshots
and the first-entry inventory ledger (the latter reset was previously omitted).

The snapshot owner has64 records of copied section/name/value,20,740 bytes total,
no handles or pointers and no gameplay allocation. Save validates capacity before
any writes, so failure cannot partly overwrite a section. Lookups fold ASCII
case; updates reuse records. The inspected campaign declares six local goals:
L5S2 ReadyToBlow; L10S4 mock; L7S2 door1, door2 and vator; L8S1 sample1.
These are practical session checkpoints, not an original on-disk save format.
Full trigger/timer state and repeated non-inventory startup side effects remain
open; a startup script that changes a restored counter still needs that work.

Focused campaign_player_handoff tests pass local section isolation, global goal
preservation, updated values, capacity failure without partial writes, invalid
section names and clearing snapshots. The PC authored lab round trip dispatches
sample1 setter8887, leaves via5625 and returns via5623:sample1 remains1 in L8S1;
a fresh-process control returns0. VAT remains0 in both cases. This proves local
counter ownership with process-local event dispatch, not the played lab route.
Both PC and NXDK builds succeed. Evidence:artifacts/local-goals.

```powershell
python tools/replay_local_goals.py
```

The existing VAT setter/fresh controls in tools/replay_mission_goals.py also
pass: global VAT carries1 or0 into L8S2, while local sample1 is absent there.
The capacity regression includes an existing-value update plus a new local
record when storage is full; failure leaves the saved value unchanged.

Stock64MiB XEMU render-20260914-164401 passes240 frames and both lab handoffs.
The complete active mission-goal list matches PC, including sample1=1 local and
unchanged VAT/doc/sample3/sample5/sample4 persistent values. Selected body,
weapon/ammo, combat, pickup, startup-inventory, animation and audio checks also
match. This is focused runtime evidence, not full mission or rendering parity.

```powershell
python tools/xemu_render_check.py --input artifacts/local-goals/return.bin --level L8S1.rfl --archive levels2.vpp --spawn --goal-uid 8887 --exit-uid 5625 --return-exit-uid 5623 --seconds 360
```
