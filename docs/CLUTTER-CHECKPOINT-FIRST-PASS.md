# Prop checkpoint component

RFPC1 encodes up to1024 UID-sorted24-byte prop records under a64-byte header, with explicit little-endian fields, authored identity32 and a checksum. It retains class identity, health, retired/hidden/hit flags, killing type and shared-class cooldown remaining. Positive-health GeoMod retirement is valid; nonpositive health without retirement is rejected. No process pointers or generation handles are serialized.

Encoding validates all input before writing. Decoding validates the complete stream and every row before publishing to caller-owned storage, including identity mismatch, truncation/corruption, UID order, cooldown consistency and capacity. It allocates nothing and uses one record of scan scratch; callers provide disjoint storage. The maximum wire component is24,640 bytes. The authored CTF06 population would require12,208 bytes.

Focused tests pass live/dead/GeoMod-retired roundtrip, identity and checksum failures, atomic output preservation, truncated input, capacity, duplicate UID, mismatched shared cooldown, missing retirement and nonfinite health. PC and NXDK compile the module.

The scene adapter now captures settled prop health, visibility, destruction and cooldowns, rejecting pending gameplay break requests and moved props. A digest covers authored source, UID, class/model and damage metadata and pose. Restore resolves saved UIDs against fresh registered owners and rebases cooldowns to the new simulation clock without replaying explosions. Cosmetic particles are transient and are not resumed. Restoring into already damaged owners remains rejected until their attachment/glare resources can be rebuilt.

RFCP4 now appends a mandatory RFPC1 section after the existing vehicle section, preserving all earlier section offsets and the32-byte header. Its length is the remaining envelope bytes and must agree with the validated RFPC header. New readers accept versions1–3 as explicitly prop-absent; old readers reject4. The RFSG maximum remains110,524 bytes: prop records reduce the available terrain budget rather than growing every save buffer. Existing saves must retain explicit compatibility rules. Legacy saves retain the immutable baseline check. RFCP4 uses a second baseline digest that permits only persisted health, flags and class cooldown to change; other static-world restrictions remain. Live PC and native Xbox save/reload now pass.

## Composed framing verification

Focused RFCP4 tests pass prop roundtrip, exact borrowed section locations, wrong identity, nested checksum corruption, atomic capacity failure and legacy prop absence. Existing RFCP framing and vehicle/remote checkpoint tests pass after factoring the common preflight path. All prop records validate without an allocated output array during outer preflight. PC and NXDK builds pass. Full RFDS semantic validation and UID/class scene remapping remain caller responsibilities; this framing change does not publish live state.

## Live scene integration

RFCP4 capture and load are now wired into the player checkpoint path for the existing authored CTF06 static-world profile. The original506-object/class/ownership guard remains; this is not general campaign persistence. The process-local baseline is104 bytes, up32 bytes for the mutable digest. The prop payload is12,208 bytes, deducted from the unchanged110,524-byte total checkpoint cap. Temporary capture/restore arrays are freed after use.

All prop decoding, UID/class mapping, freshness and registry target checks precede terrain publication. Terrain commit does not replace registered prop owners; prop publication afterward performs assignments only. Existing draw/collision/glare retirement consumes restored flags. Focused tests cover damaged/dead/hidden records, replacement registry handles, pending-break rejection, cooldown rebasing and rejection without partial mutation. PC and NXDK builds pass. A real damaged-prop PC save/load replay and native destroyed-prop reload now pass.

The live fixture relocates only original lamp UID13025 near DEV spawn while retaining all506 props and authored pickups. Normal pistol shots save health80/40/0 and retired flags0/0/2 in RFCP4. Separate-process loads restore40 and0 respectively, with zero repeated damage or break effects. The actual resumed images were inspected: the damaged lamp is visible and the destroyed lamp remains absent. Artifacts: `artifacts/clutter-checkpoint-live`.

This run exposed two integration issues, now fixed: nonvehicle DEV world textures reserve8MiB for the expanded weapon-view set under the existing total image cap, using the existing filtered texture fallback; PC mode selection now precedes world loading, matching Xbox. DEV auto-supply no longer grants optional firearms merely because their resources were loaded for distant level pickups. Actual pickup acquisition remains enabled.

Remaining: general moving props/classes/levels and campaign persistence. The temporary loaded-terrain gate clears after its upload; subsequent saving is supported, as verified below.

Native run `artifacts/xemu/render-20260918-212730` passes180 frames on64MiB with2083 free pages (8.14MiB). Its inspected framebuffer retains the destroyed lamp absence, and all compared player checkpoint and prop damage words match PC. The native recapture, PC recapture and original13,276-byte checkpoint have identical SHA256 `e8319a4d1ad773ca7dca64078dabde1e1248b3f60245f149e8cd2bf60080c51d`. This verifies reload followed by another save, not merely parsing the incoming bytes. The harness restored the original disc and closed its emulator. Audio was not auditioned.
