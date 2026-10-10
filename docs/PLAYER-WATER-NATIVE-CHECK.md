# Original-spawn underwater owner check

Source-prepared for the parent's 2026-10-09 19:00 Xbox batch. No wrapper,
compiler, test, emulator, capture or original-code oracle was executed by the
helper. This is one bounded all-neutral check, not a campaign route.

## Original authored opportunity

Use `Installed_Game/levels2.vpp / L10S3.rfl`, version180, 1,971,046 bytes.
Whole RFL SHA256:
`0268e2acbfb73abacc5fd84b2162eedf14fb91fa55353cd134d0b0a702a49cd4`.
Static-geometry payload SHA256:
`3f2287bb387c46cabacdd686007c8c8890b01530b6c1b5db4b6e56f721b4b570`.

The original 48-byte player-start section at RFL offset1605287 supplies
`(414.8033142089844, -62.700538635253906, 102.46246337890625)` and its original
facing. No authored bytes, spawn, orientation, inventory, events or geometry
are changed.

Room19, authored UID6645, starts at geometry offset1911. Its bounds are
`(409.7581177,-80.1040726,96.0761337)` to
`(432.5476990,-60.1579094,115.8681335)`. It has contains-liquid1, type1 water,
texture `greenwater01.vbm`, depth30 and liquid surfaceY `-50.10407257080078`.
The original body start is 12.596466 units below that plane.

Read-only polygon/plane arithmetic supports room19 membership, beyond the
unique containing room AABB: the source query's initial near-up ray from the
authored body first meets front-facing room19 face2441 at distance2.606916,
with opposite/back-facing room18 face2444 at the same boundary. Face2441's
plane is `(0,-1,-8.6880294e-7,-60.15792465)`. A downward line meets front-facing
room19 face1645 at Y `-71.6150734`. This is static source evidence, not a run
of the reconstructed or original room locator. The runtime observer instead
requires both actual body and gameplay-camera query caches to select room19,
with exact current positions and the live original liquid owner.

Original L10S3 has no Cutscene, Teleport/Teleport_Player, Set_Liquid_Depth or
Give_Item_To_Player event. Boot-selected submarine6794 is distant; selection
loads its owner/resources and does not board the player. Ordinary possession
requires Use through `scene_driller_entry_exit_use`; the replay has no Use.
Both entity registries, the full player handle, linked-handle field, health,
water owner and vehicle/turret/cinematic diagnostics are checked live.

## Bounded parent recipe

`tools/xemu_original_water_audio.py` stages only the normal original-level,
original-spawn, player-control and native-audio flags, plus an ordinary RFI6
replay containing360 all-zero48-byte input records. No movement, look, button,
weapon, grant, setup, save/load or transition request is introduced.

The parent runs it once after a successful committed Xbox build, with:

```
python -B tools/xemu_original_water_audio.py --parent-hourly-batch \
  --consumer-build /absolute/path/to/1900-consumer-build.json \
  --out /absolute/new/evidence/directory --seconds 900
```

The manifest must report `PASS_XBOX_BUILD`, exact current clean `source_commit`
and SHA256 values in `xbe` and `map`. Output must be new and outside the repo.
The existing native runner is reused without source changes, with stock64MiB,
the owned standalone `local/xemu-harness/pacing-base.qcow2` and `-snapshot`.
No HDD copy is created and no save/load flags are staged. Existing project
process/session exclusion, exact-token symbol resolution, original-disc asset
checks and original ISO/XBE/map/HDD restoration checks are retained.

Read-only paused observations begin at frames30 and315, with bounded windows
30..79 and315..359. Their actual water-owner frame separation must exceed the
original underwater sample duration41866/11025 seconds. No inputs are adapted,
and a missed observation or failed condition ends this one attempt.

## Acceptance and honest limits

- The original spawn/facing remains exact, and the actual player remains alive
  and on foot, with no setup, scripted grant, death, respawn, transition,
  save/load or occupied cinematic/vehicle/turret. Actual neutral displacement
  is reported, without an invented limit on legitimate buoyancy/gravity.
- Separate exact body/camera room caches identify room19 at the actual current
  positions. Their shared collision world, authored contains-liquid byte and
  live12-byte liquid row match the original. Body/eye Y values are below its
  plane and the ordinary swim owner reports wet/wet mode4 with status0.
- Exactly one underwater start and no surface start, stop, gain update, stale
  identity or water failure have occurred at both live observations. The same
  nonnegative device ID resolves through `campaign_device_voice_ids` to the
  same complete generation-qualified shared mixer handle.
- The shared active looping flat voice, bank sample metadata and native static
  voice agree on the exact PCM borrower, sample/rate/frame format and handle.
  The resident bank name is `Underwater_01.wav`, with83776 original file bytes,
  41866 mono PCM16 frames at11025Hz and83732 PCM bytes. No guest PCM payload is
  read or recorded. The original archived sample SHA256 remains
  `4778e16983eba6628749bd3364da5b165eee9b1bd5bf7bedca063d1603cedd23`.
- The Xbox native callback table is actually bound. Its uniquely matched
  native voice remains PLAYING/static/looping and retains the same sample
  buffer and full handle. This establishes native owner/admission continuity,
  not APU cursor progression, waveform output or audible correctness.
- Terminal replay completes all360 records, stock memory remains available,
  no additional water start/failure occurs, and protected inputs restore.

The observer persists each requested address/count, each returned word chunk,
and complete raw top-level owner headers before validating identities. A
failed predicate therefore retains its original read evidence. Xbox32 layouts
are source-derived: body324bytes, sample112bytes, mixer voice52bytes,
scene prefix176bytes, room cache48bytes and native slot220bytes. Native layout
is pinned to the current `nxaudio.h` SHA256
`a52f170f165a4ad4b5d1eda7a7f781fa97a6af0ece786b7d32f95e5f78f80b3c`.
An ABI/header change requires source review, not weakened runtime predicates.

No surface-swim playback, speed-dependent gain, surfacing/dry stop, relocation,
voice stealing, save restoration, vehicle/cinematic water audio, audible output,
gap-free output, images or FPS is claimed by this case. Normal final teardown
is recorded incidentally and does not count as a natural dry-exit check.

Independent source review found no remaining blocker. Gameplay-frame separation is simulation time; stable PLAYING/static/looping native ownership does not by itself prove hardware cursor advance or audible loop output. Keep any pass scoped to ownership and admission continuity.
