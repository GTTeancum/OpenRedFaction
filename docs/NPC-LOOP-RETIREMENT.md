# NPC continuous-loop retirement

Independent source correction based on published
`6d1bf09b54625cade292445139cd9da0779634d4`. The parent confirmed the same
release defect in its newer source. This branch contains no water/player-loop
changes or ancestry. No builds, syntax compilation, tests, runtime, images,
haptics, saved-HDD access or cleanup were performed. Parent owns integration
and the hourly stock64 build.

## Actual defect and existing protection

`scene_npc_loop_audio_release()` previously cleared the software voice, called
the void stop callback and unconditionally erased its fixed owner record.
The record already held the full native `source` handle. The callback can lose
an RF_IO result from `rf_xbox_audio_release_voice()` after a failed stop or
1000 ms stopped-state wait, leaving a created native loop with no scene owner.
Death, removal, weapon change and pass-end cancellation could therefore strand
looping audio and consume a native voice slot until full teardown.

This is **not a demonstrated ordinary PCM reuse defect**. Native
`rf_xbox_audio_release_idle_sample()` rejects looping borrowers. Native
allocation also skips loops, even if their current device state is stopped;
ordinary polling does not destroy them. Full audio reset calls
`rf_xbox_audio_close()`, which stops/shuts down hardware before the scene frees
bank storage. Those protections are retained. NPC rifle/Tankbot burst samples
are nonlooping and remain under existing shared completion/eviction ownership.

Original continuous Launch ownership and the implemented MP/HMG/Vauss/Flame
consumers remain documented in `NPC-LOOP-AUDIO.md`. The original actor+81c
start/stop evidence is unchanged; this correction repairs the port's failed
retirement handling, without new gameplay or original-fidelity claims.

## Fixed records and exact identity

- Retain the existing complete `source`, public voice ID, sample and NPC tuple
  on failure. Retirement never resolves a public ID or dereferences an NPC,
  registry entry, scene slot or authored record. An NPC can be gone while its
  old native generation remains owned.
- Call the existing native result API before software stop. Only RF_OK or
  RF_NOT_FOUND permit generation-qualified software/spatial cleanup. Keeping
  the software loop occupied prevents ordinary slot reuse across generation
  counter wrap while the native borrower is unresolved.
- Reuse bits in the existing `seen` word: observed this pass, pending device
  retirement, and ended logical episode. Begin clears only the observed bit.
  Pending/ended records are never treated as free or overwritten on replacement.
- When replacement for the same NPC body slot is blocked, an available record
  in the existing pool represents the new episode's silent attempt. It does
  not choose a Foley sample, advance RNG, load PCM or start another voice.
  Subsequent cooldown/shots retain that attempt after its predecessor retires.
  Pool exhaustion retains the existing optional-audio suppression policy.
- A stale playback identity during an otherwise admitted episode queues native
  retirement without ending the episode. Successful retirement clears only its
  device fields; the episode stays attempted/silent. Unexpected immediate
  post-start identity-capture failure remains unresolved rather than guessed.
- Death/removal, pass-end cleanup and load/relocation close keep their existing
  non-failing gameplay interfaces. Pending records survive those calls without
  preventing NPC deletion or introducing new save-publication failures.

## Bounded retirement work

Exactly **8 global transient bytes** are added: the last ordinary frame and a
rotating cursor whose high bit records a spent attempt budget. Records remain
32 bytes, with 30 existing records; both sizes are statically asserted. Total
fixed tracking including existing telemetry grows from 1024 to 1032 bytes.
No allocation, PCM copy, new diagnostic array, public API or save field is added.

One hook beside listener refresh services pending records even when no NPC is
alive/firing. Frame entry establishes the budget before any input/restore or
simulation callbacks can remove owners; that preparation does no native work.
Release callbacks feed the same rotating scheduler with that retained frame.
Thus early death/removal and the later listener hook cannot each spend a native
wait in the same ordinary frame. The listener hook also qualifies its frame
when there is no input callback. Re-presenting a frame retains its spent budget.
The scheduler scans at most 30 records and attempts at most **one native
retirement per ordinary frame**, including first attempts and retries. Waiting
records may finish on later frames; gameplay cadence and timers are unchanged.
The existing native call still has its 1000 ms stopped-state wait: this patch
bounds how many calls occur, not the duration of that platform API.

Explicit lifecycle drain first marks every episode ended, then drains the
fixed pool until success or the first failure. A drain attempts at most 30
records and stops at its first unsuccessful retirement; multiple failing
records cannot each add a timeout to that drain. Failed records and all
unvisited records remain owned. Reset clears scheduling/diagnostics only after
successful drain. Ordinary close does not bypass the frame budget.

## Scene integration

Checked drains precede both mixer-reset sites (`campaign_audio_open()` and
`campaign_close_movers()`). Scene entry checks reset before other owner resets.
Session idle admission also checks retained NPC audio ownership. These gates
preserve full-generation identity across reset, rather than claiming existing
native PCM protection was absent. The existing destructor order and late close
calls remain in place; teardown failure follows the sticky session-retirement
status and never clears it.

Parent integration must combine these checks with its water/player-loop gates
and newer storage/teardown guards. Keep all early reset/drain/idle checks and
each distinct ordinary retry hook; do not replace previously integrated gates.
No NPC scheduling, waypoint, animation, save composition, weapon admission,
ammo, shot timing, one-shot cleanup or global audio callback implementation is
changed. Compilation, audibility and native failure recovery remain unverified.
