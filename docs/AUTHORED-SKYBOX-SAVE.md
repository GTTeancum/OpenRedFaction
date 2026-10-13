# Authored skybox checkpoint continuation

Source-written against1cdb9f81 with the public sky owner contract from Codex
commit48bbcdf477d22bbbb179c66e82156dc7db6997b6. This adapter requires that
renderer/event implementation in the same integration rollup. No helper
build, test, runtime, image or saved-HDD work was performed. Parent owns the
hourly Xbox compile/package and short starts/runs smoke; functional coverage
belongs to testers.

## RFEN9 wire ownership

The existing identity-bound RF_WORLD_ENVIRONMENT component now emits RFEN9.
It retains RFEN8's force/nav/cutscene/form records, monitor binding records,
eight-byte replay record,16-byte explicit support-point record and variable
monitor-cycle extension. A32-byte sky record follows the entire monitor
extension. The monitor extension's stored length excludes this new record.
All current saves use this grammar, including zero-monitor/no-sky scenes.
The existing RFEN1–8 grammars remain readable.

The sky record consists of eight little-endian32-bit words:

1. Owner payload version, exactly1.
2. Presence,0 or1. This binds admitted authored sky geometry/materials, not a
   temporary rendering/memory gate and not rotation activity.
3. Exact target level identity.
4. Selected authored sky-room UID, or UINT32_MAX when absent.
5. Exact admitted sky-source identity, or zero when absent.
6. ASCII x, y or z.
7. IEEE binary32 rate in degrees per second, finite[-3600,3600].
8. IEEE binary32 accumulated phase in degrees, finite[0,360).

`rf_scene_skybox_validate` owns these domains and binds presence/level/room/
source identity against the actual prepared target. The surrounding RFEN
identity and checksum remain mandatory. The adapter also joins the five
identity words to its captured target snapshot. A present saved owner cannot
silently bind to missing or different optional geometry/materials on load.
Canonical absence retains the actual level identity with room UINT32_MAX,
source0, axis x and zero rate/phase. Positive rate enables rotation;
nonpositive rates preserve phase but render the unrotated sky. OFF remains
the renderer owner's no-op. No extra active flag, absolute clock, matrix,
handle, texture index, diagnostic counter or pointer is serialized.

## Capture, staging and final-success publication

Capture samples the current game clock into a temporary state without
advancing live phase/clock, drawing, allocating or dispatching an event.
The owner increments its capture diagnostic only. Save capture and validation
happen before writing the RFEN record; phase normalization uses the temporary
value. Clock discontinuities outside the owner's admitted half-period window
refuse capture rather than inventing elapsed rotation.

The environment's single budgeted allocation stores one32-byte before snapshot
and one32-byte candidate. Preparation decodes/validates into the candidate and
never calls restore. The existing environment validation recaptures the live
logical state and compares fields, including raw float bits, with the snapshot.
It also revalidates the candidate. No ABI padding, render cache, counter or
GPU state participates. Capture validates `campaign_blackout_now` in the
inclusive[0,1072800000] millisecond domain before publication; that is the
renderer owner's clock. The environment prepare function's separate literal16
blackout/cutscene restore time is not used for sky rotation.

The existing early `scene_world_environment_assign` is unchanged. Sky restore
runs in the new `scene_world_environment_finish` only under
`!status && world_published`, after final native storage close and all work
whose return value can change load status, immediately before disposing the
environment stage. The owner/clock remain live and unchanged through this
paused, non-ticking transaction. The public `void rf_scene_skybox_restore`
performs assignment-only phase/configuration publication and clock rebasing;
there is no validation, allocation, callback or fallible operation at commit.
No later load cleanup changes status.

Rejected input, failed candidate admission, fallible world publication, or
storage-close failure frees the stage without changing the sky timeline at
load entry. This is the bounded sky adapter's guarantee. The outer QuickLoad
path's fresh-scene construction policy is unchanged; it does not promise to
resurrect an outgoing scene after unrelated construction failure.

## Compatibility and section policy

RFEN1–8 did not save applied sky state. Their successful restore uses the actual
prepared target's admitted presence/identity and the owner-approved fresh
baseline x/rate0/phase0. It never inherits the current live angle/rate or infers
a past command from event order. Those old files cannot recover sky history
they did not encode. Saved pending type59 commands still belong to the event
scheduler and execute only at the ordinary next dispatch.

Type59's external checkpoint classification changes from UNIMPLEMENTED to
VISUAL alongside this adapter. RFEC retains its existing common scheduler
record; restoring applied RFEN state never replays a Skybox_State event.
No RFNC, RFAP, RFCH, RFWC directory, player carry or actor carry format changes.

A normal section entry/restart follows the renderer's x/0/0 reset and ordinary
startup events. The sky is section-local. RFCH does not retain previously
applied per-section sky effects, so exact rotation history across section
revisits is not implemented by this slice. Same-level explicit RFEN9 save/load
retains the accepted axis/rate/phase exactly at its captured endpoint.

## Stock64 resource bounds

The sky payload adds32 bytes versus RFEN8,44 versus zero-monitor RFEN7, or56
versus zero-monitor RFEN6. The latter differences include RFEN8's explicit
support record and empty eight-byte monitor extension. Two portable snapshots
add64 bytes to the environment stage through its existing `sizeof` accounting.
Encoding/validation use a single32-byte automatic temporary. No extra heap
owner, history table, texture, image, GPU allocation or per-frame work is added.
The renderer's separately documented20-KiB metadata/scratch allowance and
borrowed resident textures remain unchanged.

The110524-byte whole-file cap and two-MiB composed restore budget are unchanged.
Existing capacity failures remain explicit. The adapter never truncates,
downgrades or omits sky/monitor state to make a save fit.

Implementation is source-reviewed only and remains uncompiled/runtime-unverified
until the parent rollup. No per-slice test or gameplay fixture was added.
