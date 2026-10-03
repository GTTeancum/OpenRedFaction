# Project requirements

- The authoritative project root is `D:/Programming/GitHub/OpenRedFaction`; `Installed_Game/` contains read-only original game inputs.

- Read and maintain `TO-DO.MD`; keep one-sentence open milestones at the top and mark review dependencies with `USER`.
- Xbox is primary: stock 64 MiB only, NXDK, C/C++, PS2-level visual parity, single-player first.
- Keep the PC build available as a shared-code staging path; compile it only when a specific shared-code change needs that compatibility check. Direct implementation and runtime validation effort to Xbox.
- Current mandate: pause campaign-route progression and prioritize core gameplay in an enemy-free developer testbed for weapons, animations, GeoMod and movement; resume campaign integration after the core systems are usable.
- GeoMod fidelity is now the top priority: treat faceted craters as prototypes, validate against original-game evidence, and prioritize destruction shape, materials, lighting, debris, collision and repeated cuts before returning to other systems.
- Product first: deliver a playable first pass with movement, combat, AI, mission events and level progression; exact 1:1 RE is not required.
- Use practical shared implementations where they unblock gameplay; defer general visual polish, but GeoMod shape, materials, lighting and debris are required fidelity work, not deferred polish. Preserve Xbox memory limits and the final visual-parity goal.
- Do not substitute an executable patch or PC-only engine for standalone reconstructed Xbox game code.
- Record source addresses and evidence for reconstructed behavior; label scaffolding and unverified assumptions honestly.
- Keep original assets/binaries, downloaded references, Ghidra databases, and generated outputs outside tracked source.
- After every Xbox validation batch and before handing off, run `tools/clean-generated-artifacts.ps1 -Apply` to prune regenerable large run payloads under `artifacts/` and copied disc/ISO payloads under `build/`; keep compact reports/logs, original inputs, and the reusable test HDD. Check for active project processes, measure real repository size without following junctions, and keep it under 15 GiB. Do this regularly rather than waiting for the repository to grow by tens of gigabytes.
- Parent exclusively owns cleanup, builds and emulator runs. Helpers must never run cleanup, including at handoff; report generated paths to the parent instead. Cleanup must reject active harness/build processes even when launched with relative paths.
- Preserve third-party provenance and applicable license notices when reusing code.
- Never activate Computer Use, Codex capture, desktop automation, or host keyboard/mouse/controller input.
- Use source, files, logs, native emulator capture, and process-contained harnesses; ordinary terminal process management is allowed.
- Do not spawn agents unless the user explicitly requests delegation.

- Keep the four existing GitHub images; upload no additional screenshots until near-retail-quality replacements are available.
- User update (2026-09-25): No images; do not capture, generate, display, or upload images until the user changes this instruction.

- Do not launch a second Red Faction XEMU instance while an existing project session is open; leave the existing session untouched and continue Xbox source/build work.

- Do not use original-game screenshots as visual references or pursue disc-dependent capture sessions; use binary-derived mathematical evidence and reconstructed PC/Xbox validation for fidelity.

- User update (2026-09-18): Three implementation agents are authorized again for independent weapons, enemy AI and scripted gameplay work. Parent owns shared scene wiring, builds and Xbox validation. Assign separate files and bounded working-code deliverables; do not keep agents occupied generating speculative RE reports. The earlier helper pause is superseded; do not restart the separate coordinator automatically.

# Delivery and testing priority

- User update (2026-09-18): Prioritize a working, integrated engine and core gameplay; polish comes after the engine is assembled.
- Keep testing proportional: use focused checks to establish that changed behavior works and catch material regressions; do not repeatedly run broad suites or expand edge-case testing without a concrete need.
- User update (2026-09-27): Do not play through campaign routes as part of development; implement systems and use bounded functional checks instead. Keep existing route scripts in the repository, but stop running them unless the user requests that work again.
- User update (2026-09-30): Xbox is the sole runtime implementation and validation target. Do not perform routine PC builds or PC gameplay tests; compile the PC staging target only when a specific shared-code change requires a compatibility check. Use focused stock-64-MiB XEMU checks for changed behavior.
- Do not let exhaustive validation, minor fidelity details or isolated edge cases monopolize progress on missing engine systems. Record remaining issues in TO-DO.MD and continue integration unless they block basic gameplay, stability or the stock 64 MiB Xbox target.
- GeoMod remains a core priority, but its polish and exhaustive coverage must not delay assembling the rest of the engine.

# Completion reporting

- User update (2026-09-18): Report completion against code implementation and working late-alpha/early-beta functionality, not exhaustive validation, retail parity or polish. Tester feedback will drive subsequent refinement.
- GeoMod is accepted as complete for the first playable implementation (100% on that milestone). Retain known limitations in TO-DO.MD as deferred refinement; do not reopen it as the primary workstream unless a concrete blocker prevents core gameplay or the user requests it.
- Overall implementation estimate is approximately84% as of this update. It is a rough engineering estimate, not a measured test-coverage or retail-readiness percentage. Keep per-turn percentages and a very high-level current-system report.

- Reporting update: Omit the completed GeoMod percentage. End each turn with overall implementation percentage and a percentage for the current active system, naming that system at a high level.

- User update (2026-09-22): Pause all sub-agents to reduce token usage. Continue solo; do not restart helpers without explicit renewed authorization. Preserve work already written for parent integration. This supersedes the earlier three-agent authorization.

- Estimate calibration (2026-09-30): Overall working-alpha implementation is provisionally85-90% (report approximately88%), superseding95%. Count integrated usable behavior, not merely the presence of a subsystem or adapter; missing gameplay integration belongs in the estimate, while exhaustive tests and retail polish do not. Do not increase the estimate automatically for every small fix.

- User update (2026-09-30): Three implementation helpers are authorized again to accelerate bounded gameplay work; parent owns shared scene integration and serial Xbox builds/checks. This supersedes the September22 helper pause; do not restart a separate coordinator.

- User update (2026-10-03): Concentrate parent and implementation helpers on one gameplay system at a time. Current focus is vehicles: finish the active integration slice before starting unrelated NPC or item work. Split ownership within that system (implementation, compatibility review, focused harness) rather than spreading agents across subsystems.
