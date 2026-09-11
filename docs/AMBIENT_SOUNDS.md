# Ambient sound ownership

The shared C campaign now owns the authored ambient-sound records needed to
resolve Switch sound targets. It does not yet create the original runtime sound
instances or play their loops. Authored presence alone must not be reported as a
successful runtime lookup: original registration can fail and omit an instance.

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
and returns the first matching UID. Registration and runtime flags still need
their own execution proofs before this list is reconstructed.

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

Recover505a90 registration/instance setup and its failure behavior; establish
runtime flags, loop scheduling, start/stop and volume changes; register only
successful instances for UID lookup; connect Switch sound dispatch; recover
the light target owner so routing priority remains correct across both families.
