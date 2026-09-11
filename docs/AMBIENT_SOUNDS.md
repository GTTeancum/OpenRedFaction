# Ambient sound ownership

The shared C campaign now owns authored ambient-sound records and creates runtime
instances only after successful metadata registration. It schedules logical
ambient slots but does not yet play their loops. UID lookup retains original order and first-match behavior;
failed registrations are omitted.

## Source evidence

Original RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

Loader460820 dispatches section0x500 to461ff0. For version180 it reads a count,
then each record in this order:

| Field | Encoding |
| --- | --- |
| UID | uint32 |
| Position | three float32 values |
| Header flag | one byte |
| Sound filename | uint16 byte length followed by bytes |
| Near distance, volume, rolloff | three float32 values |
| Flags | uint32 |

The minimum record size is35 bytes. The obsolete pre-version67 string is absent
in v180. Rolloff and flags are version103 additions. The header flag is read via
52c780, which normalizes its result;461ff0 ignores that result. The port retains
the authored byte without assigning new behavior to it.

Unless global64ecbb equals1,461ff0 calls45aca0 with UID, filename, position,
near distance, volume, rolloff and flags. When suppressed it still reads every
record. Constructor45aca0 first calls505a90 for sound registration; a negative
result returns-1 without allocating a runtime instance. Otherwise it allocates
0x3c bytes, stores UID at+8, registered sample at+c, initial ambient-slot index-1 at+10,
position at+14, sound parameters at+28/+2c/+30 and flags at+34, and appends the
instance to the circular list at644ec0. Lookup45afe0 searches that list in order
and returns the first matching UID. Runtime playback and the final authored word
still require further recovery.

## Port reader and owner

`rf_level_ambient_begin/next` read bounded v180 records through the existing
archive reader. They require finite floats, filenames shorter than256 bytes
without embedded NUL, and exact section exhaustion. Failed reads preserve the
caller cursor and output. These validation rules are port input contracts;
they are not claimed to reproduce original malformed-file handling.

`rf_level_owned_ambient_open/close` retain300 bytes per authored record in one
allocation. The budget includes the owner structure. Ownership survives archive
closure, failed opens preserve an empty destination, and repeated close is safe.
Missing sections return RF_NOT_FOUND. The campaign treats their actual absence
as an empty owner and uses a64KiB ceiling. It does not preload all ambient PCM.

## Validation

`verify_ambient_records.py` checks all94 available level entries, including
missing sections. Across78 ambient sections, all877 records match the independent
Python section walk byte-for-byte. The C probe also checks exact/one-byte-short
budgets, truncation at every record, cursor/output preservation, reopening,
archive-independent storage and repeated close. Peak Win32 owner size is14,112
bytes for47 records.

`verify_ambient_loader.py` executes the original461ff0 iteration with supplied
stream reads, string operations and constructor boundary. All877 constructor
argument tuples match the independent records. Normal and suppressed creation
consume identical sections. This verifies loader control flow and field ordering,
not the original filesystem parser, registration internals or playback.

PC and NXDK builds pass. Native64MiB replay20260910-222328 loads authored L1S3
and matches PC `AMBIENT_RECORDS = [24,7212,1344295665]`: count, owner bytes and
ordered record hash. Existing replay-state and sound-bank checks also pass.
This two-command replay proves native ownership for that level, not ambient
playback, Switch sound mutation, or full campaign functionality.

## Remaining integration

Establish the final authored word's meaning, loop scheduling, start/stop and
volume changes; connect Switch sound dispatch; recover the light target owner
so routing priority remains correct across both families. The logical scheduler
is connected; the audio-side slot processor and PCM residency remain open.

## Runtime registration and ordered lookup

`rf_ambient_instances_open` reconstructs the45aca0 instance fields in44 bytes:
UID, sample index, ambient-slot index, position, three authored sound parameters,
uninterpreted authored word and timer deadline. Slot and deadline start at-1.
The name remains in the audio bank. Every negative registration result omits
the instance; accepted instances retain order and duplicate UIDs. `rf_ambient_find`
returns the first matching instance. Capacity is budgeted for all authored rows,
with one allocation and no allocation after registration callbacks start.
Malformed input and insufficient budget fail before invoking callbacks.

Original505a90 returns-1 when audio-enabled byte17543d8 is zero; otherwise it
calls5054b0 with the authored parameters. The shared owner receives this policy
through its registration callback. The campaign uses its enabled deterministic
bank even when no host audio device is attached. This is an explicit port policy.
Registration does not preload PCM. Global declarations precede ambient records,
which precede controllers in all installed levels containing those sections.
This preserves first-registration parameter precedence among these families.
Other sound-producing object families and full registration order remain open.

`verify_ambient_instances.py` executes original45aca0/45b080 construction,
505a90 gating, vector/timer routines, list insertion and45afe0 lookup. Allocation,
string ownership and the5054b0 result are supplied. In285 cases covering2,646
rows, shared C output matches accepted, negative-registration and disabled-audio
paths, including duplicate, zero andFFFFFFFF UIDs. The C probe also tests budget
preflight, malformed input without callbacks, nonempty-owner rejection and close.

`verify_ambient_campaign.py` independently derives sample indices and instance
hashes from global declarations, authored order and archive presence, then checks
actual campaign loading in L1S1, L1S3, L2S1 and ctf01. L2S1 omits two unavailable
underwater resources; these names are absent from all inventoried archives.
L1S1 now registers100 samples, including eight added ambient names, while retaining
the same four controller waveforms and111,904 waveform-file bytes. Bank metadata
is13,128 bytes and the nine runtime instances occupy412 bytes. The sound-bank
budget remains1MiB, with a separate64KiB runtime-instance ceiling.

`AMBIENT_INSTANCES` reports accepted count, rejected count, owner bytes and state
hash. Native64MiB L1S3 replay20260910-223214 matches PC[24,0,1072,2278460717].
This proves native registration/state ownership for that replay, not live ambient
audio or Switch volume changes. Original45b010/45b040 call505b50 with the ambient-slot
index and zero/authored volume respectively; they do not mutate a persistent
enabled flag in the instance. That playback connection is still pending.

Native64MiB door replay20260910-223419 also passes180 frames with the updated
registration order. L1S1 instance state matches[9,0,412,3934104921]; door motion,
spatial output and PCM hash773011109 remain equal to PC. The door bank-count
assertion now requires100 entries (previously92), while resident-waveform count
stays4. No ambient voices are started by this change.


## Separate ambient control slots

Original505ac0 does not allocate a mixer/device voice. It allocates the first
free entry in the25-slot table at1754170. Each24-byte slot contains a signed
sample index, separate device-voice handle, position and volume. A negative
sample marks the slot free. Sample indices0..2599 are accepted; disabled audio,
invalid samples and a full table return-1. Allocation stores device voice-1 and
copies position/volume. The shared runtime instance field has been renamed
from `voice` to `slot` without changing its44-byte layout or values.

Shared `rf_ambient_slot_start/volume/position` reconstruct505ac0,505b50 and505b80.
Volume and position changes require enabled audio, a slot index0..24 and a
nonnegative sample in that slot. They do not restart, mute by flag, free a slot,
or alter a device voice. The audio-enabled argument uses its low byte. These
helpers operate on caller-owned600-byte storage and do not initialize the whole
table, preload PCM or perform device output. Finite positions and volumes are
the tested domain; negative and greater-than-one volumes are copied unchanged.

`verify_ambient_slots.py` executes the original functions and vector callee
without hooks, comparing every byte of the table against PC and compiled NXDK.
All3,072 cases pass:1,024 each for allocation, volume and position. Coverage
includes first/last free slots, multiple free entries, a full table, signed
sentinels, slot/sample bounds, low-byte gating and unchanged neighboring slots.
The previous285 construction/lookup cases still pass after the field rename.

Scheduling source leads:45ade0 tests slot exactly-1; a zero final authored word
allocates a slot immediately, otherwise it passes that signed word to timer
setter4fa360. This identifies a startup-delay path rather than a bit flag.
45ae30 updates positions for nonnegative slots. For negative slots it requires
a valid, expired timer before allocation and clears the timer afterward, even
if allocation returns-1. Direct calls to45ade0 occur at436040 and45c4e0;45ae30
is called at480ef7. Full caller lifecycle, scheduler execution proofs and the
separate audio-side table processing remain the next steps. At that checkpoint the slot helpers were not connected to the campaign; the
scheduler integration below supersedes that limitation.


## Connected startup and tick scheduling

`rf_ambient_schedule` implements45ade0 startup and45ae30 per-frame updates.
Startup only handles slot==-1: a zero authored delay allocates immediately;
a nonzero signed delay sets the timer through the shared wrapped game clock.
Other negative slot values are skipped at startup. Tick handles all negative
slots, allocating only when their timer is valid and expired, then clearing
the deadline even when allocation fails. Nonnegative slots update position.
The helper preflights its timer-domain constraints before changing either owner.

`verify_ambient_schedule.py` executes both original list sweeps, the actual
slot/vector routines and timer routines without hooks. All2,048 original cases
match PC and compiled NXDK instance bytes and the full600-byte table. Coverage
includes startup equality versus tick signed tests, negative/positive delays,
clock wrapping, expiry boundaries, full-table failures and disabled audio.
Six invalid-input cases preserve both owners under the port contract.

Original startup435df0 calls45ade0 at436040 before executing levelstart.vcs.
Original480ef7 calls45ae30 immediately after listener refresh. The campaign
now initializes the table at its startup boundary and ticks after listener
refresh, using its owned60Hz replay clock. Repeated rendering of the same frame
does not add another tick. The table adds600 static bytes; it neither preloads
ambient PCM nor starts mixer/device voices. Full original frame timing and
other lifecycle paths, including the second startup call at45c4e0, remain open.
The initial AMBIENT_INSTANCES snapshot remains registration evidence;
AMBIENT_SCHEDULE contains live scheduling state.

`replay_ambient_schedule.py` checks two authored timing boundaries without input
staging. L4S2 UID1519 remains pending at483ms with six occupied slots and starts
at500ms with seven. L1S2 UID9925 remains pending at83ms; at100ms it attempts to
start into the full25-slot table, clears its timer, and leaves the table hash
unchanged. Audio-side slot processing/recycling is not connected yet, so these
are logical scheduling checks rather than complete original audio trajectories.

Native64MiB L4S2 replay20260910-224846 matches PC at500ms:
AMBIENT_SCHEDULE=[31,500,7,0,967988799,2945009360]. Startup, event/physics state,
registration and sound-bank comparisons also pass. Audible ambient output,
streaming/residency, device voice creation and Switch volume propagation to the
device remain unfinished.

Native64MiB L1S2 replay20260911-001906 also passes the full-table case at100ms:
AMBIENT_SCHEDULE=[7,100,25,0,3414608215,1929861464], matching PC exactly.
PC/NXDK builds and all eight CTests pass.


## Audio-side playback flag evidence

Original505ec0 processes occupied ambient slots after refreshing the listener.
505f93 calls505740 for spatial pan/gain, then multiplies gain by the selected
category scalar. It compares the unrounded x87 product with float0.1 (5893c4)
after storing a rounded copy for playback arguments. Below threshold, an
existing voice is stopped through5442b0 and reset to-1; an absent voice stays
absent. At/above threshold an absent voice starts through5439d0 when sample
byte+0x3e is zero, or543a80 otherwise. Both receive sample, scaled gain, pan,
zero and zero. Start failure is stored back into the voice field. This loop
does not free the ambient control slot.

5439d0 and543a80 lazily load through543760 and call522530 with mode0 or1,
respectively. Disassembly resolves a misleading decompiler stack alias:
522644 reads the mode argument at[esp+0x20], and522668 invokes the buffer
vtable+0x30 with Play flags equal to whether its low byte is nonzero.
The local DirectSound header identifies flag1 as DSBPLAY_LOOPING. Internal
voice flags at+0x28 set bit0 only for mode low byte exactly1 (5225dd), a
slightly different condition outside the normal zero/one caller domain.

verify_ambient_playback_flags.py executes original522530 in36 cases, with
allocation/duplication, absent3D interface, gain conversion and DirectSound
boundaries supplied. It observes ordered gain, SetVolume, SetPan and Play
arguments, the returned voice handle, stored gain and internal flags. Pan
conversion executes original code. This is not an actual-device output test.

For an already active looping ambient,505fed calls543c20(sample,position,1)
and505ffb sends its result to544390. These are gain updates, not pitch:
543c20 computes attenuated default sample/category gain;544390 clamps it and
calls522d30, which invokes buffer vtable+0x3c (SetVolume). Active nonlooping
ambients skip this update. The different startup and refresh gain paths must
be preserved rather than assuming every active sample receives the same update.

543580 populates sample byte+0x3e from bit30 of the filesystem metadata word
at+0xa8.56baa0 looks up a separate180-byte record. Recovering the writer of
that metadata bit remains necessary before declaring authored samples looping;
filename guesses and WAV-loop guesses are not evidence. No audible ambient
integration has been added by these playback-boundary checks.


## Authored loop metadata source

56bbc0 loads bluebeard.bty (string5aac9e+2) and parses sound metadata.
The loop block56bed8..56bf2b queries `+Looping Sound`; when present it sets
bit30 of record+0xa8, requires `+Loop Start:`, and replaces the low27 bits
with the parsed integer masked to0x07ffffff. Without the marker the word is
unchanged. This block is now checked against original instructions rather
than inferred from a sample filename or RIFF chunk. Other metadata fields
include separate Ambient Sound, Music Track, Preload and Incidental markers.
The preceding record initialization zeroes all180 bytes.

inspect_sound_loops.py inventories installed bluebeard.bty and executes that
original block for every authored row plus36 edge cases (2,748 total).
Parser token/integer results are supplied, with token pointer/call-order checks;
the original performs all writes, and all180 record bytes are compared.
This does not yet implement or prove the whole parser or its name normalization.

The installed file contains2,712 rows and263 looping declarations. All263
loop offsets are zero, so this dataset does not require a nonzero loop-start
playback adapter. A basename inventory join finds864 of877 ambient records
unambiguously marked looping, no matched nonlooping ambients, and13 missing
metadata records. Four duplicate basenames elsewhere are retained explicitly
as ambiguous (GlassHit.wav, Hit_Metal.wav, Hit_Rock.wav, Respawn.wav); none
are referenced by the ambient inventory. Do not collapse these duplicates by
last-write-wins without recovering the original lookup/normalization order.

Next: reconstruct the metadata reader and original name normalization, retain
loop settings in registered sample ownership, and connect ambient voices with
bounded PCM residency. These inventory matches do not establish runtime lookup
parity or audible output, and no runtime audio behavior changed in this step.


## Original metadata lookup and duplicate selection

56bb80 compares the suffix after the final backslash in each argument through
original case-insensitive57c130. Forward slashes are ordinary characters. The
loader56bbc0 sorts all4,096180-byte records using5749fa and this comparator,
including zero unused entries, then sets sorted flag1fce720. Sorted56baa0 uses
57772b binary search with the same comparator and rejects a result whose
record+0xac valid bit28 is clear. Its unsorted branch instead walks valid
records using full-string57c130; it does not strip path prefixes.

verify_sound_metadata_lookup.py populates names/loop fields from the inventory,
then executes the actual sort, comparator, search and CRT callees without hooks.
The complete table permutation preserves every input identity, and comparison
keys are sorted. Eight comparator edge cases and5,429 lookups pass, including
case changes, arbitrary backslash prefixes, literal forward slashes, absent
names, empty-name rejection and the distinct unsorted behavior. Full Bluebeard
text parsing remains supplied, so this does not prove parser behavior.

For the installed source order plus zero padding to4,096 entries, original
sort/search selects these duplicate records (zero-based source indexes):
GlassHit.wav -> weapons/index627; Hit_Metal.wav -> weapons/index817;
Hit_Rock.wav -> weapons/index820; Respawn.wav -> music/game/index2199.
All four selected loop bits are false. These selections are evidence about
this dataset and algorithm, not a general first/last-registration rule.
Replacing the sort with an arbitrary platform qsort could change duplicate
selection; preserve the original sorting/search behavior or prove an equivalent
representation before connecting the runtime metadata owner.


## Shared metadata ordering and search

rf_sound_metadata_order and rf_sound_metadata_find now implement the verified
sorted path in shared C. Rows keep a120-byte name and the original+a8/ac words
in128 bytes. An8,192-byte uint16 index table represents all4,096 original
positions, using UINT16_MAX for zero padding. Sorting indexes preserves the
original permutation without moving180-byte records or allocating another
full table. The caller owns rows and order; the functions allocate nothing.

The sorter retains5749fa midpoint pivot/scans and574b4e short-sort tie behavior.
It recurses into the smaller partition and iterates the larger to bound stack
usage. Search retains57772b lower midpoint, backslash-only basename comparison
and the returned record valid-bit check. ASCII/terminated-name and count
preflight occurs before any index write. The runtime will need to keep the
rows and order alive together; these helpers do not own or parse file text.

verify_sound_metadata_port.py compares every index against the unhooked
original across12 datasets, including all installed rows, empty/short/full
tables, equal keys, duplicated names and invalid selected rows. All2,800
lookups per backend match PC and compiled NXDK return identities. Three
invalid-input cases verify preflight preservation (the count overflow case
is checked directly on NXDK). Original full records and compact source rows
remain unchanged. PC/NXDK builds and all eight CTests pass. This is compiled
NXDK CPU execution evidence, not a new XEMU/device playback test.

File parsing, budgeted owner construction/destruction, registration attachment
and ambient PCM/device output remain open. No sound-bank layout or live audio
behavior changed in this ordering/search increment.


## Bounded Bluebeard reader and owner

rf_sound_metadata_read consumes Bluebeard text in the recovered56bbc0 field
order, retaining names and packed+a8/ac words in the shared128-byte rows.
Directory, time, envelope and incidental fields are validated/consumed but
not retained. The reader handles Music Track, Ambient Sound, Looping Sound,
Loop Start, Keyoff Time, Preload and low/medium preservation bits. It accepts
ASCII tokens/comments and bounded decimal/hex integers without NXDK strtod.
The compact representation is for runtime playback metadata, not editor export.

This is a bounded adapter with explicit errors, not a claim to reproduce every
original lexer/error-recovery behavior. In particular unknown/trailing fields,
embedded NUL, non-ASCII text, overflowing integers, missing fields and oversized
names are rejected. A validation pass precedes writes, preserving rows/count
on failure. NULL rows and zero capacity query the number of authored records.

rf_sound_metadata_open allocates rows plus the8KiB index in one block, calls
the verified sorter, and publishes the owner only after success. The budget
includes the caller-owned structure and allocation, excludes input text and
allocator overhead. Empty output is required; repeated close is safe. Input
text is not borrowed. Installed data needs355,344 bytes on Win32/Xbox for all
2,712 records, versus the original737,280-byte record table alone.

verify_sound_metadata_reader.py independently compares every installed name
and packed flag word against PC and compiled NXDK parsing. Four valid datasets
and11 malformed cases pass. Synthetic rows cover signed masked offsets, music,
preload and preservation flags. The probe checks unchanged outputs on reader
errors, rejection of a populated owner, use after input text release, and
repeated close. Exact owner budget succeeds; one byte less fails cleanly.
PC/NXDK builds and all eight CTests pass. Native XEMU file I/O, campaign metadata
attachment and ambient device playback are still unconnected and unverified.


## Campaign metadata file integration

Campaign audio startup now opens sibling bluebeard.bty before sound registration,
retains the compact owner and closes it with campaign audio ownership. File text
is capped at512KiB and freed immediately after parsing; the owner has a separate
512KiB limit. Actual installed text plus owner peaks at724,487 accounted bytes,
excluding allocator/stdio overhead. The existing1MiB sound bank/PCM budget is
unchanged and separately reported. Xbox disc staging copies the original file
locally into ignored output and repacks when it changes.

SOUND_METADATA reports authored count, owner bytes, total looping records,
registered matches, registered looping matches, missing metadata count,
registered packed-word hash, and full compact-row/index byte hash. Registration
lookups resolve through the retained owner, but device voices do not yet use
these flags. No looping behavior or PCM residency changed in this increment.

verify_campaign_sound_metadata.py checks four actual PC campaign logs against
independent packed fields, original sort permutation and authored registration
order. L1S1 has67 matched/15 looping registered entries; L1S3 has77/22; L2S1
has67/11; ctf01 has61/11. All four have33 missing metadata entries, explicitly
counted rather than pretending a lookup succeeded. Every owner hashes to
155846133 and occupies355344 bytes. Existing ambient registration and PCM
bank checks pass unchanged.

Native64MiB XEMU replay20260911-004700 (L4S2,31 frames) reads the staged file,
allocates/parses/sorts it through NXDK, and matches PC:
SOUND_METADATA=[2712,355344,263,69,13,33,1084496309,155846133].
Ambient scheduling and all other checked campaign state also pass;8,481 pages
remain available at completion. PC/NXDK builds and all eight CTests pass.
This proves native file/owner integration, not audible ambient output.

Fallback lead: original543580 uses the static180-byte record at5a7c60 when
56baa0 returns NULL. Its+a8 word is0; its+ac word is0x100003e8 (valid bit plus
keyoff1000). Thus its loop/music selection bits are both zero. This is static
binary evidence alongside the exported registration branch; fallback attachment
to the campaign sample metadata remains to be implemented and verified.


## Shared ambient voice decision

rf_ambient_voice_update now reconstructs one505f75..50603c slot decision with
spatial gain, pan, category gain and loop selection supplied by the caller.
Negative sample slots do nothing. The gain product remains double precision
for comparison with binary32(.1); only the start argument is rounded to float.
Below threshold, an existing voice is stopped and then set to-1. At/above the
threshold an absent voice starts, storing every signed backend return including
failure. Existing looping voices request the separate543c20/544390 refresh
path; existing nonlooping voices do nothing. Loop selection uses the low byte.
Callbacks observe the old slot state; they must not mutate it themselves.

The backend separates start, stop and refresh so gain recomputation is not
mistaken for pan/pitch updates or forced through the initial gain path. This
helper owns neither PCM nor device voices and is not yet attached to campaign
listener updates. The caller must provide finite floats and valid callbacks.

verify_ambient_voice.py executes the original instruction range with spatial,
gain and device boundaries supplied, comparing all24 slot bytes and normalized
callback traces with PC and compiled NXDK. All4,096 cases pass:2,624 no-ops,
361 starts,877 stops and234 refreshes. Coverage includes negative sentinels,
zero handles, failed starts, low-byte loop gating, category gains and100 cases
where the product rounds to the threshold but is actually below it. Original
refresh calls also verify sample/position/scale1 followed by the returned gain
at544390. Callback-state visibility and unchanged slot fields are checked.
PC/NXDK builds and all eight CTests pass. Actual refresh math, bounded PCM
residency and native ambient playback remain the next integration work.


## Shared initial and refresh gain

rf_audio_sample_gain reconstructs543a60: category gain times sample default
volume times input scale, retaining double intermediates before the caller's
binary32 result. rf_audio_ambient_gain reconstructs543c20 and its actual
vector/equality/distance/minimum callees. Disabled low-byte audio returns0.
When source equals listener exactly, it uses the sample-gain product directly,
without distance cutoff or the ordinary upper clamp, and applies all scales.

For distinct positions, vector differences are rounded to binary32 and the
x/y/z squared sum produces a rounded binary32 distance. Only scales below1
multiply the default volume. Distance greater than far returns0; greater than
near applies the unrounded attenuation denominator. A zero denominator gives0.
Category multiplication is rounded to binary32 before the upper-only minimum
with1. Negative values can survive this helper;544390 clamps0..1 later. Slot
volume is not an input to the refresh helper, and it does not update pan.
These differences from505740 and the initial gain path are intentional.

verify_ambient_gain.py executes unhooked543a60 and543c20 including original
comparison, vector, distance and clamp callees. All4,096 cases for both functions
match exact caller-rounded results on PC and compiled NXDK with53-bit x87
precision. Coverage includes308 enabled equal-position cases,1,639 disabled
cases, scale/category/default variations, near/far boundaries, random positions
and the zero-denominator branch. No device hooks or supplied arithmetic are
used. PC/NXDK builds and all eight CTests pass. These helpers are ready for the
ambient backend; PCM ownership and native ambient output remain unfinished.
