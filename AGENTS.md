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

- User update (2026-09-16): Work solo on resumption; keep local sub-agents and the secondary helper coordinator paused until the user explicitly reauthorizes delegation.
