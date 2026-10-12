# Confirmed Xbox dashboard exit

Source-written October 12, 2026, as a successor to the accepted Standard/Southpaw
controller-layout patch on baseline `911de963c2411183743b6b6151ed0c7a89d1828f`.
The frozen 18:00 checkout and runner are untouched. Compilation, navigation,
native storage-failure behavior and dashboard launch remain unverified. The
parent owns integration and the next hourly stock64 batch. No build, test,
fixture, asset import, emulator, HDD operation, dashboard launch, reboot,
screenshot, upload, commit or cleanup was performed for this source slice.

## Tester flow and grounded gap

The preceding tester frontend had New Game, Options and Load Game, with no
dashboard action. End Session returned to that root; the process otherwise
stayed in its terminal sleep. This patch appends Exit to Dashboard as root row 3.
Rows 0 through 2, difficulty selection, Load Game and all pause-menu actions keep
their indices and actions. There is no new in-game teardown path or exit chord.

Exit opens a two-row confirmation with Cancel selected. Only a fresh A edge on
the explicitly selected second row requests exit. B or Start cancels; A on
Cancel returns to root row 3. A disconnected/replaced controller disarms the
second row, and existing attach/release suppression still owns every input
edge. Entering the page, holding A, and navigation cannot launch the dashboard.
The page warns that unsaved settings will be discarded; Exit never autosaves.

The same action is available at cold idle startup and after the ordinary End
Session has completed full scene/audio/render retirement and successful
session reset. A previous terminal session error continues to disable all
actions. Existing End Session progress-loss confirmation remains unchanged.

## Launch and full-idle admission

The frontend first completes its procedural HUD draw and closes its controller
through the existing input owner. Only then does the main tester loop call the
single dashboard helper. Its read-only checks reject:

- Live resident world, collision, movers, materials, lightmaps or preview owners
- Existing shared scene retirement faults, actor/model/collision borrowers,
  retained checkpoint buffers, queued transfers/save/load work and callbacks
- An owned or uncertain native checkpoint handle/alias, or uncertain portable
  checkpoint close, including evidence whose original caller was stack-local
- Locked controller-settings storage or a still-owned settings handle/alias
- Open/paused native audio, remaining voices, an incomplete close or the existing
  audio stop-timeout fault
- Open platform input or an SDL game-controller subsystem still initialized
- Renderer world/model/texture/HUD borrowers, incomplete command scopes or a
  retained draw-audit buffer; the existing process-owned pbkit device receives
  one idle wait bounded to 1000 ms, with RF_IO on timeout and no owner release

If renderer admission fails, the caller records the error and terminally stops
without retrying a GPU wait through an error-screen redraw. Other errors can
still use the ordinary error screen. The 1000 ms bound is an explicit port
policy; existing rendering and retirement fences are unchanged.

These checks do not end a session, retry cleanup, clear faults, close live
owners, change saves, or detach callbacks to manufacture idle state. Ordinary
End Session and session reset remain the only path from gameplay to this idle
frontend. The process-owned framebuffer/device remains available for an error.

Installed official NXDK `lib/hal/xbox.c` implements `XLaunchXBE(NULL)` by selecting
`LDT_LAUNCH_DASHBOARD`, clearing its launch-data page and invoking the kernel's
`HalQuickRebootRoutine`. `lib/hal/xbox.h` says a failed launch can return. The
patch calls that supported API once, without a guessed dashboard path or a
direct firmware fallback. Any return is `RF_IO`, shown as Dashboard Exit Stopped
and terminally held. No retry, successful-exit counter or dashboard-success claim
is synthesized. Native dashboard installation/firmware behavior remains untested.

## Bounded storage-retirement prerequisite

The existing session status was insufficient to certify no orphaned native
storage. A recoverable failed save could later reach a clean idle menu:

1. `src/core/checkpoint_file.c` could hide a failed `fclose` behind earlier
   `RF_FORMAT`/`RF_RANGE`, including its empty-token probe. Its three close sites
   now pass through a process-lifetime uncertain-close latch. Close failure
   takes `RF_IO` precedence so corrupt-slot fallback cannot conceal it. All
   later portable load/store calls refuse before opening another file. There
   is no reset or retry of a possibly invalidated `FILE *`.
2. `src/platform/xbox/checkpoint_storage.c` used a stack-local native flush
   handle. Its failed close could be obscured by an earlier flush error and the
   handle was then lost. Directory/flush handles now use one static owner and
   checked `NtClose`; failure retains the exact handle, latches uncertainty and
   takes failure precedence before any success can be published.
3. Native alias ownership now survives independently of a caller's stack.
   The one persistent owner identity is only compared, never dereferenced.
   A failed unmount latches before the caller can leave scope. Native
   open/load/store and later close attempts reject uncertainty; no uncertain
   handle is retried and its R: alias is not removed. Normal successful close
   clears the identity and still permits the next ordinary operation.
4. The old `nxIsDriveMounted` probe opened a symbolic-link handle and ignored
   its `NtClose` result. Direct `nxMountDrive` uses atomic `IoCreateSymbolicLink`
   rejection of an existing alias, avoiding that probe handle entirely. It
   never borrows, replaces or unmounts another owner's R:.
5. Installed NXDK `CreateDirectoryA` does report failed `NtClose`, but its local
   handle is lost. The preceding checkpoint adapter returned a save error yet
   could later unmount/reopen. A directly owned `NtCreateFile` with `FILE_OPEN_IF`
   and `FILE_DIRECTORY_FILE`, followed by checked close, preserves the same
   create-or-existing-directory policy while retaining that failed owner.

There are no changes to save bytes, paths, generation/slot choice, payload
validation, prior-slot protection, native flush ordering or successful
publication. Known validation/read/write failures whose handles close normally
retain their existing retry/fallback behavior. Actual uncertain retirement is
stricter: it remains blocked until process restart and cannot be cleared by a
menu/session reset or by replacing diagnostic arrays. No data is repaired,
deleted or rewritten to get around that block. Settings RFCS1/RFCS2 and gameplay
codecs are untouched by this successor.

Primary sources inspected read-only: installed NXDK `lib/hal/xbox.c/.h`,
`lib/nxdk/mount.c`, `lib/winapi/filemanip.c`, `lib/winapi/handleapi.c`,
`lib/pdclib/platform/xbox/functions/_PDCLIB/_PDCLIB_close.c`, and
`lib/xboxkrnl/xboxkrnl.h`. No third-party implementation was copied.

## Integration and proportional verification

The patch depends on controller-layout patch SHA-256
`953241dc1102f848f819f1e00ac291acc5ff59a6ea62e8d5620868879eb55ca1`.
Apply that predecessor first, then this successor's bounded diff. Do not replace
newer parent files wholesale or overwrite the parent's current TO-DO history.

Code paths: checkpoint_file.c/h; checkpoint_storage.c/h; main.c; input.c/h;
audio.c/h; settings_storage.c/h; renderer.c/h; scene.c; scene_preview.h; and
scene_session_menu.inc. Existing object lists cover these files. Added state is
bounded static ownership evidence and small local values, with no new heap
buffer, asset, image, external dependency or save-record field.

The unchanged frozen 18:00 observer uses the preceding three-row root. This
successor wraps Up from root 0 to Exit 3; its later observer must use the actual
four-row navigation. Existing root 0/1/2 still identify New Game/Options/Load,
and frontend page 4 now identifies exit confirmation. The existing layout
predecessor also requires its seven-row Options/four-word settings contract.

Suggested next parent-owned batch: compile stock64, check cold and post-End
Session confirmation/cancel plus held input/disconnect suppression, then only
if separately authorized perform one real dashboard launch on a suitable
disposable tester environment. A bounded failure check should show uncertain
close/unmount blocks later storage acquisition and exit with prior data kept.
The new idle-wait timeout must retain a terminal failure without a second GPU
wait or dashboard call; this behavior remains source-only and unverified.
No protected saved HDD, campaign traversal or new per-slice fixture is required
or authorized by this document. These are pending checks, not executed results.
