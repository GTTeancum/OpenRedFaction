# Removed Auto Turret ordinary save check

`tools/xemu_auto_turret_removed_save.py` is prepared for parent-owned serial Xbox execution. It reuses the real L3S2 Auto Turret base UID1994 in CTF06 and the existing process-local Remove_Object setup: normal activation/fire, removal at frame60, save at75, fresh load for12 neutral frames. It adds no artificial head record or damage injection. The load disc removes campaign-setup.bin so removal cannot be replayed to manufacture the expected state.

The saved RFWC component directory must contain RFTU3 with one224-byte generated row and no inner vehicle payload. Assertions cover stable base UID/child role, removal_reason1/base.retired1, hidden noncolliding head with60 health/dead0, cleared target/cadence and finite independent pose. The actual RFNC10 row must separately retain the authored base with retired1, health0 and no synthetic death pose. Exact head basis, angles and position must match the immutable restore probe.

The source retirement snapshot precedes base unregister and therefore records base/head registrations1/1; the ordinary actor-retirement completion count verifies the later unregister. The fresh-load retirement snapshot runs after NPC publication and must record registrations0/1, hidden head, link-1, unchanged head health and dead0. Fresh-load removal counters, setup dispatch, turret shots, target acquisitions, coupled-death dispatch and death-effect counters must remain zero. This verifies restoration of terminal state without replaying removal or generating a death. The fixture also requires successful ordinary checkpoint telemetry, a living player and available stock64MiB pages.

Source-only preparation; native result pending. Head-only removal persistence, visual disappearance, audible output and broader campaign behavior are outside this check. No images, desktop input, PC runtime or campaign route is used. The harness restores all prior disc controls and rebuilds the original disc in finally. `--validate-existing <artifact-folder>` only reads retained evidence and leaves the original report untouched.

## Native result (2026-09-30)

`artifacts/xemu/auto-turret-removed-save-20260930-140833/report.json` passed on stock64MiB: one authored base retirement after five earlier head shots, a real RFTU3 224-byte row and RFNC retired fact, fresh-load restoration of the hidden detached head, zero loaded shots or replayed death/removal effects, and3357 free pages. Fixture disc inputs restored. Head-only removal remains unverified.
