# Xbox controller settings persistence

Source implementation, October 12, 2026. Integrated after the completed17 batch and durable proof.
Not compiled or run. No test, fixture, native launch, HDD inspection or change,
image, cleanup or upload was performed. The parent owns integration and the
single hourly stock64 batch. This document describes the implemented protocol,
not verified FATX durability or a native success result.

## Tester behavior

Options retains the existing indices and actions:

0. Look Sensitivity, 25–200 percent in 25-percent steps
1. Invert Y, on/off
2. Stick Deadzone, 0–40 percent in one-percent steps
3. Reset Defaults, 100-percent sensitivity/invert off/18-percent deadzone
4. Back
5. Save Settings, appended

Only a fresh A edge on Save Settings writes. Reset Defaults, adjustments,
Back/Start, controller reconnect, pause/resume, section transitions, End Session
and New Game remain process-local actions. Settings continue applying immediately
even when persistence is unavailable. No other setting, difficulty selection or
gameplay/replay input record is serialized.

Normal tester startup reads settings once, after the stock-memory/archive gates
and before the first frontend/controller/session opens. Absent files retain
the existing input defaults. A fully validated file is applied only after all
native file handles and the owned drive alias have closed successfully. Failure
does not prevent playing or change input defaults. Diagnostic/replay boots do
not automatically read controller settings.

The six-row screen shows whether values are stored, changed, absent, recovered,
blocked, unconfirmed after an I/O error, or locked after cleanup failure. Changing
a successfully stored value marks it unsaved; returning to the stored values
clears that notice. B and Start retain their existing caller/resume behavior.

## Separate native owner

`src/platform/xbox/settings_storage.c/h` owns only:

- Its own temporary S: alias to `\Device\Harddisk0\Partition1\`
- `S:\OpenRedFaction\controller-settings.0`
- `S:\OpenRedFaction\controller-settings.1`
- Its own 12-word `rf_xbox_settings_storage_state` receipt

It never calls checkpoint storage or codec functions, changes the campaign
selection/generation/diagnostics, mounts/unmounts R:, opens manual/autosave/dev
checkpoint files, formats a partition, replaces a mounted drive, or deletes any
file. The only shared directory is the existing `OpenRedFaction` directory.
Directory creation happens only on an explicit Save after file admission.

The owner follows the native conventions already used by
`checkpoint_storage.c`: NXDK mounting, synchronous CreateFile/ReadFile/WriteFile
and native flush. Its mount directly uses `nxMountDrive`, whose
`IoCreateSymbolicLink` refuses an existing alias. It does not inspect/borrow S:
or replace it. This also avoids `nxIsDriveMounted`'s unchecked probe-handle close.
No NXDK implementation or license text is copied into the project.

Directory creation/opening uses a settings-owned native `NtCreateFile` handle
with FILE_OPEN_IF and FILE_DIRECTORY_FILE. It never replaces an existing file.
The native directory handle uses the same checked close and terminal latch as
data files; `CreateDirectoryA` would otherwise hide that handle on a close error.

Every successfully opened file or directory receives one checked `NtClose` on every return
path. A failed close keeps the static handle and alias owner intact and latches
settings storage until reboot. A failed alias removal similarly retains its
owner and latches. Later calls never clear, reopen over, retry or unmount an
uncertain owner. A native close failure cannot become a successful restore or
Save notice.

## RFCS1 record and admission

Each file is exactly 36 bytes; all integers are explicit little endian:

- Bytes 0–3: ASCII `RFCS`
- Bytes 4–7: version 1
- Bytes 8–11: record length 36
- Bytes 12–15: nonzero generation
- Bytes 16–19: look sensitivity 25–200, exact 25-percent steps
- Bytes 20–23: invert Y, exactly 0 or 1
- Bytes 24–27: deadzone 0–40
- Bytes 28–31: reserved, zero
- Bytes 32–35: FNV-1a32 over bytes 0–31

Generation 1 belongs to slot 0; generation 2 to slot 1; alternation continues.
The checksum covers format/version/length, generation, all settings and reserved
bytes. It detects accidental corruption; it is not authentication. Size mismatch,
truncation, trailing bytes, unknown version/magic, reserved data, invalid ranges,
invalid boolean, slot/generation mismatch or checksum mismatch all reject.
The input setter is never used to normalize malformed stored values.

If both records validate, their generations must be adjacent; ties and gaps are
ambiguous and reject. There is no wrap: Save at UINT32_MAX refuses. A lone valid
record may load when its partner is absent. A valid record with a corrupt/unknown
partner restores the valid controls, visibly reports recovery and blocks Save;
it never silently repairs the other file. If neither validates, startup retains
defaults and Save remains blocked. An I/O failure is not treated as absence or
known corruption: the owner does not guess which inaccessible slot was newer.

## Bounded commit and failure protection

Each explicit Save:

1. Mounts its own alias and fully reads/validates both slots with exclusive
   synchronous read handles, closing each. Any corrupt, unknown, ambiguous or
   inaccessible existing file prevents writes.
2. Chooses only the absent or older slot. The highest validated generation is
   never opened for writing. The first save chooses slot 0/generation 1.
3. Creates an absent target with CREATE_NEW. For an existing older target, uses
   OPEN_EXISTING without truncation, revalidates and compares its original bytes
   under the exclusive read/write handle, then rewinds. An unexpected newly
   appearing file or changed target is never silently overwritten.
4. Writes exactly 36 bytes, checks the full native byte count, calls
   NtFlushBuffersFile and waits for STATUS_PENDING completion when necessary.
5. Checks native close, reopens that target, validates and compares all 36 bytes,
   checks close again, and removes only its owned alias.
6. Reports success and remembers the persisted UI baseline only after every
   stage succeeds. No heap or checkpoint-sized buffers are needed.

This is a two-record commit protocol, not a claim of power-loss-atomic FATX
rename/replace. The previous highest validated file's contents are untouched on
write, flush, reread or cleanup failure. If no previous valid record existed,
failure still leaves the process defaults/settings usable. A failed operation
may leave partial, complete but unconfirmed, or uncertain inactive bytes. On the
next boot a complete validated newer record can be selected; a torn one cannot
hide the older valid record, and its existence blocks silent overwrite. Native
flush success cannot prove physical disk/controller durability. The code never
deletes a rejected file to make retry succeed.

There are two bounded startup reads and at most four 36-byte record reads plus
one 36-byte write per Save. Files are never scanned for unbounded content. The
application has no other writer for these names. It does not claim coordination
with an external process that replaces the selected file between handle scopes.

## Source review and next parent batch

Source review covered framing, strict ranges, checksum coverage, generation
selection/overflow, good-plus-torn recovery, absent creation, target revalidation,
all handle cleanup returns, terminal ownership retention, one-time startup
publication, Save-only write dispatch and unchanged rows 0–4.

The parent should compile this only after integrating it into a later batch.
Native persistence verification needs an authorized disposable/fresh settings
disk: boot defaults, change controls, explicitly Save, restart, confirm restored
controls, then demonstrate process-only edits are not autosaved. Any bounded
corruption/failure check must use disposable settings files. Do not use, inspect
or mutate a protected original saved disk merely to verify this feature. No
new per-slice test fixture is provided, and no native result is claimed here.

Relevant native source references inspected read-only:
`nxdk/lib/nxdk/mount.c`, `nxdk/lib/winapi/fileio.c`,
`nxdk/lib/winapi/handleapi.c`, `nxdk/lib/winapi/filemanip.c`, and
`nxdk/lib/xboxkrnl/xboxkrnl.h`.

Integration files: settings_storage.c/h, main.c, input.h(comment only),
scene_session_menu.inc, scene_preview.h, platforms/xbox/Makefile and this document.
Recommended TO-DO.MD milestone: Controller settings persistence is source-written
with explicit Save Settings, guarded startup restore, independent RFCS1 slots
and fail-closed native ownership; await parent integration and the next scheduled
stock64 compile/persistence check (docs/XBOX-CONTROLLER-SETTINGS-PERSISTENCE.md).
