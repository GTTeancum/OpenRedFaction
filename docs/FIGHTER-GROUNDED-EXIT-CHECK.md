# Parent-only Fighter grounded-exit check

Prepared 2026-10-09 for the 04:00 UTC consolidated Xbox batch. This harness is
source-written and unverified. No helper build, test, emulator launch, cleanup,
capture or image generation was performed.

## Run after the parent Xbox build

From the repository root:

```sh
python tools/xemu_fighter_grounded_exit.py --parent-hourly-batch
```

Optional `--out <new-directory>` selects the evidence directory; `--seconds`
defaults to 600. The command packages the existing XBE with extract-xiso only.
It never invokes make, a compiler, `build-xbox.sh`, or a restore build. It refuses
to stage if the freshly built map lacks either new exit/ground probe.

Defaults use the restored `/workspace/shared/xemu-inputs` firmware,
`/workspace/shared/xemu/squashfs-root/usr/bin/xemu`, and this checkout's
`local/xemu-harness/pacing-base.qcow2`. The HDD must be standalone QCOW2 and is
opened only with XEMU `-snapshot`. `RF_XEMU_ROOT`, `RF_XEMU_BINARY`, and
`RF_EXTRACT_XISO` can override the corresponding parent-owned paths.

## One unchanged original scene

- Original `Installed_Game/levels3.vpp/L18S2.rfl`, Fighter UID10066.
- The entire staged levels3 archive must hash-identically to the original.
- Existing process-local `campaign-actor.bin` / `rf_scene_stage_actor` places
  the player near the original host. No level/entity/geometry edits are made.
- Ordinary replay: Use12; crouch180 through519; release520 through599; Use600;
  neutral continuation through639; terminal presented-frame count640.
- Board sample at or after40 and before180; ground sample at or after540 and
  before600. The runner records the actual sampled frames.
- The earlier primary/secondary fire is deliberately omitted. This is a
  grounded-exit-only check, not a repeated combat or performance measurement.
- No campaign traversal, inventory grant, save/load, host input or PC runtime.

## Required evidence

The checker requires the actual original Fighter selection, one successful
ordinary board, its authored hover pose, more than60 units of real descent,
unchanged horizontal host position, and stopped vertical motion before Use600.
`rf_scene_vehicle_ground_probe[20]` must record a genuine downward host sweep
with walkable world/mover contact, with frame and full position matching this
Fighter descent. This records the existing query before collision response or
tangential requery; the checker therefore also requires the accepted host pose
and stopped vertical velocity. The probe is shared host-resolver telemetry; it
is not player support evidence.

`rf_scene_vehicle_exit_probe[80]` must show one accepted ordinary exit at600,
an accepted full standing-body support sweep, host-hull exclusion, and the
separate actual-body final-placement provider returning clear. The terminal
native state must show one board, one exit, a living player and host, no death
or respawn, no player health loss, no weapon launch/ammunition debit, and no
runtime error. Ordinary player ground contact must be reacquired after release.

HUD checks require actual original-asset admission, submitted Fighter cockpit
faces before release, no HUD fallback/error, and a later on-foot art-HUD frame
with its vehicle fields cleared. Cockpit telemetry deliberately persists after
exit, so its retained nonzero values are not interpreted as a still-drawn
cockpit. This is native state/submission evidence, not visual parity.

## Preservation and failures

One SessionLock covers staging, packaging, the native run and restoration. The
existing native runner borrows that lock, and its file descriptor is inherited
by its owned XEMU child. Existing sessions are refused and left untouched.

Before launch the runner copies the exact XBE and map to `tested-default.xbe`
and `tested-main.map`, records their SHA-256 values, and saves every affected
disc flag in `disc-restore.json`. It temporarily moves the original ISO aside;
the original bytes are moved back afterward without a restore build. Original
flags, XBE/map hashes and scene hash are verified after restoration.

The compact evidence directory contains `verification.json`, `recipe.json`,
live sample JSON, raw native `result.json`, logs, exact XBE/map copies, and
`exit-rejection-telemetry.json`. The latter decodes all four candidate rows,
including lateral-path, absent/invalid support, host-overlap and final-placement
rejections. A failed exit stays a failed check. No rejection stage is assumed
from the earlier failed run, and no extra retry or changed route runs
automatically. A runtime error after terminal telemetry was written preserves
that raw evidence even if a live sample was missed.

Only the parent owns generated-artifact cleanup after the hourly batch. This
runner restores/removes its own temporary ISO; it does not invoke project
cleanup or delete other run evidence.
