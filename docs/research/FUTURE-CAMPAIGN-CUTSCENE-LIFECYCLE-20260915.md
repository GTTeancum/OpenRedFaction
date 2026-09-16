# Future campaign: cutscene lifecycle and completion — 2026-09-15

## Gap and priority

`src/core/event.c` recognizes Cutscene55 and When_Cutscene_Over83 names but excludes both from supported scheduled runtime action dispatch. Existing objective, Load_Level, scripted movement/animation and vitals families already have implementations; this report does not redo them. Camera/control handoff and cutscene timeline playback are absent. This is future campaign work, not a change to the current GeoMod-first mandate.

The installed event inventory has 12 Cutscene events in 12 levels and 4 When_Cutscene_Over events. Full extracted records, incoming event links and resolved outgoing events are retained in `artifacts/future-campaign-re/cutscene-authored.json`. Examples:

- L6S3: Cutscene3696; completion7056 links in order to7001,6998,7058,7060,6839,6841,7178. Resolved event targets include Invert7001, Teleport_Player6998, UnHide7058, Delay7060 and Message7178. The unresolved-in-this-inventory UIDs may be non-event objects; do not drop them.
- L11S3: Cutscene10626 and completion10655 both have authored header_byte1. Completion links10656,12102,10654,2658,10662,12145; Message12102 is named `self_destruct_start` and Teleport_Player10654 immediately participates in the post-cutscene chain.
- L8S4: completion8518 links Invert8385 and Set_AI_Mode10330.
- L15S4: completion19886 links Endgame19887 (`Shuttle`). Ignoring completion is a terminal campaign progression blocker, even if the player can move normally.

## Executed evidence

`tools/future_re/campaign_cutscene_lifecycle.py` executes the fingerprinted original RF.exe (`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`) in isolated Unicorn. Eight scenarios pass; results and exact intercepted argument traces are in `cutscene-lifecycle.json`. No reconstructed build, emulator or desktop input was used.

Three completion cases execute **complete45b900**, real array access40a480/40a490, ordinary event activation4b8b70, the real type83 virtual action through4b9070, propagation predicate4b8c40, ordered link walking4b8b00 and complete delayed tick4b8ce0. Only outgoing generic target dispatch4b65c0 is intercepted. Cases cover an empty event registry, interleaved unrelated types, immediate handlers, a250ms delayed handler, disabled handlers and repeated completion broadcasts.

Five start/stop scenarios execute complete45bb00/45bda0 and the real descriptor search45b280. Audio, game-state, controller, camera and entity subsystem boundaries are intercepted and recorded; the actual writes, gates, list loops and lookup execute. Cases cover absent player, blocking predicate, missing descriptor, descriptor with zero points and ordinary two-point descriptor. Vehicle-specific branches and actual camera interpolation are outside this execution.

## Completion is a broadcast, not a special event action

The on dispatcher4b9070 routes type83 to4b9d60, which is literally `ret`. The useful behavior comes from45b900:

1. Iterate registered event pointer array856470 in list order using live count/accessor calls.
2. Select events whose uint32 type at+290 equals83.
3. Call4b8b70 with ECX=that event and stack arguments `(-1,-1,1)`; this means ordinary source/actor sentinel values and on activation.
4. Ordinary disabled/delay/propagation semantics then apply. Type83 is propagation-enabled; its no-op action is followed by ordered event links.

The event activation owner stores actor at+2a8 and source at+2ac. Disabled bit1 at+2b0 suppresses execution/scheduling but those source/actor writes still occur. With delay0, links dispatch immediately. With delay0.25 and timer global5a3ed8=1000, deadline+298 becomes1250 and complete event tick delivers links exactly at1250. All captured target calls receive source=actor=ffffffff.

There is no completion-broadcast one-shot latch. Calling45b900 twice triggers each enabled immediate completion event twice and reschedules enabled delayed events. A port must ensure timeline completion dispatch occurs once per completion transition; it must not add a permanent one-shot bit to each event, since another legitimate cutscene may complete later.

Static timeline endpoint45b67a..45b6c5 establishes order: expiry of descriptor timer+810; increment point index+808; if index reaches count+4, call45bda0 (restore/end), then45b900 (completion links), then434190(11,0), then435450. If points remain, call45b3f0(descriptor,next_index). That endpoint segment is currently disassembled evidence, not included in the eight executed scenarios.

## Start/stop ownership and fields

45bb00 is cdecl with one descriptor selector argument. The type55 on-dispatch slice4b908d..4b9096 passes the event's uint32 at+24, not one of its generic words/texts. Descriptor search45b280 walks array645fa8 and compares this selector to descriptor uint32+0. Event UID mapping to this field still needs the constructor/loader overlay to be traced explicitly; do not rename it from inference alone.

Start gates: global5cb054 must point to a player entity, and427020(player) must not return AL1. After those gates, several audio/control subsystem calls precede descriptor lookup. The descriptor is written to active pointer645320. For a found descriptor, signed point count+4 must be positive to execute the normal setup; start resets point index+808 to0, calls45b3f0(descriptor,0), performs additional subsystem calls, and copies descriptor float+85c into camera/controller global7c75d4 offset+d8. The ordinary synthetic case writes60 degrees there. Player/controller offset+fb0 is zeroed.

Descriptor fields supported by local instruction evidence:

| Offset | Observed use |
| --- | --- |
| +0 | Selector matched against start argument |
| +4 | Signed point count |
| +8 + index×32 | Point record array |
| +808 | Current point index |
| +80c | Result of45b230(point uint32+0), interpreted as a linked object pointer later |
| +810 | Timer owner used by4fa360/4fa3f0 |
| +81c | Current point record pointer |
| +820 | Optional result of45b590(point string) |
| +824 | Camera position copied by45be90 |
| +830 | Camera orientation copied by45beb0 |
| +859 | Byte gate for hide/vehicle-related start behavior |
| +85c | Camera/controller FOV scalar |

Start saves float global596140 into645fb4, then writes31.25f to596140, and calls52fc60(1). Its exact global meaning remains unknown; do not call it FOV or framerate merely because of the value.

45bda0 is cdecl with no arguments. It does nothing if active pointer645320 is null. Otherwise it calls40ddf0 with camera/controller+c4, resets camera/controller+d8 to90.0f, calls player restoration boundaries, zeroes player vectors+144 and+150 via actual4fad00, clears active pointer645320, invokes further subsystem restore boundaries, calls52fc60(0), and restores596140 from645fb4. The cleared vector semantics require corroboration from physics layout; the byte-level writes are executed evidence.

45be80 reports active pointer non-null as AL. That is merely a lifecycle predicate and not proof that a valid timeline or camera exists.

## Invalid-data behavior is not an implementation model

Original missing-descriptor start still reaches the52fc60(1)/31.25 global footer while active pointer remains null; a subsequent45bda0 cannot restore it because of its null guard. A zero-point descriptor stores a non-null active pointer but exits before normal setup/footer. Our fixtures expose both cases. The port should reject absent/empty invalid cutscene data before changing control ownership, rather than reproduce these inconsistent partial states.

The normal path contains per-level string exceptions and vehicle calls, including4279d0 under a descriptor+859 gate. Those were deliberately excluded from the current execution. Vehicle RE owns that detach path; coordinate later when the cutscene loader identifies authored flags and level exceptions.

## Integration entry points and next proof

Add an explicit start-cutscene callback to the runtime trigger/event services and support55 in `startup_event_action` / scheduled `rf_runtime_events_tick`. Type83 needs an ordinary no-op action with existing delay/propagation semantics, plus a separate scene completion broadcaster over the current event registry. Reuse the existing ordered-link and timer implementations; do not directly fire all completion links while bypassing disabled state and delay.

The scene should own one bounded cutscene controller with explicit inactive/running/finishing states, a safe control/camera handoff, and completion after restore. That is a proposal, not a recovered original structure. Before implementation, recover the authored cutscene section or external resource, point fields/durations, start-selector mapping, camera interpolation and skip/abort versus natural completion behavior. Recovery of Teleport_Player63 is immediately useful because two completion chains depend on it.

No claim of playable cutscenes, exact original camera motion, complete controller semantics or whole-campaign equivalence is made by these probes.
