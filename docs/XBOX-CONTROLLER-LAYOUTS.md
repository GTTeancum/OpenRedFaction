# Xbox controller stick layouts

Source-written October 12, 2026, against clean
911de963c2411183743b6b6151ed0c7a89d1828f. Staged outside the repository and
the frozen 18:00 candidate. Compilation, runtime, persistence and physical
controller feel remain unverified; the parent owns integration and the single
hourly stock64 Xbox batch. No build, test, import, fixture, XEMU launch, disk
operation, image, upload, commit or cleanup was performed by this lane.

## Grounded gap and first-playable choice

The current Xbox input adapter hardwires left-stick movement and right-stick
look. Its existing Options page can change sensitivity, invert Y and radial
deadzone but cannot select a controller layout. The tester-readiness document
lists remapping as a deferred gap. This pass supplies one useful bounded choice
through that existing Options system:

- Standard: left stick moves; right stick looks.
- Southpaw: right stick moves; left stick looks.

These are explicit port-owned analog-stick policies. No original executable,
console manual or retail layout evidence is claimed. There is no arbitrary
button editor, alternate menu map, input framework or PC controller change.

## Tester flow

Options is still reachable from startup and pause. Existing indices 0 through 5
remain sensitivity, invert Y, deadzone, Reset Defaults, Back and Save Settings.
Stick Layout is appended as index 6. Left, Right or a fresh A press toggles the
two layouts. The screen names the selected layout, spells out which stick moves
and looks, and explicitly says that stick clicks and buttons remain unchanged.

All changes apply immediately to the input owner. Reset Defaults restores
Standard alongside sensitivity 100, invert off and deadzone 18; it still does
not save. Only fresh A on Save Settings writes. Saved/changed comparisons now
include layout. Reconnect, section transitions, End Session and New Game retain
the selection in process; successful normal tester startup settings restore
also restores a saved Southpaw layout. Diagnostic/replay boots retain the
existing no-automatic-settings-load policy.

The seven rows use the same allocation-free procedural HUD and unchanged
renderer calls. A separate draw-only flag carries Southpaw outside the existing
storage-notice bit mask. The compact rows end above the status, mapping and
navigation text; all text remains inside the existing 640 by 480 viewport by
source-coordinate inspection. No image or rendered-UI result is claimed.

## Input ownership and unchanged bindings

The physical stick pair selection happens after existing hotplug/action
suppression and pause/save-load early returns, and before radial deadzone,
look sensitivity, invert Y, saturation and Fine Aim. Both whole X/Y pairs are swapped, with the
same existing sign conventions. Movement never receives look sensitivity or
inversion. The right-click Fine Aim reduction still halves the final normalized
look command, now from whichever stick is assigned to look. Ordinary on-foot
and mounted consumers receive the same shared command fields as before.

Physical bindings remain A Jump, B Crouch, X Use, Y Reload, RT Fire, LT Alternate,
D-pad/shoulders weapon cycling, right-stick click Fine Aim and left-stick click
Holster. Therefore Southpaw's right movement stick still carries Fine Aim and
its left look stick still carries Holster; the choice swaps axes only. All
authored control-hint tokens remain accurate without dynamic rewriting.
Start pause/resume, Back+Start End Session, Back+Y Save, Back+X Load and fixed
menu controls retain their existing priorities and release edges. In particular,
the action bit mask and attached/pause release latch are not remapped. No extra
USB pump, synthetic button edge, shared scene input/replay field, simulation
clock or gameplay checkpoint change is added.

## RFCS1 and RFCS2 compatibility

The existing two-slot protocol and native ownership are described in
[controller settings persistence](XBOX-CONTROLLER-SETTINGS-PERSISTENCE.md).
This pass extends only the 36-byte controller-settings record; paths, size,
generation rules and FNV-1a32 coverage over bytes 0 through 31 stay unchanged.

- RFCS1 still requires version 1, length 36 and a zero reserved word at bytes
  28 through 31. A valid RFCS1 record explicitly decodes as Standard.
- RFCS2 requires version 2 and length 36; bytes 28 through 31 are the layout
  enum, exactly 0 for Standard or 1 for Southpaw. Any other value rejects.
- Both formats retain the strict sensitivity steps/range, boolean, deadzone,
  nonzero generation, slot parity and checksum admission. Malformed stored
  layouts never reach the input setter's normalization path.
- The writer emits RFCS1 when layout is Standard and RFCS2 when it is Southpaw.
  At the same three control values and generation, Standard is byte-identical
  to the previous RFCS1 writer. RFCS2 Standard input is accepted for a complete
  two-value enum; a subsequent Standard Save writes canonical RFCS1.
- Changing back to Standard never edits the previously highest-generation
  slot. Ordinary validated generation advancement writes only the absent or
  older slot. Mixed RFCS1/RFCS2 neighbors are valid only with the same existing
  adjacent-generation requirement. Generation overflow still refuses.

An older RFCS1-only executable rejects an RFCS2 slot as unknown. It may recover
a different valid RFCS1 slot using its existing visible recovery policy, or
retain defaults if neither slot is understood. In either case, an existing
unknown RFCS2 file blocks Save, so the older executable cannot silently
overwrite the layout it cannot understand. The new reader likewise continues
rejecting versions other than 1 or 2 and blocking writes when an existing slot
is invalid or unknown.

No changes are made to slot selection, inaccessible-file handling, directory
creation, exclusive target revalidation, newest-slot protection, write/flush/
reread comparison, checked native close, alias release, uncertain-owner latch
or publish-after-success ordering. The change adds one uint32 option member
and small bounded local format values, with no heap or disk-size increase.
RFWM, campaign saves and all other checkpoint codecs are untouched.

## Integration and next proportional batch

Code changes are limited to input.c/h, settings_storage.c/h, main.c,
include/rf/scene_preview.h and scene_session_menu.inc. The input-options object
and saved-baseline objects are now four uint32 words; existing three-word
observations still describe only sensitivity/invert/deadzone and cannot
establish the selected layout. Up from row 0 now wraps to Stick Layout 6,
whereas the preceding six-row page wrapped to Save Settings 5. The frozen
18:00 helper must remain pinned to its original source; any later observer
must use this successor's actual option width and navigation. The renderer
signatures and frontend/session diagnostics retain their contracts. No test
or harness file is changed or created.

Suggested bounded checks in the parent-owned hourly batch: compile stock64;
select Southpaw and confirm swapped analog movement/look with current
sensitivity/invert/Fine Aim, fixed buttons/chords and pause release ownership;
Reset Defaults restores Standard; on an authorized disposable settings disk,
explicitly Save Southpaw, cold boot and observe the restored layout. Existing
RFCS1 records should load Standard without normalization. Unknown formats and
invalid RFCS2 enum values must remain read-rejected/write-blocking, with the
prior valid record preserved. These are pending checks, not executed results;
campaign traversal is unnecessary and tester feedback owns feel/refinement.
