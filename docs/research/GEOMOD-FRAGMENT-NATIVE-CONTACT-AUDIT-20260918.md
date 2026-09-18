# Native narrow-surface contact audit

Added the opt-in `--fragment-contact-test` mode to `tools/xemu_render_check.py`. This closes the native numerical-coverage gap recorded in the static and mover contact notes: the production reciprocal query functions now execute their narrow-surface fixtures in the NXDK build, and the harness reads their complete64-word result directly from guest memory.

## Shared fixture and controls

`src/diagnostic/scene_fragment_contact_audit.inc` implements `rf_scene_fragment_contact_check`, shared by PC and Xbox and also called from the existing scene unit test. Its seven result records cover:

1. The demonstrated approximation gap: eight sphere sweeps and four corner rays all miss the finite0.04-unit patch.
2. The production static-world vertex/face path catches the patch at fraction0.25, with the expected upward normal, source face17 and material3.
3. A translated committed mover catches it at0.25 and preserves handle77, texture11, material7 and velocity.
4. A90-degree rotated mover/fragment pair catches it at0.25 with a negative-X response normal, ignoring deliberately wrong ray-facing poses.
5. Updating the committed mover pose changes the contact fraction to0.125.
6. Disabled-mover rejection preserves the prior result.
7. A material callback failure returns the expected I/O error and preserves the prior result and miss flag.

The fixture owns its small collision tree and temporary scene wrapper, frees both on every exit, and never installs them into the loaded level. It publishes only `rf_scene_fragment_contact_audit[64]`; header words are version1, completed record count7, status0 and completion1. It is a numerical native fixture, not a visible narrow platform inserted into the room and not a player-interactive mover demonstration.

PC activation uses `RF_REPLAY_FRAGMENT_CONTACT_TEST`; Xbox activation uses the isolated disc flag `fragment-contact-test.flag`. Both require DEV mode. The harness saves/restores that flag along with the rest of its disc state, reads all64 words, requires exact PC/Xbox equality, then independently validates case counts, expected fractions, normals, ownership and rejection records. A non-DEV CLI request rejects before emulator launch (`artifacts/fragment-native-fixture-guard.log`). The default game path does not run the fixture.

## Results

All123 PC tests pass (`artifacts/fragment-native-fixture-all-tests.log`). Native run `artifacts/xemu/render-20260918-023556` executes the fixture and the ordinary600-frame east beam first-cut replay on stock64MiB. All78 harness checks pass, including all64 exact contact-fixture words. The native endpoint has3329freepages (about13MiB), no added memory, and the ordinary5300-byte checkpoint matches PC and the previous fixture-off first-cut checkpoint exactly (SHAd27cfa99dae3f1595e695e85f32b777a8f8b856017c85b944cab34c241601aba).

The framebuffer is byte-identical to the previously inspected `render-20260918-020011` capture. Thus the expected level, destruction, rubble, weapon and HUD content is preserved; no duplicate image is posted or uploaded. The disc restore succeeds, the fixture flag was originally absent, and the harness closes its owned emulator.

This validates the new narrow static/committed-mover contact cases directly on Xbox. It does not complete edge/edge coverage, continuous relative mover motion, sleeping-fragment wake, carrying/crushing, a visible authored interaction demonstration, or a many-fragment performance budget. Those remain open gameplay work.
