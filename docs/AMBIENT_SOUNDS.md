# Ambient sound ownership

The shared C campaign now owns authored ambient-sound records and creates runtime
instances only after successful metadata registration. It does not yet schedule
or play their loops. UID lookup retains original order and first-match behavior;
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
0x3c bytes, stores UID at+8, registered sample at+c, initial voice handle-1 at+10,
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
so routing priority remains correct across both families.

## Runtime registration and ordered lookup

`rf_ambient_instances_open` reconstructs the45aca0 instance fields in44 bytes:
UID, sample index, voice handle, position, three authored sound parameters,
uninterpreted authored word and timer deadline. Voice and deadline start at-1.
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
audio or Switch volume changes. Original45b010/45b040 call505b50 with the voice
handle and zero/authored volume respectively; they do not mutate a persistent
enabled flag in the instance. That playback connection is still pending.

Native64MiB door replay20260910-223419 also passes180 frames with the updated
registration order. L1S1 instance state matches[9,0,412,3934104921]; door motion,
spatial output and PCM hash773011109 remain equal to PC. The door bank-count
assertion now requires100 entries (previously92), while resident-waveform count
stays4. No ambient voices are started by this change.
