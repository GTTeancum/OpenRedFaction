# Native voice-over and music output, 2026-10-09 03:00 batch

The existing parent run `native-audio-20261009-030917`, source `defcf61c0b897e143cf8bc2ef8f5e5f91872aed9`, completed600 frames at terminal phase5 on stock64MiB. No emulator rerun was made for this review.

## Harness false failure

The original `report.json` remains unchanged and says FAIL. Its terminal replay words were `[0,600,600,0]`: inactive,600 records,600 consumed, successful read status. `player_input_close()` in `src/platform/xbox/main.c` clears the active word during normal teardown, so it cannot certify prior admission. The harness now checks retained record count, consumed count and read status instead. This validator correction is source-written for the next parent batch; the results below are a post-hoc review of the already captured run, not a claimed rerun of the corrected harness.

## Original gameplay and native device evidence

- Original L1S1 trigger9917 was admitted through normal contact and fired Music_Start9913, `Spirits_02.wav`.
- Original auto trigger9030 fired Message8663, `L1S1_PAA_01.wav`; one speech request started and completed, with111 native-active observations and zero message failures.
- The dedicated music voice queued and completed219 buffers (897,024 submitted PCM frames), with zero errors and one early starvation/restart. No further starvation occurred in the remaining sampled interval.
- The ordinary audio adapter reported six plays, zero rejections, one clean reset and no shutdown-timeout fault. There was no level transition; terminal available memory was3171 pages.

## Actual recorded signal

The actual SDL/ALSA native-output WAV is48kHz, stereo16-bit,4,865,024 frames. SHA256: `724f41f10ac21d859ace27b3447332056180c2b7c51e4a290ef4d64d978bd7cb`.

A contiguous, unmodified gameplay excerpt starts at source frame3,903,616 and lasts961,408 frames (20.029333s). It contains1,879,430 nonzero samples, peak6113, RMS658.473 and zero clipped samples. Excerpt SHA256: `a3c3755e46f8f332c1252b0831681bccb69366aae50cf07acf35e3ebd251d098`.

Offline comparison against the original assets establishes their contribution to that real output:

- Speech: the complete84,994-frame/22,050Hz PA line (3.854603s) matches at recorded82.538417s, normalized correlation0.79845. Seven separate half-second windows correlate0.7845–0.9849 with implied start times within0.4ms, supporting full-line playback without truncation.
- Music: nine one-second windows from original `Spirits_02.wav`, spread over its first16 seconds, match the native stereo-side signal at the same exact implied source start, recorded frame3,954,464 (82.384667s). Correlation ranges0.6946–0.9994. Stereo-side comparison suppresses centered speech; unfiltered middle-channel results, including weak/confounded matches, are retained in the detailed receipt. This supports sustained, correctly timed original-track output rather than merely nonzero queue counters.

The comparison used independent FFmpeg decoding of the original music archive entry only as a numerical reference. No reference samples were inserted into the recording. This is recorded-output evidence, not a listening/intelligibility or real-hardware quality verdict. The run does not validate every radio/NPC route, L14 memory-pressure case, a full music loop, replacement fades, or retail audio polish. The single early stream starvation remains a recorded limitation.

## Preservation

Original FAIL and guest-result records are retained unaltered. A separate proof bundle contains those records, raw native WAV, the contiguous gameplay excerpt, numeric analysis and source-comparison scripts, and this validator fix. Original source assets are excluded. Builds/emulator execution remain parent-owned and hourly; this review did not add a runtime pass.
