# Campaign alarms

Alarm event46 now reaches a shared PC/Xbox service. On wakes linked living NPCs
and starts one nonspatial `alarm_loop.wav` voice. Repeated on requests still
process NPC links, but neither duplicate the voice nor extend its 17-second
deadline. Off stops the owned voice without clearing that deadline; the regular
tick clears an expired deadline. A subsequent on starts a new interval.
Section loading resets the alarm. Audio failure is counted without blocking
mission progression. PCM is loaded through the existing bounded bank on demand;
the opening fixture adds 44,224 bytes (about43KiB).

The NPC response is a practical first pass: clear the hidden flag for living
linked actors and let armed hostile actors pursue the player through the existing
combat owner. Friendly/neutral actors keep their affiliation. This is not a
reconstruction of the original complete AI states10/12, interruption rules,
appearance effects or timers. Dead/removed actors are not resurrected, non-NPC
links are skipped, and alarm-off does not reset NPC combat behavior.

## Original evidence

Local RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- Factory4b69d0 selects constructor4bebd0 for type46, vtable589b7c.
- On4ba8f0 resolves linked entities through426fc0, invokes48a660 (including
  clearing object flag4000), clears the entity timer at4d0, sets AI state10
  through407e20 and state12 through407e80, then invokes4280b0 and4b0590.
- 4b0590 checks the global active byte7cabb0. Only an inactive alarm starts
  global sample51 through505560 and sets timer7cabd0 to17000ms.
- Off4ba970 jumps to4b05d0, stopping voice7cabc8 and clearing the active flag.
- Tick4b0560 checks timer expiry, calls4b05d0, then clears the timer.
- The independently inspected sounds.tbl row51 is `alarm_loop.wav`, with
  near15, volume0.9, rolloff1. The scene resolves by name, retaining established
  metadata instead of assuming the bank index is the original global slot.

Addresses were inspected in the original disassembly and bounded Ghidra exports
under ignored artifacts/analysis. This is source evidence, not a claim that an
original executable oracle verified the complete AI response.

## Validation

`python tools/replay_alarm.py` passes five PC cases: start, duplicate on, delayed
switch-off,17-second timeout and linked hostile wake in L8S1. Unit coverage in
mission_goal_dispatch checks missing services, delayed on/off and no duplicate
delivery after a deadline is consumed.

L1S1 Alarm8686 starts at0. Switch8687 is requested at frame60 (1000ms), waits its
authored8seconds, then stops the alarm at9000ms. A520-frame test ends before
that switch fires;580frames covers the off action. The1080-frame timeout case
does not request a switch. L8S1 Alarm6449 wakes/alerts one NPC and skips its
non-NPC Message10556 link. This does not prove the original Message routing or
the complete lab encounter.

The520-frame stock64MiB XEMU run render-20260914-172725 passed all selected
gameplay comparisons, including the12 alarm words while active, with5731 free
pages (22.387MiB). The580-frame shutoff run render-20260914-173018 also passes every selected
gameplay comparison with5731 free pages; ALARM is
[1,1,1,1,0,17000,0,0,0,0,44224,8686] on both platforms.
The focused harness compares gameplay fields, not CPU/GPU geometry counts or
pixel parity. Voice ownership/counters do not prove audible device output.

The first linked-NPC fixture in L2S3 failed its first simulation tick with RF_NOT_FOUND(-3) at
scene stage6 before the delayed setup event fired (artifacts/alarm-replay/linked.log).
L2S3-GAMEPLAY.md records the subsequent particle, mover and image-budget fixes;
L8S1 remains the linked-NPC alarm case. No original asset was changed.

Reproduce the native shutoff check after the PC fixtures:

```text
python tools/xemu_render_check.py --input artifacts/alarm-replay/switch_off.bin --spawn --setup-uid 8686 8687 --seconds 240
```

ALARM / rf_scene_alarm contains12 words: on requests, off requests, voice starts,
voice stops, active, deadline (UINT32_MAX means inactive), NPC wakes, hostile
alerts, skipped targets, audio failures, added PCM bytes and last event UID.

Remaining Alarm46 work: representative natural trigger encounters and audible
playback, native linked-NPC coverage, original AI states/appearance effects,
and preserving its active alarm/timer state across section returns. These
limitations remain separate from the Alarm_Siren45 implementation below.

## Alarm_Siren45 first playable (2026-10-09)

Source-written, not yet built or run. The shared event consumer now plays one
positional `alarm_01.wav` at the authored event position. It uses the existing
bounded PCM bank, idle-safe eviction, software mixer, spatial gain and native
device ownership. It has no NPC wake action and no 17-second timeout. Existing
Alarm46 behavior is unchanged.

Original evidence was read directly from the same SHA256-verified RF.exe above:

- Factory4b69d0 selector45 uses jump-table entry4b71bd, allocates0x2c0 bytes
  and calls constructor4bebb0. It installs vtable589b6c: no-op4b8cd0,
  ON4ba830, OFF4ba8c0 and tick4ba880. Creator4b87a0 clears active byte+2b8.
- ON4ba830 returns if the active byte is1. Otherwise it invokes5056a0 with
  sample28, event position+40, gain1, the zero-vector address173c378 and flag0;
  stores its returned voice at+2bc; sends an additional ON link pass through
  4b8b00 with source/actor both-1; then sets active1. Common activation4b8b70
  and tick4b8ce0 still perform their normal propagation afterward. Predicate
  4b8c40 includes type45, so repeated ON still has common propagation.
- OFF4ba8c0 acts only when the stored voice is not-1: stop505a40, voice=-1,
  active0. With a missing handle it leaves active unchanged. The reconstruction
  retains that behavior; audio failure cannot block links or mission progress.
- Tick4ba880 first runs the common delayed scheduler, then starts sample28
  only for active1 with voice=-1. A successfully acquired ID remains stored
  even after its PCM finishes or its generation-qualified mixer slot expires.
  It is never changed back to-1 merely because playback ended, avoiding an
  invented repeating siren. Missing initial audio allocation retries on the
  ordinary tick without a new timer.
- Save4bdc00 selects type45 and writes active+2b8 to record+14 after the common
  header. Restore4be0c0 matches authored identity, restores the common header
  through4bdf20 and restores that active byte. Neither saves a device handle
  or sample position. The new owner likewise saves only the latch and common
  scheduler; a restored active siren acquires a fresh cue on the next normal
  tick, without replaying ON or linked events. It may therefore sound again
  after loading even if the cue finished before the save.

Read-only archive inspection found exactly three authored Alarm_Siren owners
across the installed levels archives:

- `levels2.vpp/L8S2.rfl`:6523 at(18.113934,3.384022,8.407745), linked from
  Alarm6520;6789 at(15.610184,2.860343,24.988617), linked from Alarm6788.
- `levels2.vpp/L8S3.rfl`:6600 at(0.164398,7.546374,22.860016), linked from
  Alarm6597. The siren and its Alarm both have authored header byte1.
- All three sirens have delay0, no outgoing links, zero flags/values/words,
  and empty sound-name fields. The sample must come from the original handler,
  not those empty authored fields. General delayed ON/OFF and link semantics
  use the same shared scheduler; these placements do not establish runtime
  coverage of delayed sirens or sirens with outgoing links.

`tables.vpp/sounds.tbl` row28 is `alarm_01.wav`, near15, volume0.80, rolloff1,
explicitly marked non-looping. The matching `bluebeard.bty` record has no loop
directive. `audio.vpp/Alarm_01.wav` is mono16-bit PCM at22050Hz, with75388 data
bytes (about73.6KiB,1.71seconds); the complete RIFF is75506 bytes. It is distinct
from sample51 `alarm_loop.wav`. Sample identity is resolved by name in the
existing bank, retaining its original parameters and installed loop metadata.

### Ownership and saves

Remove_Object closes only the removed siren's generation-qualified voice and
clears its latch; scene teardown releases all siren voices before the ordinary
audio reset and event destruction. A whole-world load does not touch live audio
on staging rejection. After every fallible guard and parked publication has
succeeded, it stops old voices immediately before the existing assignment-only
transaction. The codec itself performs no audio or event callbacks.

RFEC stays192 bytes. Only type45 writes version4, using offset172 for active0/1;
176..191 stay zero. Other types still write version3. RFEC2/3 sirens remain
readable as inactive legacy owners, preserving their previously represented
common state. Invalid active values and retired-active combinations reject.
RFCH4 retains its existing row size, using the type45 row's enabled word for
the latch. It also preserves pending delays and UID-mapped references through
the existing section-history path. After successful history restoration, only
sirens with an actual saved row release any startup voice; untouched first-entry
startup cues are not restarted.

The runtime event adds8 bytes, with all previous offsets unchanged. Xbox32
stride is116: retired104, siren_active108, siren_voice112. Event allocation and
restore-stage budgets use sizeof, so they account for this growth. Audio service
pointers are appended to the trigger owner, preserving its prior field offsets.
No new PCM cache, parallel voice table, playback timer or checkpoint file wrapper was
added. `rf_scene_alarm_siren[8]` reports starts, stops, failures, loads, added PCM
bytes, last UID, last status and section-history restores.

Verification in this slice is source/disassembly/archive inspection and diff
review only. No build, runtime check, emulator, direct event injection, new
fixture or campaign route was run. Parent hourly Xbox validation and audible
output remain pending. Alarm46's existing AI/global-timer limitations remain.
