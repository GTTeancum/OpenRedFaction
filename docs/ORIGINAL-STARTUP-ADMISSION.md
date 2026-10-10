# Original L1S1 startup admission

Status: source-written only. No helper execution, test, build or emulator run
was performed while preparing this helper. The parent reviews it before the
single scheduled hourly Xbox batch.

## Purpose and boundary

`tools/xemu_original_startup_admission.py` checks one original L1S1 startup on
stock 64 MiB. Commit 686307df introduced unconditional weapon-preference loading
inside `campaign_clutter_open`; this check can catch an admission failure even
without selecting or firing a grenade. The subsequent unarmed-pickup work does
not require changing the spawn, inventory or original level for this check.

The guest consumes exactly 120 all-zero RFI6 input records at the authored spawn.
There is no altered archive, gameplay fixture, forced event, inventory grant,
save/load flag, audio-output flag, host input, image/audio capture, world-payload
capture or FPS measurement. Ordinary authored startup callbacks may still run.
The result establishes startup/table admission only, not unarmed acquisition,
grenade replacement selection, gameplay, audio playback, visuals or performance.

## Parent invocation

After reviewing/committing the helper and completing the one successful hourly
Xbox build, create a manifest with `status: PASS_XBOX_BUILD`, the exact current
`source_commit`, and SHA256 fields `xbe` and `map` for
`build/xbox/disc/default.xbe` and `build/xbox/main.map`. Both source and build
must match at launch; a failed build or stale executable cannot be substituted.

From the repository root, after sourcing the normal cloud toolchain environment:

```sh
export RF_XEMU_ROOT=/workspace/shared/xemu-inputs
export RF_XEMU_BINARY=/workspace/shared/xemu/squashfs-root/usr/bin/xemu
export SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1
python3 tools/xemu_original_startup_admission.py --parent-hourly-batch \
  --consumer-build /workspace/shared/rf-2200-build.json \
  --out /workspace/shared/rf-startup-admission-20261009-2200 --seconds 600
```

The output directory must be new and outside the repository. The helper never
compiles or calls make. It packages the already-built XBE directly with
extract-xiso, then invokes the existing native runner once. Its stock 64 MiB
command disables host input/network/audio output and uses `-snapshot` with the
owned standalone `local/xemu-harness/pacing-base.qcow2`. An existing project
session is left untouched. There is no automatic retry.

## Acceptance and evidence

The result requires terminal phase 5, 120 completed replay frames, no death or
respawn, no level transition, available memory, and the exact original spawn
and basis. Original start words come from the read-only installed L1S1 entry;
every staged original archive is hash-matched to `Installed_Game`.

The retained four-word `rf_scene_weapon_supply` must contain a valid nonempty
catalog, primary count and retained byte size. In `scene.c`, these words are
published only after `rf_weapon_supply_load`, `scene_grenade_empty_load` and
the extra-weapon load succeed. Terminal phase 5 separately establishes that
the rest of scene initialization and the bounded loop completed. The private
preference-ready state is deliberately not checked because cleanup resets it.

All shared-runner and extra symbol lookups use the existing exact-token map
resolver, temporarily scoped to this invocation. A borrowed session lock spans
packaging, the owned emulator and restoration. The helper restores the prior
disc selectors/names and ISO; it verifies XBE/map, original archives, fixed
geometry data and base HDD unchanged. Unexpected XBE/map changes are restored
from the pinned copies and reported as failure, never accepted silently.

`verification.json` records the consumer manifest, hashes, attempt count,
acceptance and restoration checks. `native/result.json`, `native-result.json`
when available, and native stdout/stderr retain raw guest evidence. Acceptance
checks run only after saving the returned guest result. Failed/partial evidence
is not overwritten by a retry; timeouts retain their logs and wrapper error.
Any guest failure, failed acceptance or restoration problem is `CHECK_FAILED`.
The parent owns cleanup and any later decision to investigate or rerun.
