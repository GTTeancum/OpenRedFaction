# Xbox audio settings

## Scope and status

The staged platform UI and storage implementation adds process-retained master,
effects and music preferences, each from 0 to 100 percent with a default of 100.
The menu changes values by five percentage points and clamps at either endpoint;
the storage contract admits every integer percentage in that range. Zero means
mute once the platform audio adapter applies the corresponding gain.

This is port-specific frontend policy, not a reconstruction of the original
options screen or its volume curve. Audio labels and port-specific notices remain
English, alongside the existing authored English/German/French label subset.
There are no new text assets, render allocations, gameplay fields, campaign save
fields, voice files, music decoders or sample owners.

The UI/storage source binds to the separately implemented backend API from
`e2838f3ecd56f0944cdaca1b79d885607c316376`, modifying none of `audio.c/h`.
Its `rf_xbox_audio_options` contains `master_percent`, `effects_percent` and
`music_percent`; the caller uses `rf_xbox_audio_set_options` after startup
restore, after live preference edits, and when entering Audio. Valid requested
values survive backend open/close. A native `RF_IO` retains those values but can
leave some current native gains unchanged; the menu does not treat that as a
fully applied change. Both Options and Audio display a separate warning.
Entering Audio or adjusting/resetting after a failed attempt retries the desired
gains and refreshes the warning from that result. Saving still persists the
requested values and independently reports disk success or failure. Backend
ordinary polling can also retry native gain application. No voice or decoder
transport is changed by the menu.

The implementation is source-written and the initial independent UI/storage
review found no material blocker. Final API-binding review remains pending.
No builds, tests, fixtures, emulator runs, image work, disk-image access or
publication were performed for this slice. Compilation belongs to the next
parent-owned hourly rollup; functional coverage and audibility remain with
testers.

## Bounded menus and ownership

- Options preserves existing indices 0 through 8 and adds Audio at index 9.
  Ten choices use two fixed five-row pages. Up/Down changes the selected row,
  including crossing the page boundary; the title identifies page 1 or 2.
  Rows start at y=100 and advance by 36 pixels, leaving the existing notices and
  help below y=310. The existing options still retain their physical controls.
- Audio is a separate six-row page: Master Volume, Effects Volume, Music Volume,
  Reset Audio, Save Settings and Back. Rows start at y=100 and advance by 32
  pixels, ending at y=260. Storage notices remain at y=310/330; help remains at
  y=360/382/408/434. It uses the existing 640x480 HUD sink and renderer fence.
- Startup Options and paused-session Options call the same Audio page. Back
  returns one page. Start returns from a cold subpage and resumes a paused
  session through the existing pause owner. Hotplug and input-edge gating use
  the existing physical menu controls. A held A cannot become another fresh
  selection when a subpage opens.
- Reset Audio changes only the three audio values to 100. Root Reset Defaults
  resets controls, button mapping, text language and all three gains. Neither
  reset writes a file.
- Save Settings on either page explicitly persists the whole preference record,
  including controls, language and audio. It is the only write request. Changing
  a value, Back, Start, disconnect, End Session and process shutdown do not save.
- `rf_xbox_input_options` retains all preferences across controller and session
  open/close. Its setter clamps audio to 0..100. Storage validates the range
  rather than silently clamping malformed records. Dirty-state comparison
  includes all three gains.

The pause menu remains the sole owner of audio pause/resume and gameplay-clock
rebasing. The audio-options page must never unpause, restart, decode or replace
voices just to change a percentage. Effects includes ordinary sound effects,
speech and ambience; music has its separate streamed path.

## RFCS5 record contract

All words are little-endian uint32 except the four-byte magic. RFCS5 has exactly
48 bytes, with these offsets:

- 0: `RFCS` magic
- 4: version 5
- 8: total length 48
- 12: nonzero generation, with `(generation - 1) & 1` identifying its slot
- 16: look sensitivity percent
- 20: invert-Y boolean
- 24: radial deadzone percent
- 28: unchanged RFCS4 packed value,
  `((button_permutation_rank * 2 + stick_layout) * 3 + text_language)`
- 32: master volume percent
- 36: effects volume percent
- 40: music volume percent
- 44: FNV-1a accidental-corruption checksum over bytes 0 through 43

RFCS1 through RFCS4 retain their exact 36-byte formats, including checksum at 32
and each version's exact word-28 interpretation. They imply 100 percent for all
three audio values. A 36-byte RFCS5 or 48-byte RFCS1..4 is rejected, as are unknown
versions, invalid checksums, invalid options and unsupported total lengths.

The writer remains canonical: if every audio value is 100, it emits the same
RFCS1/2/3/4 form as before this change, byte-identical at the same generation.
Any nondefault audio value emits RFCS5 even with English/default controls. Thus
RFCS5 always uses RFCS4 packing in word 28. A valid RFCS5 containing all-default
volumes is accepted but is canonicalized only during a later explicit save.

## Inactive-slot safety

The two existing settings paths and their ownership are unchanged. Startup only
reads. Both records are validated before a save; existing generation ordering,
wrap refusal and adjacent-generation checks remain intact. A last-good record
can be loaded beside an invalid/unknown record with a recovered notice, but
saving stays blocked and never repairs that other file.

The absent or older slot is the only write target. Before overwriting an existing
inactive slot, it is opened exclusively without truncation, reread and compared
with both the earlier exact length and all record bytes. An unexpected file
blocks the operation. The selected newest validated slot is never opened for
writing.

After a full record write, an exclusively revalidated known 48-byte inactive
record may be shortened to canonical 36 bytes with native
`NtSetInformationFile(FileEndOfFileInformation)`. This is the only shrink path.
Expansion from 36 to 48 bytes uses the ordinary write. Unknown or corrupt files
are never truncated or repaired. A failed/short write or failed EOF update leaves
the inactive record unconfirmed; it does not erase or overwrite the previously
selected good record.

Success still requires native flush, native close, reopened exact-length
read/decode/byte comparison, and release of the owned S: alias. No saved-status
or restored-options publication occurs before final cleanup succeeds. A native
close or unmount failure retains the existing sticky ownership lock; later
requests do not retry an uncertain handle or replace its alias. Physical
power-loss durability is not asserted.

The existing 12-word storage diagnostics remain unchanged. Native EOF failure
uses the existing write phase and records the native and translated error.
EOF completion waits use the same pending-I/O convention as the existing native
flush. No gameplay/checkpoint schema or capacity is changed.
