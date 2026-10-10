# One original-spawn automatic-reload check

Source-written for the parent's next consolidated Xbox batch. The helper and
this note have not been run, syntax-tested, compiled, or exercised in XEMU.
Only `tools/xemu_original_auto_reload.py` and this note are part of this
preparation. Parent owns review, commit, the one hourly build/run, and cleanup.

## Why this case

Use original `L3S1.rfl` in `levels1.vpp`, retaining its player start and basis.
The existing 07:00 evidence at
`artifacts/xemu/20261009-0700-prop-release/live-samples.json` records owned
pistol ID 3 with `[3,125,16,0,0,0,448,0]` PLAYER_AMMO at frame 9, before any input.
Frame 318 has the same inventory. After one ordinary frame 330 shot, frame 337
records `[3,125,15,0,0,0,448,0]` and COMBAT shot count 1. That source was
`f531af1a71359c2e5c21e0350580236382bd71b3`. Its retained verification reports
`PASS_OWNER_DESTRUCTION_RELEASE`, stock 64 MiB, no death/transition/save, and
restored disc controls. The new check does not reproduce its movement or aim.

This is the port's existing provisional starting inventory: scene
`campaign_ammo_reset` acquires the pistol and fills its mapped reserve to the
table capacity. It is not an authored original-mission starting loadout.
No inventory or startup policy is changed for this check.

Original L1S1 is unsuitable. Its enabled auto trigger 9030 has links
8663/8630/8366, no script gate, and auto flag 8. Startup immediately dispatches
the authored Strip_Player_Weapons event 8366; the trigger's 30-second timing is
a cooldown, not a delayed first activation. The queued strip is applied on the
first combat tick after the provisional initializer. The 22:00 L1S1 startup
admission did not collect ammo and cannot establish an armed initial state.

## Fixed ordinary input

- Frames 0–119: all inputs neutral.
- Exactly 16 one-frame primary-fire presses at 120, 150, ..., 570.
- Movement, look, jump, crouch, Use, reload, cycle and alternate remain zero.
- Frames 571–779: every input neutral, with no seventeenth empty-trigger press.
- Stop at 780 simulation frames. No aim correction, route adjustment or retry.

Original `weapons.tbl` lines 411–456 declare the 12mm handgun semi-automatic,
SP reserve capacity 125, clip 16, reload 1.1 seconds and primary wait 0.50 seconds.
The live conversion is 66 reload ticks and 30 primary-spacing ticks. Sixteen
accepted presses empty only the loaded magazine, leaving 125 reserve rounds.
The extended neutral tail permits the actual fire action and ordinary reload
to finish; it does not fabricate their completion or assert exact timing.

## Run only in the parent batch

After committing the helper with the integrated gameplay source, build Xbox
once and create the usual successful consumer manifest for that exact clean
commit, XBE and map. Invoke once with the manifest and a new outside-repository
evidence directory:

```text
python tools/xemu_original_auto_reload.py --parent-hourly-batch --consumer-build /path/to/successful-consumer.json --out /path/to/new-evidence-directory --seconds 600
```

The helper adapts the 22:00 original-startup safeguards. It requires a clean
manifest-pinned source, exact XBE/map hashes and unique complete map-symbol
tokens. It acquires session exclusion and refuses an existing project XEMU.
It repackages the already-built XBE without invoking make or a second build.
The shared native runner owns one stock 64 MiB, host-input-disabled, network-off,
snapshot-only XEMU instance on the existing standalone test HDD.

All disc VPPs and the original asset file are checked against Installed_Game.
Original archives, fixed data and base HDD are hashed again after execution.
Existing selectors are journaled and cleared, then only campaign spawn/level,
scene preview, player control and the exact ordinary replay are staged.
There is no altered scene archive, setup event, grant, changed spawn, quick
action, save/load flag, image/audio capture, host input or FPS measurement.
The exact staged input is also retained as `ordinary-input.bin` with its hash.

## Required evidence

One read-only coherent initial probe is requested at frame 30 and must land
before frame 120 in the live scene. It preserves `initial-owner.json` and
checks the actual `campaign_player_inventory` 448-byte owner, pistol ID 3,
selected slot 0, explicit-unarmed 0, owned pistol and loaded 16. Published
PLAYER_AMMO must be `[3,125,16,0,0,0,448,0]`, COMBAT must be unfired and not
reloading, original spawn/basis must match, and setup/script-grant state must
be zero. Private owners are not sampled after scene cleanup.

Retained terminal telemetry must establish:

- Exactly 780 completed frames on stock 64 MiB with available memory.
- Exactly 16 accepted shots, loaded 16, reload countdown 0, and no combat error.
- PLAYER_AMMO exactly `[3,109,16,16,1,0,448,0]`: one completed reload,
 16 transferred rounds, no denied trigger, and no ammo error.
- Conservation: reserve 109 + loaded 16 + shots 16 = initial 141.
- Unchanged selected slot and no cycling, placed pickup, script grant, setup,
 death, respawn, transition, checkpoint/storage operation or section autosave.
- Original spawn/basis and admitted weapon tables, with actual primary rules
 matching magazine 16/reload 66/cooldown 30 and semi-automatic input.

The exact replay contains neither manual reload nor an extra empty trigger.
Together with actual accepted shots and one exact reserve-to-magazine transfer,
these facts establish functional autonomous reload. A short fire/reload clip
sample is deliberately not required. This is not an animation-timing-parity,
audio-output, aiming, performance, campaign-route or broader-weapon pass.

Missing initial-owner evidence, hostiles preventing the sequence, unexpected
ammunition, or another failed predicate yields `CHECK_FAILED`; raw evidence
is retained. There is no fallback level, aim adjustment or second attempt.
The helper verifies restored selectors, original ISO, exact XBE/map, unchanged
assets/HDD and clean pinned source before preserving the final verdict.
Generated pack/run payloads are left for the parent's ordinary cleanup.

The same run passively retains weapon-impact audio admission/owner telemetry.
It is not an audio-output acceptance condition, adds no input or run, and does
not establish audibility or exact contact placement.
