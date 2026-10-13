# Player weapon-loop retirement ownership

Source-written on `codex/weapon-loop-retirement`, directly from published
`6d1bf09b54625cade292445139cd9da0779634d4`. The parent independently confirmed
the defect in its newer source. This branch contains only the weapon delta;
it does not include or repeat the separate water-loop correction.

No builds, syntax compilation, tests, runtime, images, haptics or saved-HDD
access were performed. Parent owns integration and the hourly stock64 build.

## Established defect and evidence

The already-integrated flamethrower, rifle alternate, HMG and machine-pistol
consumers share `scene_weapon_loop_audio.inc`. Its previous close resolved the
public voice ID, cleared the software voice, called the void native stop
callback, then discarded every owner field. `rf_xbox_audio_release_voice()` in
`src/platform/xbox/audio.c` returns RF_IO without destroying the native voice
when stop or its 1000 ms stopped-state wait fails. The callback cannot convey
that retained PCM borrower to the old owner, so replacement/reset could forget
it. This is a port lifetime defect, not a missing weapon feature.

The existing native declaration in `audio.h` makes RF_OK/RF_NOT_FOUND
authoritative retirement/absence; RF_IO retains ownership. Lookup matches the
complete handle. `rf_audio_voice_start()` protects software looping slots and
`rf_audio_voice_stop()` clears only a matching complete generation. Idle bank
eviction checks software/native borrowers. Those existing contracts permit a
bounded correction without a global callback or allocator change.

Original continuous firing ownership remains documented in
`PLAYER-WEAPON-LOOP-AUDIO.md`: 41e642..41e688 retains Launch at actor+81c,
41aee3..41aef7 stops it before Stop Sound, and 424fdc..424fef closes it at actor
teardown. This correction does not add new original-game behavior claims.

## Retained identity and firing episodes

- Capture the complete handle immediately after serialized start/bind, then
  retain it directly. Retirement never resolves a public ID or stops by slot.
- Attempt native retirement first. Failure preserves the handle, public ID,
  sample, spatial record and occupied software loop. The protected slot cannot
  be recycled by ordinary starts, including after generation counter wrap.
- Only RF_OK/RF_NOT_FOUND permit generation-qualified software/spatial cleanup
  and clearing the retained voice. Unexpected immediate capture failure keeps
  an unresolved error instead of guessing a later public-ID mapping.
- Logical episode fields (`weapon`, `owner`, `attempted`) are independent of
  the retained device fields (`handle`, `pending`, `voice`, `sample`). Closing
  ends the firing episode immediately even if device retirement is pending.
- A replacement requested while pending consumes its one episode attempt
  without choosing a Foley sample, advancing RNG, loading PCM or starting a
  voice. Retirement success preserves that attempted flag: the held episode
  never acquires a delayed replacement. A later ordinary episode may start.
- Repeated inactive/selection callbacks do not retry an already-pending native
  stop. One frame-qualified hook beside listener refresh retries while pending,
  even after firing ends or the player changes weapons, mounts or dies.
  Re-presenting the same frame does not add a retry.
- Explicit lifecycle drain attempts retirement once and returns its result.
  Reset clears diagnostics/frame bookkeeping only after successful drain.

Existing diagnostics keep their sizes. Owner/weapon/attempted describe the
logical episode; voice/sample may describe its predecessor while retirement is
pending. The existing status/failure counters report retirement errors. No
audio error rejects a shot, changes ammunition or delays gameplay timing.

## Memory and integration

Exactly **12 transient bytes** are added: one uint32 full handle, one uint32
pending flag and one uint32 last retry frame. A static assertion fixes the owner
at 32 bytes (previously 20). No allocations, PCM copies, new diagnostic arrays,
public API changes or save fields are introduced. Non-Xbox staging retains its
existing void stop callback; Xbox uses the existing result API directly.

`campaign_audio_open()` drains before mixer/ID-map reset. `scene_miner()` checks
reset before changing other owners. `campaign_close_movers()` adds a checked
drain before destructors, while leaving the existing teardown sequence and
late close call in place. Failure uses the existing sticky session-retirement
status and retains bank storage. Read-only session idle admission also checks
this owner. Relocation/load continue using the existing non-failing close;
unresolved retirement survives and the ordinary hook retries it. No new
fallible operation is added to saved-world publication.

When merging into the parent's newer scene source, **retain both water and
weapon gates**, plus all newer teardown/storage guards. Both preflight drains
must precede mixer reset/bank destruction; neither may clear the sticky session
error. Combine the early `scene_miner()` reset checks and idle checks rather
than replacing one with the other. This branch has no water include changes
and no water-branch ancestry. All completed weapon admission/cadence, attack
and decay sounds, reload behavior and saved state remain with their owners.

Source review covers retained identity, blocked-episode suppression, ordinary
retry, both mixer-reset sites, scene entry, relocation/load and session idle.
Compilation and native stop-timeout recovery remain unverified.
