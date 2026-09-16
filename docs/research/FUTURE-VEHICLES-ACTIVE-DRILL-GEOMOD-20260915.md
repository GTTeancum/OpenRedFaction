# Actual active Driller excavation orchestration

Priority P1 for vehicle GeoMod. This is distinct from427550 submarine crash effects that were mislabeled Driller contact. Original RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Whole-function instruction proof

`tools/future_re/vehicle_drill_excavation.py` executes complete cdecl421310(owner*) in144 cases: zero/one/two drill bits, each hit mask, owner contact timer disabled/due/future, successful-cut count24/25, and GeoMod return0/1. Actual vector arithmetic, constructors, timer gates and counters execute. Bit pose418e60, world sweep4df1c0, impact/audio/feedback and final467020 are supplied service boundaries. `drill-excavation.json` records calls, full cut request and resulting state;421310.disasm.txt records the reviewed original listing. No visual fidelity, actual collision intersection or successful CSG geometry is claimed.

## Actual shape request

Bit count comes from the count word at `(owner+29c pointer)+1d4`. Each bit obtains pose through418e60; invalid pose result-1 skips it. The fixture supplies valid poses at(-1,2,3) and(1,2,3), identity orientation and a hit fraction0.5. Original forward cast distance is3.5 (float40600000). It calculates hit positions at Z4.75, not the origin or a camera shot point.

After due contacts are collected, their arithmetic mean becomes the GeoMod center, with Y increased by binary32 constant0.78 (589580). Requested scale is accepted contact count times10.0 (58957c). Exactly two collected contacts choose `bit_driller_double.v3d` via437570; otherwise the nonempty set chooses `bit_driller_single.v3d`.

At4216f8,467020 receives `(scale,owner handle,owner room,center,owner published forward+60,template ID,0x3e)`. One contact produces scale10; two produce20 and averaged X0. The query fixtures verify complete argument values, including flags3e. This provides the concrete special Driller template caller that the existing PROJECTILE-CRATER-TEMPLATE report left to investigate; it does not justify changing ordinary rocket crater templates.

## Timing, count and feedback

A positive hit sets owner+814 bit40. Successful-cut counter owner+1470 >=25 aborts at the hit, emits limit feedback4383c0 and never asks467020 for another cut. The fixture starts24/25 and verifies467020 true increments24 to25, while false preserves24. The counter therefore measures accepted GeoMod calls, not contacts or held-fire frames.

Owner+13cc is the contact warmup timestamp. First contact with a disabled timer schedules now+750 and returns immediately; it does not examine remaining bits on that call. A future timer allows contacts/effects but collects no cut points. Due contact positions are collected; before submitting the shape the timer is invalidated. No contacts invalidate the timer as well and stop a retained sound if present. The fixture initially has no retained sound, so the stop-existing-sound path is inspected statically, not covered by these144 cases.

Contact audio uses the retained handle owner+13d0 (initially-1); ordinary contact selects sound ID0x26, exhausted limit selects0x27 when a new handle is needed. Actual sound assets and playback are not verified. Impact4c8a10 is called for eligible contacts; its rendering/damage meaning is outside this fixture. Do not import the submarine crash's100 self-damage into active drilling: this routine has no4892c0 self-damage call.

## Authored context and implementation boundary

Installed L1S2 Driller01 UID8122 uses Drill as primary with no secondary and distinct Driller class bit1000. It is the concrete campaign vehicle requiring this active excavation path. Existing shared terrain467020-compatible services, retained entity models and use/control ownership are the integration points in src/diagnostic/scene.c and shared entity/terrain code. Preserve the two authored bit templates and bounded accepted-cut counter; use existing collision sweep services and explicitly retained sound/timer ownership.

Still open at initial proof: original scheduler/activation call to421310 (resolved by follow-up below), drill-held input gates, actual bit tag/model list at owner+29c, invalid pose handling in live assets, true scene collision queries, exact template geometry placement/fidelity, impact effects and player-readable feedback. This report proves orchestration with supplied geometry services, not playable drilling. No main source edits, builds or emulator runs were performed.


## Activation and bit spin follow-up

`tools/future_re/vehicle_drill_activation.py` executes original41e9ed..41eab0 inside entity update41e4b0, supplying its surrounding ESI=owner/EBP=-1 register context.216 cases pass across actual Driller class predicate, two independent firing-query results0/1/2, delta time0/0.125, prior spin speed0/5/12 and absent/present retained sound. Actual42d780,40a4c0 clamp and4fa3e0 timer invalidation execute;41a830 firing query,421310 excavation body and505a40 stop sound are supplied. Results drill-activation.json.

Only actual class bit1000 enters. Each tick first clears owner+814 bit40 (the excavation body sets it on contact).41a830(owner_handle,current_weapon) supplies continuous firing state. Nonzero increases owner+13c8 speed by delta-time times the binary32 constant412fede0; zero decreases by half that rate. Speed clamps to0..that same constant. Owner+13c4 phase advances by delta-time times the newly stored speed. This is bit animation spin state, not movement propulsion.

A second fresh41a830 query controls excavation: exactly AL1 invokes421310(owner) at41ea82. Otherwise owner+13cc warmup is invalidated, a non--1 owner+13d0 sound is stopped, and its handle becomes-1. In ordinary boolean use, release thus cancels warmup and contact audio while the bit decelerates. The two-query distinction is faithfully recorded, not recommended as a new port complication.

Owner+1470 accepted-cut count stays25 through release and this update prefix; release does not reset the cap. Its construction/reset/save lifetime still needs investigation. This follow-up resolves the earlier scheduler/activation uncertainty: drilling is called from the ordinary entity update while continuous firing is active, and its spin state is updated alongside it. Real held-input activation of the continuous weapon remains a separate integration boundary.


## Counter lifetime audit (static, not full save/load proof)

A linear executable-section operand audit records six direct+1470 references in cut-count-xrefs.txt: the421310 read/increment; constructor422360's zero store at423ae6;4297e0's zero store at429836; and packing/unpacking references42b80b/42be54.4297e0 is reached by a direct call at435dcc in object reconstruction/setup code, but the surrounding end-to-end lifecycle is not executed here. This establishes explicit reset sites outside released-fire cleanup; it does not justify resetting the counter on every use press or labeling the cap permanently lifetime-wide.

The packing function42b300 reads the low byte at42b80b;42ba50 restores an unsigned byte into the full owner+1470 word at42be54. Their packed-record bases/layouts differ in the raw exports, so this is not a standalone binary save-format specification. Full save/load and level transition counter semantics remain open.
