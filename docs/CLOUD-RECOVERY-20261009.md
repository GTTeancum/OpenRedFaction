# Cloud recovery, 2026-10-09

At 02:28 UTC the workspace was replaced. Published source `179c75582a75c3fd437e5c1e31549e98c3652088` was cloned again. Vehicle HUD, Jeep view, silencer, voice-over and music edits were reconstructed from retained authoring patches and integrated as a new recovery commit. This does not assert the lost commit objects or exact former tree hashes survived.

The recovered source compiled and linked an Xbox executable at 02:38 UTC. No original game assets were needed for this source-only compilation; no recovered runtime test has run. Original input materialization returned HTTP 403, and the owner was asked for a fresh upload or private Drive copy.

## Reproducible tooling

- NXDK: `14d5ee97e73347c973f1f57b68b79ec08c9e77f2`, recursive official XboxDev repository checkout.
- Clang/LLVM/lld: Debian 19.1.7, official Debian trixie packages downloaded and extracted into a workspace prefix. No system-wide package installation succeeded or was required.
- Nxdk-audio/DSP: repository `tools/build_apu_probe.py --backend-only` pins the audio source and checks the assembler archive hash.
- XEMU: official v0.8.136 x86_64 AppImage extracted into the cloud workspace. Equivalence to the pre-reset emulator build is not established, so future FPS must not silently be treated as the same benchmark.
- Source-only build: create `build/xbox/geomod-profile.stamp` containing `0`, prepare the APU backend, then run the Xbox Makefile with `GEN_XISO=`. This does not prepare playable disc assets.

## Evidence retained in conversation before reset

All 18 handheld view resources passed bounded authored-offset/FOV/projection and HUD telemetry. Jeep gunner placement and live HUD values for Submarine, Fighter, Driller, Jeep and APC were exercised. Submarine, Driller, Jeep and primary-only APC exit checks passed; Fighter exit failed separately after landing. An earlier stationary APC mortar shot killed the pilot and the following Use input restarted the section; that case was not reclassified as a success.

VO/music changes compiled before reset, but successful native audible output was not established. The QEMU WAV route did not capture XEMU's direct SDL output. A process-local paced ALSA file sink was implemented; an initial run omitted scene-preview admission, and its recording was silent. The corrected run was interrupted by the reset. No success is inferred from it.

Previous raw run artifacts were not recovered. The surviving source packages and screenshots are evidence of their own contents, not substitutes for missing runtime logs. Runtime verification must resume with actual owned inputs.

## Exact checkpoint recovered at 02:48 UTC

The retained publication payload restored the exact original vehicle/weapon checkpoint as remote commit e6390eb74f3c349c44c100e432eaf4ca8697346e, tree9af544dab8764e7e3ef479426f8e699c16669f99. The active continuation now starts from this exact checkpoint and applies only the recovered VO/music edits; it supersedes the context-reconstructed vehicle source. Native audio remains unverified.
