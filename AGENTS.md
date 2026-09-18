# Project requirements

- The authoritative project root is `D:/Programming/GitHub/OpenRedFaction`; `Installed_Game/` contains read-only original game inputs.

- Read and maintain `TO-DO.MD`; keep one-sentence open milestones at the top and mark review dependencies with `USER`.
- Xbox is primary: stock 64 MiB only, NXDK, C/C++, PS2-level visual parity, single-player first.
- Maintain a PC build with the same core code and Xbox resource constraints.
- Current mandate: pause campaign-route progression and prioritize core gameplay in an enemy-free developer testbed for weapons, animations, GeoMod and movement; resume campaign integration after the core systems are usable.
- GeoMod fidelity is now the top priority: treat faceted craters as prototypes, validate against original-game evidence, and prioritize destruction shape, materials, lighting, debris, collision and repeated cuts before returning to other systems.
- Product first: deliver a playable first pass with movement, combat, AI, mission events and level progression; exact 1:1 RE is not required.
- Use practical shared implementations where they unblock gameplay; defer general visual polish, but GeoMod shape, materials, lighting and debris are required fidelity work, not deferred polish. Preserve Xbox memory limits and the final visual-parity goal.
- Do not substitute an executable patch or PC-only engine for standalone reconstructed Xbox game code.
- Record source addresses and evidence for reconstructed behavior; label scaffolding and unverified assumptions honestly.
- Keep original assets/binaries, downloaded references, Ghidra databases, and generated outputs outside tracked source.
- Preserve third-party provenance and applicable license notices when reusing code.
- Never activate Computer Use, Codex capture, desktop automation, or host keyboard/mouse/controller input.
- Use source, files, logs, native emulator capture, and process-contained harnesses; ordinary terminal process management is allowed.
- Do not spawn agents unless the user explicitly requests delegation.

- Keep the four existing GitHub images; upload no additional screenshots until near-retail-quality replacements are available.

- Do not launch a second Red Faction XEMU instance while an existing project session is open; leave the existing session untouched and continue PC-side work.

- Do not use original-game screenshots as visual references or pursue disc-dependent capture sessions; use binary-derived mathematical evidence and reconstructed PC/Xbox validation for fidelity.

- User update (2026-09-18): Three implementation agents are authorized again for independent weapons, enemy AI and scripted gameplay work. Parent owns shared scene wiring, builds and Xbox validation. Assign separate files and bounded working-code deliverables; do not keep agents occupied generating speculative RE reports. The earlier helper pause is superseded; do not restart the separate coordinator automatically.

# Delivery and testing priority

- User update (2026-09-18): Prioritize a working, integrated engine and core gameplay; polish comes after the engine is assembled.
- Keep testing proportional: use focused checks to establish that changed behavior works and catch material regressions; do not repeatedly run broad suites or expand edge-case testing without a concrete need.
- Do not let exhaustive validation, minor fidelity details or isolated edge cases monopolize progress on missing engine systems. Record remaining issues in TO-DO.MD and continue integration unless they block basic gameplay, stability or the stock 64 MiB Xbox target.
- GeoMod remains a core priority, but its polish and exhaustive coverage must not delay assembling the rest of the engine.

# Completion reporting

- User update (2026-09-18): Report completion against code implementation and working late-alpha/early-beta functionality, not exhaustive validation, retail parity or polish. Tester feedback will drive subsequent refinement.
- GeoMod is accepted as complete for the first playable implementation (100% on that milestone). Retain known limitations in TO-DO.MD as deferred refinement; do not reopen it as the primary workstream unless a concrete blocker prevents core gameplay or the user requests it.
- Overall implementation estimate is approximately70% as of this update. It is a rough engineering estimate, not a measured test-coverage or retail-readiness percentage. Keep per-turn percentages and a very high-level current-system report.

- Reporting update: Omit the completed GeoMod percentage. End each turn with overall implementation percentage and a percentage for the current active system, naming that system at a high level.
