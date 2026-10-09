# Voice-over playback ownership

## Status: source-written, not runtime-verified (2026-10-09)

The existing mission voice path was already complete enough to submit a clip:
Message event -> level-local text-table record -> audio.vpp -> bounded PCM RIFF
parser -> shared mixer -> Xbox NXDK static PCM voice. Music uses a separate
path and is outside this change. Queue/start counts alone never prove spoken
or intelligible output.

The missing piece in this slice was live ownership. All dialogue previously
used one global centered voice; every subsequent subtitle stopped it. That
ignored authored NPC speaker positions, could truncate an unrelated speaker,
and restarted a flat event's unfinished voice. Speech now uses a fixed table
of 30 owners (1,920 bytes on the 32-bit target), sharing the existing 30 mixer
and native slots and unchanged 1,280 KiB total sound-bank admission budget.
There is no additional PCM cache, global budget increase, or per-frame heap.

## Original evidence

Read-only RF.exe SHA-256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- Event loader 4624d8..4624ee and constructor 4b7f60 map words[0] to the text
  record, words[1] to speaker UID, trunc(values[0]) to persona, flags[0] to radio.
- Registration 467953..46796f uses near distance 7, default volume .9, rolloff 1.
- Message action 4bb210: persona -1 uses flat playback. With persona present,
  radio flag exactly 1 selects 439400; otherwise positive signed speaker UID
  selects 425210/427020 and 4299b0; without a speaker, 4bb2f1 checks the
  event-owned voice before starting another flat voice.
- Actor 4299b0 stops the previous actor+1400 voice, uses actor+7d4 eye position
  through 5056a0, and retains the returned voice. 429ab0 stops that owner.
- Radio 439471..43948a stops only the selected persona's prior active voice;
  4395b6..4395d0 starts its speech with category 2 and input scale .9.
- 4394c8..4394f1 obtains sample duration, multiplies by 1000, rounds with .5,
  then adds 400 ms for the radio lifecycle. The port uses ceil(PCM frames /
  rate * 1000) + 400 ms as a lower bound on its existing readable subtitle
  fallback. This extension to every speech route is a practical port policy.

Fresh read-only archive inspection found 714 Message events: 384 radio,
188 speaker-owned and 142 flat. L1S1 UID8356 is message0, speaker8323,
persona1, radio0; UID9499 is message23, persona1, radio1. These are existing
authored setup-event targets, not new gameplay fixtures or a campaign route.

## Implemented behavior

- Radio voices replace only the same persona. Actor voices replace only the
  same speaker and follow the live eye position at listener refresh. Flat
  events with a persona suppress a repeated trigger while their own voice is
  active. Another event, speaker, or subtitle does not truncate that voice.
- Missing, dead or unregistered speakers retain the subtitle without inventing
  a centered disembodied voice. The original's additional AI-mode eligibility
  gates are deferred because the port's AI mode coverage is still partial.
- Explicit off requests stop only the matching event's owned speech; the port
  preserves this existing cancellation extension, even though original Message
  off is the generic no-op 4b9f80.
- Empty voice records remain subtitle-only. They can cancel only that event's
  old speech. Missing/over-budget clips do not halt gameplay or clear subtitles.
- Native handles are retained independently of generation-aware software IDs.
  Hardware playing state is authoritative when a native backend is present:
  software cursor completion or slot reuse cannot make an unrelated speaker
  stop that still-playing native voice. Native completion does not shorten the
  deterministic mixer's lifetime; the existing bank checks both borrowers
  before freeing PCM.
- Reset/level teardown clears owners only after the existing synchronous device
  reset. Save admission still explicitly marks active dialogue as presentation
  that is not serialized; it now checks all speech owners.
- Latest subtitle still wins. Its existing 55 ms/character, 4..12 second
  fallback is extended where necessary to cover the loaded clip plus 400 ms.

The existing MESSAGE_AUDIO counters are retained. New MESSAGE_VOICE traces
identify the event, route, owner, sample, native logical handle, PCM frames/rate
and duration. `rf_scene_message_playback[16]` exposes per-route requests,
duplicate suppressions, stops, active owners, last UID/route/sample,
PCM frames/rate, native/software-active polls, completions, unavailable
speakers and duration. Native-active polls are not speaker-audibility proof.

## Read-only asset evidence

All 713 available named mission clips are PCM16 mono: 712 at 22,050 Hz, one at
44,100 Hz. The six missing original references are L20S2_EOS_04.wav,
L8S2_DOC_01.wav, L8S2_HEN_07.wav, L8S2_HEN_09.wav, L8S2_HEN_10.wav and
L8S2_PAA_01.wav. No ADPCM speech decoder is needed for this installation.

L1S1_GRD_01.wav is 80,246 whole-file bytes with 80,126 data bytes / 40,063
mono PCM frames. The source contains 39,568 nonzero samples, peak 32,254;
PCM SHA-256 is `5e7f8f3ee4ba145c1e1948fbde6442c5f234207ee95dd3d38936500d1c14ceac`.
The largest clip, L17S2_EOS_02.wav, is 489,098 whole-file bytes and 11.088 s.
These are source-asset facts, not runtime decoder/mixer/APU measurements.

## Parent-owned next hourly verification

No compiler, tests, emulator, PC runtime or captures were run for this slice.
Do not reuse old September aggregate APU-ring evidence as proof of this code.

Use existing bounded setup-event functionality and stock 64 MiB Xbox, without
campaign traversal or new gameplay fixtures:

1. For authored L1S1 UID8356, confirm route3/owner8323 and the exact source
   PCM frame/rate pair above. Verify the admitted PCM bytes/hash against that
   original asset, a progressing shared mixer cursor, and no load/start errors.
   Repeated actor speech should replace that actor, with an unrelated flat or
   radio voice left active. Existing UID9499 supplies a radio case.
2. Enable the existing native event backend and DSP. Confirm the traced native
   speech handle is active over multiple polls and completes near clip duration
   on a paced audio clock. Observe per-voice APU state and nonzero output while
   it is active; aggregate ring nonzero with other effects is insufficient.
3. To establish spoken output rather than queue activity, record the existing
   native audio-output path over the line, compare the known clip's envelope /
   resampled waveform contribution with a same-scene control or isolate voices
   through supported existing controls, and inspect/listen for an untruncated
   intelligible line. Any allowed audio capture is audio-only; no image work is
   required or performed here. Host speaker audibility needs direct confirmation.
4. Check the largest authored clip under ordinary level audio residency; verify
   no RF_RANGE admission failure and stock-memory headroom. A 489,098-byte clip
   fitting by itself does not establish coexistence with pinned sounds.

Remaining work: radio start/end beeps and 350 ms delay, portraits/mouth motion,
full original AI speaker eligibility, full speaker lifecycle integration,
category volume settings, active speech save/resume, native isolated output,
and crowded/long-clip residency. No retail-parity or audible-output claim.

## Controller-residency blocker and bounded fix

A further read-only source/asset audit found a concrete admission problem.
L14S2's unique controller sound files total 1,064,358 bytes. Its source-derived
bank capacity is 1,431 slots, or 160,296 metadata bytes on 32-bit Xbox (24-byte
bank + 112 bytes/slot). Before optional force/rejection sounds, that leaves
86,066 bytes below the 1,280 KiB ceiling; its largest authored voice requires
235,708 bytes. L14S3 has the same tram files and similarly little space. The
source previously marked these controller preloads permanently ineligible for
idle eviction. Thus a known resource could still become silently unplayable
without any decoder failure. These are source-budget calculations, not a new
runtime allocation or execution result.

Controller samples now retain metadata IDs even if initial optional preload
cannot fit, and become idle-evictable. The common controller/switch sound-start
path reloads missing PCM with the existing bank/device-safe eviction helper.
When a long line needs room, idle tram start/stop/door sounds can be released;
an active/looping software voice or active native borrower still vetoes release.
The force callback's directly borrowed global sample 0x53 remains pinned.
A reactivated controller can reload its sample rather than staying permanently
silent after eviction. Startup registration/parameter precedence is unchanged.

This does not guarantee admission when actual concurrently playing sounds fill
the bank, nor increase the cap. Next hourly validation should include long
L14 speech followed by a controller restart, verifying successful reload,
no active-voice eviction, and stock-memory headroom. No new fixture was added.
