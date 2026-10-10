# Independent final shared-charge source review

Date: 2026-10-10. Baseline: `f595620ae6a9f9ada44596e9961d3cafd3519e3d`.
Reviewed joint tree: `/workspace/shared/continuous-charge-joint-next`.

## Disposition

Approved for parent-owned integration of the complete resource, runtime and codec slice. No remaining confirmed source-level blocker was found in the frozen joint artifacts. This is source review, not executable verification. No build, compiler invocation, syntax check, test, game/emulator run, route, inventory/health fixture, or active-repository edit was performed by this reviewer. The separate parent-reported 18:00 build failure does not establish any runtime result for this candidate. A Makefile/SDK warning-ownership correction is outside this review.

The approval applies to the exact 24 files in `FINAL-JOINT-SHA256SUMS`, including new includes that must not be omitted during integration. The joint scene file is exactly the runtime candidate plus the codec's intended three include/prototype hunks.

Input patch SHA256:

- Resource: `0fe2612736d60b727250d708e7dfbede2b2a48adaf20560092c672d16ac2ae9f`
- Runtime: `1d15011ba23fc5dcac0f34addd149c64a1322637be83100877d04d5427230e2f`
- Codec: `954b6719850026381d3ec8cc885f7801dfee2846bef7baec02e6fa45d33f8b74`
- Joint `src/diagnostic/scene.c`: `0fd7532fc5a3fc2f9c85052e1c39b5ce40a53966f11739c3d65ae81bd03b2b3c`

## Interface and inclusion

The eight-field 32-byte POD constructor initializers provide every field in the agreed order. Wire fields are written explicitly rather than by copying the runtime layout. The early state include exposes types and prototypes before selection, reset and checkpoint users. The runtime helper follows its command, inventory, timer and empty-operation dependencies. Checkpoint types precede loader declarations; the implementation follows Flame/burning checkpoint owners and precedes RFAP implementation. The linked-host adapter is defined after the driller runtime's complete types, with an earlier matching prototype. The new core functions are declared in their matching public headers.

## Authored resources

The new parser preserves actual idle0/run1/loop_fire2 ordinals separately from the existing four visual clips. It is limited to the 18 existing handheld profiles, Jeep's idle-only profile and eight known empty-FP hosts. Named resources, state order/count, compiled filenames and continuous flags come from the source table. Parsing and validation preserve output on failure.

The resource validator reads the real model skeleton and motion headers/tracks/keys. It frees model scratch before sequential motion payloads and frees every payload on success or failure. No geometry, material, playback cursor or extra inventory owner is constructed. Table scratch uses 128 KiB; sequential model/motion scratch uses 64 KiB. The supplied maximum state payload is 62,252 bytes, established by the separate original-resource inspection rather than execution of the new C validator. Retained profile admission does not demand an extra full rendered weapon.

## Gameplay ownership and accounting

The player tuple owns the source current/pending pair, reversible 0.2-second blend, f90 deadline, retained valid profile and admitted continuous episode. Runtime full handle, epoch and service reservations remain separate. The source request sequence retains ensure-idle followed by possible run or repeated idle, the strict movement thresholds, the halfway reversal rule and completed-transition elapsed overshoot. Current command comes from gameplay input, not the rendering cursor or later diagnostic ring.

Selection stops the outgoing episode and performs its own choose/blend observation while retaining f90. Ordinary unarmed observation freezes the pair without inventing a model clear. A real empty-FP host retains prior binding positivity and still advances a blend; a genuinely absent binding does not. Mounted descriptor selection is generation-qualified and does not treat stowed handheld inventory as the mounted weapon.

The original mounted actor distinction was independently reread: chooser4aa3b7 resolves the player actor and4aa3d3 queries that player's continuous byte, whereas4a5058 substitutes the host actor for mounted firing;4a584d/425840/42602e carry that host into41a870. Jeep's continuous-primary flag therefore does not justify a fabricated player episode. The current idle0 projection is consistent with this actor split.

The runtime captures the debit descriptor after ordinary cycle/mode selection and before later port callbacks. Once-only service observes the resulting pair, spends at most one loaded unit if f90 is unset/expired, and rearms from now at the qualified 25 ms Riot or 20 ms Flame interval. It does not catch up, refill or transfer reserve. Source duration/magazine metadata is used and its qualified result is checked. Both former fractional lanes are removed; no references to their old remainder variables remain in joint source.

Riot's separate attempt cost is committed once inside the reserved alternate-contact backend, before collision callbacks. The actor bit8 gate and actual source/weapon identity are checked. A miss still costs a serviced attempt. The existing ticket/service guard prevents duplicate continuation; release or the last charge does not become a new contact veto. Flame retains its captured admitted pulse through the final periodic debit, with damage cadence and fuel accounting remaining distinct.

## Findings corrected during review

1. Moving all Riot contact work to the outer completion initially allowed automatic empty reload to cancel the previously admitted last-cell contact. The final source services a prior alternate after the once-only shared drain and before those empty/reload branches. The new alternate remains independently delayed. Its 10 ms contact and 500 ms admission cadence prevent a legitimate same-frame old-pending/new-admission overlap.
2. Raw reload input initially cleared a Flame receipt even when the existing reload owner refused the operation, leaving ignition active without its episode. The final code stops on actual admitted reload or the existing Flame stop owner. A refused full-magazine/no-reserve request cannot silently break the join.
3. Episode release now uses its retained onset mode, and repeated accepted cooldown pulses do not remode the active episode. This follows the original actor active-byte and onset-mode ownership.
4. The late host adapter resolves the incomplete-type inclusion issue. Generic continuous admission follows successful finite shot debit; Riot admission additionally checks its existing live backend.
5. Boot wire preflight resolves Flame from the already opened immutable name catalog, rather than using the cached Flame ID before initialization.
6. Retained profile metadata no longer forces full renderer allocations through the saved weapon-demand mask.
7. Flame controller/canister publication was moved to final success beside the new shared tuple, avoiding a failed close publishing only one of those newly coupled owners.

## Persistence and failure boundaries

RFAP5 kind11 is one conditional 112-byte singleton row with reserved identity words, an explicit 32-byte payload and a zero tail. Only the exact constructor tuple is omitted. Shape validation covers signed state ranges, finite/canonical floats, active blend constraints, completed overshoot, remaining deadline bounds and real retained profile IDs. A retained state2 is valid even when current selection has no loop state or is unarmed; no blanket pair2/future-deadline world-save veto is added.

Only an actual primary Flame episode is supported for active-episode persistence. It joins kind9 ignition1/2 and candidate selected/owned Flame, without inferring pair2 or requiring a positive loaded cell. Other active episodes remain unsupported under existing generic transient fire/contact admission. New kind9 fractional debt must be zero. Legacy nonzero debt or continuous ignition/delay/active provenance is refused before scene replacement; separately represented reload, canister and cadence state remains eligible for the explicit constructor compatibility migration.

Cheap preflight is read-only and precedes QuickLoad replacement acceptance. Full candidate preparation rebases remaining f90 time once and snapshots the actual tuple, player handle and epoch. Final validation precedes narrow suspension of shared mutation hooks. Final successful storage close is followed by the existing finish work, Flame controller/canister assignment, shared tuple publication, scope release and audio reconstruction. Failure releases the scope without publishing these two coupled owners. This does not claim rollback of preexisting terrain/player/vehicle publication in the fresh-scene loader.

World save guards remain save-only and do not reject candidate restoration because the old runtime has an episode or retained pair. Standalone RFCP explicitly rejects a nonconstructor/busy live owner before mutation while admitting a genuine constructor-empty owner. RFPL, RFWM and RFCH formats, stock transport limit and loader stage budget remain unchanged. The RFAP row bound grows by exactly one row and the added private stage is charged using its actual `sizeof`.

## Explicit port policies and remaining validation

The approved policies are fixed-60-Hz gameplay observation plus committed selection calls; the bounded pre-contact capture/debit ordering; nonnegative saturation of the original raw Riot attempt decrement; live/dead/unavailable-source protection; quiescent legacy constructor migration; and new-player/accepted-section constructor initialization while retaining existing integer inventory. These are not claims of exact original disk or cross-section timing parity. Ordinary release, failed load, selection and accepted reload do not reset f90.

The candidate still needs the parent's scheduled executable validation. There is no direct Riot/Flame gameplay or save-roundtrip result here. Existing exact-version diagnostic save scripts were neither updated nor run. Any later source change invalidates the corresponding hash approval until reviewed.
