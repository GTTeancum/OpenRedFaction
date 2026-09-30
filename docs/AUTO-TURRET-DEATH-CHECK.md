# Generated Auto Turret base-death check

`tools/xemu_auto_turret_death.py` reuses the passed UID1994 Auto Turret fixture. It adds a harmless zero-delay Delay event at frame0 and a zero-delay Slay_Object event linked to the authored base at frame60, using existing `campaign-setup.bin` dispatch. These are explicitly synthetic test events; entity class/vitals and runtime head creation are unchanged. No head RFL record or direct damage injection is added.

The120-frame neutral run must activate and fire through the generated head before Slay. The ordinary Slay/NPC death path must produce exactly one base-origin coupled-death callback, no head-origin duplicate callback, head health0/dead state and a detached host link. A live observation after frame75 and final counters must show unchanged shot counts since death and exactly one terminal head-effect transition without duplicate presentation requests. Head audio/visual effects may legitimately be absent in its class. This checks scheduling/state, not visible appearance or audible output.

The adapter exposes `rf_scene_turret_generated_death_probe[10]` on successful callback: base UID, generated role, reason, base health bits, head health bits, head dead state, head linked handle, global turret shots at death, global turret death-effect transitions at death, zero status. The isolated fixture has one head, so those global counts identify it unambiguously. Counter dependencies use explicit extern declarations. Parent should zero this array at level initialization; do not clear it at teardown before collecting results. Errors are still propagated and reported by existing generated stats; an absent successful callback cannot be mistaken for a populated terminal probe.

Prepared source only; no new build/runtime performed by the helper. Head-to-base1000 damage, pure removal and ordinary saves remain outside this bounded fixture. The original live generated-head check passed separately at `artifacts/xemu/auto-turret-20260930-134316`.

## Native result (2026-09-30)

`artifacts/xemu/auto-turret-death-20260930-135335/report.json` passed on stock64MiB: five head shots before scripted Slay at frame60, one base-origin coupled death, zero head-origin recursion, head health0/dead/detached, no shots after death, and3516 free pages. Disc inputs restored. This does not verify head-to-base damage or visual/audio output.
