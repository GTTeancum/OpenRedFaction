# Smallest writable Xbox DEV destruction checkpoint

2026-09-16, read-only implementation plan. No build, emulator, original-game launch or runtime filesystem experiment performed for this report. Scope is the existing RFDS DEV destruction state, **not** a full campaign/player/inventory save.

## Native helper implementation update

`src/platform/xbox/checkpoint_storage.h/.c` now implements the bounded adapter described below. It owns no heap allocation and wraps the existing two-slot transport with a caller-owned session token and buffer. `open(session,writable)` refuses an existing R: alias, mounts Partition1, and creates/validates the directory only for writable requests. A directory setup failure retains mount ownership; the caller must still call close. `load` passes the pure RFDS validator to the transport. `store` writes/verifies the inactive generation then opens only that new slot with GENERIC_WRITE/OPEN_EXISTING, invokes NtFlushBuffersFile, handles STATUS_PENDING using the NXDK synchronous-I/O wait pattern, and closes the native handle. Any store/flush/close failure invalidates the token; load is required before retry. It never opens the protected previous generation for writing.

`rf_xbox_checkpoint_storage_state[8]` exposes phase, RF status, Win32 error, NTSTATUS, generation, slot, bytes and flags (owned mount, usable selection, native flush success). Portable stdio failures intentionally report native error0 because errno does not guarantee a current GetLastError. `close` unmounts only an alias owned by this session; unmount failure retains ownership for retry. The helper is not yet added to the build or invoked by scene/main; primary owns integration.

Additional exact local evidence: NXDK `winapi/fileio.c` CreateFileA selects FILE_SYNCHRONOUS_IO_NONALERT unless OVERLAPPED, and its ReadFile path waits on STATUS_PENDING using NtWaitForSingleObject then io.Status. `winapi/filemanip.c` CreateDirectoryA274..298 uses FILE_CREATE and returns mapped NT errors; the helper accepts only already-exists/file-exists with a confirmed directory attribute. `xboxkrnl.h` IO_STATUS_BLOCK197 and NtFlushBuffersFile2969 supply the native bridge. These are source-backed API observations; the helper has not been compiled or executed by this agent.

Remaining proof is still the isolated two-launch HDD-copy test below: write/flush in launchA, exit, load solely from the same owned non-snapshot HDD copy in launchB and compare exact RFDS/terrain/next-blast state. A successful kernel flush result does not establish physical power-interruption durability. No emulator was launched for this implementation.

### Adapter compilation and failure-injection verification

The actual helper now compiles independently with NXDK `nxdk-cc -DRF_IMAGE_XBOX_NATIVE -std=c11 -O2 -Wall -Wextra -Werror -fstack-usage -Iinclude -c`; the object is isolated at `artifacts/checkpoint-native-helper/checkpoint_storage.obj`, without linking or changing the native target. Retained `python tools/verify_xbox_checkpoint_storage.py` compiles the same helper with host doubles and passes18 cases: preexisting mount, mount failure, retained mount on directory failure, existing-directory variants, file/directory collision, read-only save refusal, successful new-slot flush, portable write failure, native reopen failure, flush failure, close failure, pending-flush success/wait failure/completion failure, unmount retry, initial generation and invalid-selection refusal. Output is `artifacts/checkpoint-native-helper/host/report.json`, recording the tested helper source hash.

The doubles assert the exact Partition1 mapping, directory and slot paths, GENERIC_WRITE/OPEN_EXISTING access, native close attempts, and selection invalidation. They do not exercise real disk/kernel behavior or replace shared transport file-corruption tests. A native CloseHandle error is reported and invalidates the token; because the kernel refused closure, this path cannot claim proven handle release. No adapter source fixes were required by these tests. Persistence and physical durability remain distinct pending proofs.

## Approved implementation

Keep RFDS v1 payload bytes and existing scene restore/capture ownership. The approved transport is two bounded files with a24-byte RFSG v1 envelope, implemented in `include/rf/checkpoint_file.h`, `src/core/checkpoint_file.c` and `tests/checkpoint_file_tests.c`. No rename, manifest, allocation, dashboard metadata or general save manager is involved. Compilation/tests are coordinated by primary; the author has not run them.

Proposed Xbox paths are `R:\OpenRedFaction\geomod-dev.0` and `.1`, using base path `R:\OpenRedFaction\geomod-dev`. R: is a proposed private process-owned alias to `\Device\Harddisk0\Partition1\`. Mounting, platform flush integration and persistent XEMU verification remain pending. This is a homebrew DEV path, not dashboard-managed title storage.

`rf_checkpoint_file_load` uses one caller-owned110524-byte buffer and a **pure** validation callback; the callback must not restore/publish live terrain. It returns an explicit selection token identifying the newest structurally and semantically valid slot. Both absent returns RF_NOT_FOUND plus an initialized empty token. Both invalid, I/O errors and ambiguous equal generations fail without a usable token. A capacity smaller than110524 fails rather than selecting an older smaller checkpoint.

`rf_checkpoint_file_store` first validates outgoing data and verifies that the selected previous slot still matches its token. It writes only the other slot, flushes/closes, then reopens and compares exact bytes with512-byte readback scratch. No second payload allocation occurs. On success it updates the token; on failure it leaves that token and the previous selected file untouched. Reload before retry after any store failure, since a fully written newer candidate may exist despite a later close/readback error. Concurrent writers are unsupported.

The envelope is six little-endian32-bit words: magic bytes RFSG, version1, nonzero generation, payload length, payload FNV1a, header FNV1a over its first20 bytes. Generation UINT32_MAX rejects the next save instead of wrapping. FNV checks accidental corruption, not authentication. RFDS identity/geometry validation remains caller-owned. Exact raw RFDS bytes, after removing the24-byte envelope, remain comparable to PC/QMP outputs. Each slot uses at most110548 bytes on disk.

## Verified local source evidence

- `src/diagnostic/scene.c:9393` `scene_checkpoint_begin`: builds strong scene identity, loads optional native `D:\geomod-checkpoint.bin`, checks length288..110524, invokes restore and releases its blob. Native absent staged input is optional; PC explicit input failure is fatal.
- `scene.c:9428` `scene_checkpoint_capture`: serializes RFDS from committed terrain/admissions/noise maps/bindings, bounded by110524 bytes. PC writes a temp then C rename; native merely requires `D:\geomod-checkpoint-out.flag` and retains the buffer for QMP. No native HDD write exists.
- `scene.c:13160` restore runs after `scene_terrain_open`, before the first frame. `scene.c:13202` capture runs only on successful stream completion, before scene resources are freed. Atlas completion guards reject incomplete capture. Continue using these exact lifecycle positions.
- Export globals at scene.c569..571 expose status/bytes/FNV/ready and external blob resident/peak. The buffer intentionally survives scene teardown for QMP, and is released at the next begin. Do not free it after a successful HDD write if QMP verification is requested.
- `tools/xemu_render_check.py:212..242` chooses isolated `pacing-base.qcow2` but **always passes `-snapshot`**. Those process writes cannot prove persistence across emulator exits. Current48 comparisons demonstrate RFDS/readback/reload fidelity, not writable save storage.
- `C:/nxdk/lib/nxdk/mount.c` provides `nxIsDriveMounted`, `nxMountDrive`, `nxUnmountDrive` using kernel symbolic links. `nxIsDriveMounted` establishes existence only, not the target. `samples/winapi_drivelist/main.c` explicitly maps E: to Harddisk0 Partition1. Therefore Partition1 mapping is locally documented; this particular project's HDD mount/write behavior still needs execution.
- NXDK `lib/nxdk/automount_d.c` maps D: to the running XBE directory. It does not establish U:/T: or E:. D: in the present disc harness is not the save destination.
- `C:/nxdk/lib/winapi/fileapi.h` exposes CreateFileA, ReadFile, WriteFile, GetFileSizeEx, SetFilePointerEx, CreateDirectoryA, MoveFileA. Existing Xbox code already includes windows.h and xboxkrnl.h, but the project does not currently implement a native writable save adapter.
- `lib/winapi/fileio.c` maps FILE_FLAG_WRITE_THROUGH to FILE_WRITE_THROUGH and synchronous access by default. Use synchronous operations, no OVERLAPPED and no NO_BUFFERING (the latter would impose alignment constraints).
- `lib/winapi/filemanip.c:305` MoveFileA hardcodes `ReplaceIfExists=FALSE`. It cannot replace an existing checkpoint. `SetFileInformationByHandle` does not support Unicode FileRenameInfo. Do not copy the PC rename path and assume overwrite works.
- `lib/xboxkrnl/xboxkrnl.h` exposes NtFlushBuffersFile(handle, IO_STATUS_BLOCK*) and NtSetInformationFile(..., FileRenameInformation), with FILE_RENAME_INFORMATION containing BOOLEAN ReplaceIfExists, HANDLE RootDirectory, ANSI_STRING FileName. No usable FlushFileBuffers declaration was found in the local NXDK WinAPI surface inspected.

## Exact remaining integration touchpoints

1. `src/platform/xbox/checkpoint_storage.c/.h` (pending): mount/path/error adapter only. Add this and the existing new shared utility source to `platforms/xbox/Makefile` when integrating. Keep NT handles out of shared RFDS/GeoMod code.
2. Scene must perform pure validation/probe **before encoding a replacement**. Reuse the single temporary110524-byte caller buffer for slot selection, then release it before normal play/capture; retain only the returned selection token. Do not call `scene_checkpoint_restore` as the selection validator: it publishes state. Extract a genuinely read-only RFDS validation pass or supply one that checks all semantic conditions needed to select a previous slot. This is the key pending integration dependency.
3. For explicit load, take the selected raw RFDS buffer and restore once into the fresh scene at `scene_checkpoint_begin` (after scene_terrain_open, before frames), then release the buffer. For save-only, validate/probe existing slots with the same scene identity without publishing them, preserve the token and discard the probe bytes. Both-invalid must fail rather than overwrite a possibly recoverable file; both-absent permits initial creation.
4. `src/platform/xbox/main.c`, alongside DEV flags around541: proposed `D:\geomod-hdd-load.flag` and `D:\geomod-hdd-save.flag`, requiring dev-room. No flags means no mount or disk mutation. Reject explicit HDD-load combined with staged geomod-checkpoint.bin. HDD-save may coexist with memory-output flag.
5. `scene_checkpoint_capture`: configured HDD-save can request capture without the old output flag. Encode RFDS once, call `rf_checkpoint_file_store` using the retained token and pure validator, then set ready only on success. Retain successful RFDS memory output for existing QMP comparisons. Preserve allocation max110524; the transport adds512 bytes of scratch plus bounded path/header/token locals, not another full buffer.
6. Expose storage diagnostic phase/status/native error/generation/slot/bytes. Distinguish selection, write/readback success and platform durable-flush success. Current utility has only portable fflush/fclose and cannot claim durable hardware publication.

The utility's pure callback contract is intentional: selection must reject a checksum-valid but semantically invalid newer slot so a later save never overwrites the last genuinely usable slot. Do not weaken that contract by installing a callback that merely checks the first four RFDS bytes.

## Mount and lifetime rules

- Initialize once, only when an explicit HDD operation is requested. If R: already exists, fail with an actionable collision diagnostic; do not unmount/repoint an unrelated alias and do not assume its target. A later refinement can query its target, but is unnecessary for the first isolated probe.
- `nxMountDrive('R', "\\Device\\Harddisk0\\Partition1\\")`; record ownership only after success. Do not format or repartition anything.
- Ensure `R:\OpenRedFaction` with CreateDirectoryA. An already-exists error is acceptable only after GetFileAttributesA confirms directory; other errors fail. Read-only load should not create a missing directory, and should report missing checkpoint cleanly.
- Keep alias alive through scene reads/write verification and diagnostics. Close every handle on every branch. Unmount only an alias created by this adapter, after scene operation completion or explicit app teardown. A test harness may leave the process idle for QMP; an idle process must have no open file handles.
- One fixed path is acceptable only because this is an explicit DEV operation with scene identity enforcement. Do not silently use it as an automatic campaign autosave.

## Write/error semantics and pending native flush bridge

The implemented sequence is validation -> verify protected token -> write inactive slot header/payload -> fflush -> fclose -> exact readback -> return new selection token. Previous valid slot is never opened for write. A crash/short write in the inactive slot is rejected by header/length/payload checks on the next load, which may fall back to the protected slot. There is no delete-old or rename window.

A successful portable return proves a closed, readable, byte-matching file; it does not prove platter/power-cut durability. The future Xbox bridge can use NtFlushBuffersFile on a reopened native handle after utility write/readback and before advertising durable-save success. It must report failures and preserve the distinction between portable selection success and native flush status. Exact native sharing/access/cache semantics and whether additional volume flush is needed require local execution; no physical durability guarantee is made here.

Do not add ReplaceIfExists rename to this two-slot flow. Local NXDK rename observations below explain why a portable overwrite-based strategy was rejected; they are evidence, not a competing implementation prescription. Mount/flush behavior is still pending verification.

## Load and malformed-state safety

The adapter validates I/O/length; existing RFDS restore validates identity, geometry/history and atlas metadata. Load only into a fresh scene before frames as now. A restore error exits scene construction and releases that scene; do not try another file against the partially built owner. This matters because core RGCH publication can precede final scene binding checks. It is not an in-place live-load transaction.

Storage implementation must not regenerate random seeds, patch identities, or change the serialized RFDS layout. Exact RFDS bytes remain comparable to PC outputs. Raw RFDS has no full-payload checksum; the new RFSG transport supplies one in its envelope. Identity hashes still validate source/configuration separately. FNV detects accidental changes probabilistically and does not authenticate a save or establish physical durability; do not advertise retail save resilience yet.

## Test plan and harness changes

- First bounded native storage test on a **fresh disposable HDD copy**: mount/create/write/read, then alternate the two slots and verify raw RFDS bytes and generations. Inject truncation/corruption into the newest inactive slot and prove fallback to the protected checkpoint. Record native flush/error status and confirm handles close. No filesystem/kernel assumption becomes verified until this runs.
- Add an explicit persistent-HDD test mode to `tools/xemu_render_check.py` or a small wrapper: copy the existing isolated HDD base once into `artifacts/geomod-hdd/<run>/save-test.qcow2`; two sequential launches reuse that copy **without -snapshot**. Keep default harness behavior unchanged. Never omit -snapshot while pointing at the base or user's normal HDD. Preserve require_no_project_xemu and terminate only the owned process; no second project instance.
- Run A: same deterministic DEV repeated-cut replay as current checkpoint tests, staged HDD-save flag, existing memory-output flag. Check RFDS ready, storage write/readback success, QMP bytes versus PC output and resident/peak<=110524. Save matching ISO/XBE identity, source commit and HDD-copy path in report.
- Exit A, run B with a new process and HDD-load flag but **no staged geomod-checkpoint.bin**. No-fire same-view replay must reproduce physical RGM1, admission count/RNG, atlas audit and upper-world pixels. Then run another deterministic blast and compare against uninterrupted PC control. QMP retained data alone is insufficient; B's load source must be proven HDD by flags/path diagnostics.
- Repeat empty checkpoint, reset-then-empty, repeated edits and orthogonal shallow fixtures. Same-level scene identity mismatch/malformed length/truncated file must fail before frames and leave previous valid slot untouched when testing writes.
- Separate write error cases: missing/unmounted drive, directory-name collision, open target sharing collision and short-write/fault injection. Disk-full behavior should use a bounded fault-injection adapter first, not fill the user's HDD. Physical hardware persistence and power interruption are explicitly later verification.

No frame-by-frame or user-input harness change is required. First success establishes writable DEV destruction save/reload only; player/weapons, dynamic actors, mission events, full game-state saves and dashboard slots remain outside this pass.

## Executed native restart proof

artifacts/geomod-hdd/20260916-092840-146111 passes: separate write550/read32-frame XEMU launches against a private standalone HDD copy, stock67108864 base and0 expansion. Native store/flush selects generation1 slot0; fresh load selects the same9554byte payload and matches current PC baseline byte-for-byte (SHA2568202934d614484ec745dadb3aeb91b0b5e2ea1aca4a22f3a984a798ccef16186). Source HDD hash unchanged; both disc restoration manifests verified. Explicit HDD flags preserve ordinary staged RFDS flow, and owned R: is unmounted while completed-operation diagnostics remain.18 native-adapter host failures and four relevant CTests pass.

Current PC baseline is artifacts/hdd-baseline-092804 (older overwrite attempt failed as documented PC rename policy; new paths succeeded). This proves DEV destruction persistence and native kernel flush, not full game state, physical power-loss durability, or native corrupt-newer-slot fallback. No framebuffer acceptance claim applies to this memory/file test.
