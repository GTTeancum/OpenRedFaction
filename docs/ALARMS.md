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

The first linked-NPC fixture in L2S3 failed scene loading with RF_RANGE(-3) at
scene stage6 before the setup event ran (artifacts/alarm-replay/linked.log).
That failure remains open; L8S1 provides an independently loadable authored
linked-NPC case. No original asset was changed to bypass the failure.

Reproduce the native shutoff check after the PC fixtures:

```text
python tools/xemu_render_check.py --input artifacts/alarm-replay/switch_off.bin --spawn --setup-uid 8686 8687 --seconds 240
```

ALARM / rf_scene_alarm contains12 words: on requests, off requests, voice starts,
voice stops, active, deadline (UINT32_MAX means inactive), NPC wakes, hostile
alerts, skipped targets, audio failures, added PCM bytes and last event UID.

Remaining work: representative natural trigger encounters and audible playback,
native linked-NPC coverage, original AI states/appearance effects, Alarm_Siren45,
and preserving active alarm/timer state across section returns. These limitations
remain separate from the implemented Alarm46 siren and practical wake behavior.
