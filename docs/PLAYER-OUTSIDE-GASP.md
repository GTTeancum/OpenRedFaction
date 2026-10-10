# Player outside gasp ownership

Source-written optional presentation for the ordinary on-foot player. Staged
against `0a65a1ee55a4d522810616615dae5d2eaea75ec8`, which already contains the
separate outside-damage consumer. Compilation, physical playback, actual
exposure and stock-memory behavior remain unverified pending the parent-owned
11:00 UTC Xbox batch. No new runtime recipe is supplied or authorized here.

## Original evidence

The inspected RF.exe SHA256 is
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- `4a2700` resolves player+`0x14`. An absent actor or actor+`0x810` dead bit1
  reaches `4a29c2`, stopping player+`0x1150` and clearing its voice to -1.
- For a live actor, `4a2997` calls `4a2a60` independently of damage outcome.
- `4a2a79` calls `428b50`, which resolves the retained room and reads byte
  `+0x42`. The gasp comparison requires this byte to equal exactly 1. Damage
  `421170` instead accepts a nonzero byte; this distinction is preserved.
- `4a2a85` permits current armor <= 0 and closes the owner on positive armor.
  Its unordered x87 behavior is deliberately not reproduced: the port
  requires finite armor, health and physical body coordinates.
- `4a2a9c` queries the retained voice through `505c00`. While it plays, the
  function leaves it alone. Upon completion, `4a2ab1` starts
  `505560(sample50, category0, pan0, volume1)` and retains the new voice.
- Leaving an outside room or regaining armor stops `505a40` at
  `4a2ac2..4a2add`. There is no class, mutant, suit-bit, invulnerability, water,
  applied-damage-result or oxygen-timer gate in this sound function.
- Geometry loader `4eda1f..4eda2a` maps serialized room byte30 to runtime
  byte+`0x42`; dynamic airlock pressure is a separate byte+`0x44`.

The installed `sounds.tbl` row50 names `gasp_01.wav`, near5, gain0.6,
rolloff1. Its comment says “looping sound,” but Bluebeard lines3953–3957 do
not enable looping: envelope `0x1D0F0007`, keyoff0, no Looping Sound, Loop
Start or Preload. The repetition is implemented by the player owner.

The original `audio.vpp` entry `Gasp_01.wav` is 90,892 file bytes, containing
90,848 PCM bytes: mono PCM16, 22,050Hz, 45,424 frames. Its SHA256 is
`15112e484b44c508bdb8268d607655ab658e401c4e3b6dacc478aa3c1c0d682e`.
These values are provenance, not a synthetic duration or a hard-coded PCM
replacement. Existing Bluebeard parsing retains the looping/keyoff words and
does not implement the original envelope.

## Bounded consumer

`scene_player_outside_audio.inc` has one full player identity, one full sound
handle/public ID and one cached sample index. It adds no gameplay state,
timer, RNG, per-player allocation or new bank. The owner is 24 bytes and its
18-word diagnostics are 72 bytes with the existing 32-bit field layout.

The sole start-capable tick follows liquid damage and outside damage in the
ordinary simulation step. It independently requalifies the full registered
player, current life, current armor and current body room. It does not consume
outside-damage counters, class filters, status or positive damage amounts.
Lethal damage therefore stops rather than restarts audio, while an otherwise
eligible invulnerable or nonhumanoid actor can gasp without positive damage.

The body query uses `scene_player_room_locate` slot0, not the listener, camera,
eye-water membership, a forced room index or a new room owner. Both room
counts and the complete 40-byte fixed header are bounded before byte30 is
read. Missing or malformed room information closes audio without rejecting
gameplay. Finite coordinates are required before the spatial lookup.

As in the newly integrated damage slice, this initial consumer is limited to
the qualified allocated local ordinary body: removed/hidden ownership,
attachments, mounts and cinematic ownership are excluded. A defuse modal
also retires the voice because it pauses the ordinary simulation. These are
explicit port-scope restrictions, not claimed original sound exemptions.
Current-body room timing is a port phase adaptation to the original retained
room query. Dynamic airlock-atmosphere reconstruction remains separate.

## Physical lifetime and safe cleanup

`campaign_audio_events.playing` is authoritative whenever present. Xbox
`audio.c` finds the complete retained handle and queries
`nxAudioVoiceGetState`; paused native audio still counts as unfinished. The
software mixer is consulted only when there is no native activity callback.
There is no timer, duration estimate, forced loop or unconditional frame
restart.

After a successful `campaign_sound_start`, the complete generation-bearing
handle is captured immediately. The public ID is never resolved again to
decide lifetime or stop the voice. This preserves ownership when software
audio finishes and reuses its slot before hardware finishes.

Retirement clears the gasp's logical owner before callbacks, sends the full
retained handle directly to native stop, calls the generation-checking
`rf_audio_voice_stop`, and clears spatial tracking only if that full handle
still matches. Thus a newer software or spatial occupant survives. Natural
completion releases the old owner before starting the next one-shot. No
manual PCM release or bank unload is inferred from software completion.

## Optional resource policy

Only `gasp_01.wav` is declared by name, preserving the existing authored table
row and gain. Present nonloop metadata is required; looping, nonzero loop
offset and music declarations are refused. No replacement sample is chosen.

The archive entry is checked before admission against a 96KiB file ceiling.
PCM is loaded only when an eligible player needs playback. Loading/reloading
uses `campaign_ambient_reload`, including the existing
`release_idle_sample` native-borrower check before eviction. The 1,280KiB bank
budget and its capacity are unchanged. The sample is evictable rather than
pinned, and no new PCM pointer is retained by the gasp owner.

Before start, the resident sample must satisfy the same nonempty frames,
1..192,000Hz, mono/stereo, PCM8/16 and byte-consistency contract as the shared
mixer/native adapter. A missing, malformed, incompatible, oversized or failed
resource admission is cached for the level and is nonfatal. This conservative
optional policy also leaves the gasp silent for the remainder of the level
if available bank budget or allocation cannot satisfy admission. It avoids
per-frame archive parsing/eviction on a permanent failure. Other users may
still use or evict any already-resident shared sample.

Voice-start refusal after successful PCM preparation remains retryable on
the next eligible ordinary tick. The shared start API returns only -1 on
failure, so its diagnostic status is a generic refusal, not an asserted
native error classification. There is no backoff timer. The shared start
path supplies flat category0/pan0/requested volume1 and applies authored
sample gain once; its ordinary native allocator does not steal active voices.

## Lifecycle and save boundaries

- Audio-bank open and campaign teardown close the owner before spatial,
  public-ID/mixer, native device or bank destruction/reset.
- Full startup resets both the voice and cached resource success/failure.
- Explicit live teleport and respawn boundaries retire the old voice.
- Ordinary-frame closure-only reconciliation after vehicle/turret ownership
  updates retires on boarding, identity loss or current ineligibility. It
  never starts audio and is not called from shared vehicle candidate staging.
- Post-simulation reconciliation catches late NPC damage, scripted ownership
  or armor/room changes and the paused defuse modal without another start.
- The ordinary death path explicitly retires the owner as well.
- Whole-world successful load closes only in the final
  `!status && world_published` block after storage close. Neither shared
  `scene_player_impact_relocated`, checkpoint preparation nor provisional
  vehicle publication acquires this new side effect. Rejected or
  storage-close-failed loads do not retire it through a new load hook.

No voice, native handle, sample cursor, admission flag or audio owner is saved.
No checkpoint layout, save admission or canonical gameplay hash changes.
After successful load, the next ordinary tick reconstructs eligibility from
the published body, room and armor and can start a fresh one-shot.

## Verification status

Implementation and wiring passed independent source review, including a
bounded direct check of the original executable, tables, metadata and WAV.
The exact serialized start/bind/resolve contract and ordinary-frame/load
closure call sites were also reviewed; no blocking finding remained. See
the staged review receipt. No compiler, syntax
check, test, fixture, emulator, PC runtime, campaign route/scan, image capture,
original-input change, worktree, active-repository edit or cleanup was run.
The parent exclusively owns integration, cleanup and the scheduled Xbox
batch. An ordinary proportional exposure case is not established by this
patch; build-only is appropriate unless the parent has such a case.
