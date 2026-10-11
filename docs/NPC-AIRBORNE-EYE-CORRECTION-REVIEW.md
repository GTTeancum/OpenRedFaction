# RFNC26 actual07 ordinary-eye correction: independent exact review

## Disposition

APPROVED for parent integration of the six exact staged files identified below. No remaining source-level blocker was found. This is a source-review disposition, not compilation, actual live qualification, stock64 fit, save promotion or fresh-load success. The actual07 first nested failure remains unobserved.

The initial candidate had one interface blocker: the loader receipt's identity recheck called the former two-argument signature after the diagnostic receipt parameter was added. The frozen candidate corrects that call to `scene_npc_airborne_identity(e->slot,o,NULL)`; the lifetime path retains its original error channel and semantics.

No repository edit, import, syntax check, test, fixture, build, emulator, process lock, HDD action, image or runtime retry was performed in this review. Staged review artifacts were written outside the repository.

## Exact composition

Game-source baseline: `cd1ab654c81dabc5e5dee3434763fed4c7092901`.

Current clean documentation successor: `473f9444947f8adba5dbe0b35e2b57cf9661ba37`. All six baseline file hashes matched the author's manifest independently.

Candidate root: `/workspace/shared/npc-airborne-eye-correction-next`.

- `correction.patch`: `9663a47596694d70d43bd7000ff457679c07be82338a40232c2d4ecf31a04783`
- `manifest.json`: `4cbf9586cd87ab167d69405e91709117f5fb3124635204f7309d220f2a4ba501`

Approved staged source SHA256:

- `include/rf/eye.h`: `ec642994f3cb71eba42508cc1c6d00dc9871be51569db5ae0814735b648945c1`
- `src/core/eye.c`: `38ec4505a6c95063a016cb977e13ab9a7e1931ae5030d8553d0fe5188062f66e`
- `src/diagnostic/scene_npc_airborne_checkpoint.inc`: `396c5b6949cecf882703ef76d37593d3c0dfb28290b9c5f3ec82c8919afaf243`
- `src/diagnostic/scene_npc_checkpoint_capture.inc`: `dd9d13f5d048bcaa32fb49933ab334893fbe0f664452e93156b6aac8cb3a2a57`
- `src/diagnostic/scene_npc_checkpoint_restore.inc`: `e569600589b9093273f2a34f4977d7e022ea007a4df0ac52d3b6152b03ceffd5`
- `tools/xemu_natural_save_gameplay.py`: `cb6efbdace1d7903c6b8b34404fc1b10914753e548df4fe12b8fc5cf28e6e574`

Approved supporting prose:

- `FINDINGS.md`: `c21a820f106f87224cddb0302076326ec4098d998fa919d645e25f61ad06bd44`
- `OBSERVATION-FORMAT.md`: `eb57e4ee2cb96977f6f2f2a189e3fe8155185b6a4a80f037614b3469f25c4be7`

Every staged source file, both documents, the patch, manifest and all listed evidence files were independently hashed. Source was compared directly with the unchanged baseline; all changed hunks were reviewed. Approval does not extend to later changed source bytes.

## Observed07 evidence boundary

Raw actual07 JSON hash `8139de4c068ce02c95b189700216a64f31f3c51edfd6cf9e7ee4138a284f5604` matches both the existing producer result and packet evidence. The tuple `[929,461161902,0,1325400064,4294967294,120]` resolves to the aggregate `ordinary_airborne` rejection, RF_FORMAT(-2), frame120. The old velocity guard follows it and was not reached. The zero velocity observer is expected from this control flow, not evidence that velocity was zero or the old rejection passed.

The part field does not identify an inner condition. The generic flags argument was reduced to binary32, losing low integer bits, so this tuple cannot prove current flags0x80000079. The new discriminator must not be retroactively applied to actual07. No promoted save or consumer result exists for that run.

## Ordinary mathematical owner and narrow repair

Retained original disassembly independently supports the source distinction. The body4000-clear ordinary49d184..49d1f6 path adds body864 and eye87c through40a030, whose three sums are stored as binary32, then calls4a0d70 for the eye matrix. The separate49fe7a..49feb1 path copies the physical basis and performs pitch/roll rotations using894+87c and89c+884; it omits yaw. Original4a0d70 consumes pitch and yaw and ignores roll.

Baseline `rf_ordinary_motion_commit` already follows the first convention. The RFNC26 qualifier and generic restore builder incorrectly selected the second convention for the new ordinary lane. Nonzero eye yaw and general eye pitch establish intrinsic non-equivalence independent of the missing actual07 inner-predicate observation.

The new pure `rf_ordinary_eye_orientation` validates finite inputs, constructs each sum using the same double-add/binary32-store expression, and delegates to the same private `look_orientation_calculate`. Output is preserved on failure because the existing private routine publishes only on success. Inputs are not mutated; local combined storage also permits the documented output alias. It introduces no eye integration, offset894 application, extra clamp/wrap, timer, physics, callback or owner publication. It avoids public `rf_look_orientation`'s narrower body-angle range, which would incorrectly reject legitimate summed angles.

The actual ordinary commit is unchanged. Only the new-airborne endpoint comparison and explicitly present airborne candidate eye-basis branch select this helper. Legacy, seated, Drone, flight and script-physics candidate policy is unchanged. Capture and restored candidate now share the actual ordinary mathematical owner while retaining the 1e-6 finite basis tolerance.

The fresh-constructor admission uses the same corrected endpoint check. All its other stricter identity, yaw-only body, exact current/next endpoint, body/model basis, old word168-zero, absent dynamic support and compensation-subset conditions remain. An authored pose that fails those conditions is still rejected; no normalization or coercion is introduced. Saved eye angles already have an existing common codec lane, so no codec/version/layout change is needed.

## Predicate and discriminator audit

Every expanded identity, geometry, endpoint, descriptor, flag, contact, support and dynamics condition was compared against its old compound predicate. Apart from the intended ordinary-eye reference, truth conditions, reached-input order, tolerance and return statuses remain equivalent. Pointer/bank/count checks still precede protected dereferences. The original current/next matrix memcmp becomes a per-element raw-bit comparison, retaining signed-zero distinctions after the existing finite basis checks. The descriptor expansion preserves every component and enabled-low-byte gate. Potential-owner selection and no-fallthrough behavior are unchanged.

The new receipt is20 bytes of local scalar metadata; the terminal bank is16 uint32 words/64 bytes. The global bank is not an admission input and is never used to repair a state. Generic rejection and top-level capture clear it. On an airborne failure, the unchanged aggregate rejection is produced first; only then are the exact six-word tuple join, owner index, first source site, component, operand kind, operands and raw body flags written. The format marker is assigned last. No later guard is evaluated after failure merely to collect diagnostics.

Observed float fields are copied directly through pointer-based memcpy rather than float-value parameters. Expected values are explicitly documented as comparison-reference operands, including placeholders/bounds where appropriate, rather than universally promising untouched source bytes. Pointer checks expose boolean outcomes, never addresses. The line-based identity is correctly bound to the exact source-file hash, and the documentation warns against interpreting it against edited source or inferring success from a zero bank.

Restore and loader lifetime paths pass NULL for the optional receipt. The previously missed call is fixed. Include order places the exported diagnostic bank and helper types before their capture use. All changed helper call signatures were inspected. The Python harness changes only one symbol-retention entry for16 words; schedule, source/disk guards, acceptance, inputs and consumer policy remain unchanged.

## Capacity and remaining work

The source-accounting statement is consistent:64 global diagnostic bytes,20 local receipt bytes, one capture descriptor pointer,12 helper angle bytes plus loop index; the old72-byte temporary eye state is removed from endpoint reconstruction. There is no new heap allocation, NPC-owned persistent field, per-row array or wire payload. Generated code size and compiled stack/memory fit remain unmeasured.

Parent can integrate only the approved bytes under the existing authorization. The next permitted parent-owned batch must establish compilation and its own actual producer/load outcomes. Another inner condition, later row, candidate placement or resource/storage guard may still reject. This correction repairs a proven source mismatch; it does not turn actual07 into a success or establish exactly which unobserved predicate rejected first.
