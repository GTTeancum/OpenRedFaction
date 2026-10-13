# Actual22 startup: concrete unsupported-conversion dependency

## Useful result

A source-proven Xbox defect sits in the observed unfinished stage48 load path:
`scene_weapon_impact_visual_profile_read` invokes `strtof` to read its first
selected weapon's impact radii. The installed NXDK implements `strtof` as an
assertion stub, and the actual22 linker map resolves that symbol to the stub's
library object. Replace that call with the staged bounded decimal parser.

This is a grounded correction, not proof of the exact actual22 instruction
pointer. The smoke did not read guest registers or an assertion message, and
stage48 covers multiple resource calls. Do not claim that fixing this defect
has already made startup pass.

## Preserved build and smoke evidence

- Actual22 source: e1255c25e7a752185eb1b099c951ae4c3f0578ce.
- `/workspace/shared/rf-20261012-2200-build.json` reports successful Xbox
  profile0 build, link, XBE and tester ISO generation. The parent log agrees.
- `/workspace/shared/rf-startup-smoke-20261012-2200/observation.json` records
  default New Game acceptance followed by zero simulation/presentation frames.
- Last top-level load stage7; campaign stage48; late-load words
  `[48,0,5187,140,14603664,0,0,0]`; profile `[0,12]`; audio-close phase0.
  Stage48 began with 5,187 available 4KiB pages, about20.3MiB. That is the
  stage-entry sample, not a measurement at the unknown final instruction.
- Offline parsing of the preserved QMP stdout places the first observed48 at
  130.551 seconds after the first STOP, and the last complete48 observation at
  238.952 seconds. There are5,786 stage48 samples. The resumed intervals sum to
  about65.68 wall seconds between those samples; they are not CPU-time data.
- QMP continued returning immediate stop/read/continue receipts at the end.
  Its final timeout occurs near the launch deadline. It is not evidence that
  the host emulator itself was deadlocked.
- QMP quit was acknowledged, normal exit0 completed, and no forced cleanup was
  used. This establishes clean host-directed shutdown, not healthy gameplay.
- Actual21 also failed to produce a frame, but its helper lacked these stages.
  Its source8dce77c7 already includes the new impact parser from c4f9bf2e.

## Source chain

1. At actual22 `src/diagnostic/scene.c:22866..22870`, stage48 precedes turret
   death effects, robot death visuals, and conventional impact-resource open.
   The same stage remains during archive close and the initial clutter scan.
2. `src/diagnostic/scene_weapon_impact_visual_resources.inc:158..166` walks
   selected ordinary-weapon metadata during that open.
3. Its profile parser's line81 invokes `strtof(p,&last)` for impact radius.
   This was the only strtof call under project `src` when investigated.
4. Actual22 `build/xbox/main.map:2350` resolves `_strtof` at0036b290 to
   `libxboxrt:stdlib_ext_.obj`.
5. `/workspace/shared/recovery/toolchain/nxdk/lib/xboxrt/libc_extensions/stdlib_ext_.c`
   lines95..99 implement strtof as `assert(0); return0.0;`. It never implements conversion
   or writes endptr. With assertions disabled, this still cannot be used by
   this caller, which needs both the parsed number and initialized endptr.
6. `/workspace/shared/recovery/toolchain/nxdk/lib/pdclib/platform/xbox/functions/assert/assert.c:9..20`
   prints an assertion,
   disables interrupts, and repeatedly halts outside DEBUG_CONSOLE; its
   DEBUG_CONSOLE branch invokes RtlAssert.
7. Existing `docs/XEMU-MEMORY.md:102..108` records the same NXDK strtod/strtof
   issue and the repository's prior bounded-decimal solution. This is a
   reintroduced dependency, not a newly discovered requirement for NXDK.

## Minimal staged change

`startup-strtof.patch` modifies only the impact-resource include. A local
positive-decimal token parser follows the existing shared-reader approach:
optional plus, decimal digits and optional decimal point, ending at whitespace,
right parenthesis or NUL. It admits only the existing float values .25/.3.
Exponent, hexadecimal, negative, non-finite, empty and suffixed tokens are not
part of this bounded authored grammar and are rejected. The line reader already
bounds every input token within a1024-byte NUL-terminated local buffer. Numeric
accumulation bounds the integer part at1 and the fraction below1; it cannot
produce overflow. Failed parsing leaves caller pointer/output unchanged.

The authored values are documented in `docs/WEAPON-WORLD-IMPACT-VISUALS.md`:
.3 for ordinary supported weapons and .25 for scope_assault_rifle. Surplus
radii for one-clip profiles remain parsed and validated exactly as before.
Independent reviewer confirmed existing extracted source
`/workspace/shared/next-operational-gap/evidence/weapons.tbl`: literal0.3 pairs
at lines481/572/665/751/934/1035/1123/1506/1741/1808, and literal0.25 pair
at1599. No original asset or HDD access was needed. Reviewer accepted the exact
manifest-bound patch with no blocking findings; review was source-only.

This keeps the optional-bank failure behavior and does not disable visuals,
change damage, enlarge memory budgets, add archive work or alter save formats.

The staged source is based on the same unchanged impact file now present at
parent91f9604a after PlayVclip integration. `manifest.json` binds source/patch
hashes. Parent owns integration, tracking, scheduled compile and any authorized
startup-only smoke. No compile, syntax execution, tests, emulator launch,
protected-HDD inspection, images, original evidence edits or real-repository
edits were performed by this worker.

Parent reported integration as b6acfad911389bf154444aa8962aeab44e62bad5 at22:09 UTC.
That source commit was not compiled or run at this handoff; actual22 remains a
failed startup observation of e1255c25 and is not relabeled by the source fix.
