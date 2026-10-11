# Ordinary hit reaction persistence: staged source

Baseline: `d98ee2882c9caa467e0ba2bcb23d118b0f3fbce0`.
Target integration: after the parent-owned 22:00 outcome only.
Status: implemented outside the active repository and independently source-approved; parent joint integration review remains separate. No compilation, syntax check, tests, new gameplay fixture, emulator, or runtime verification was performed.

## Files and integration

Apply `hit-reaction-codec.patch` against that baseline. It changes only:

- `include/rf/npc_checkpoint.h`
- `src/core/npc_checkpoint.c`
- `src/diagnostic/scene_npc_pain_checkpoint.inc`
- `src/diagnostic/scene_npc_checkpoint_capture.inc`
- `src/diagnostic/scene_npc_checkpoint_restore.inc`
- `src/diagnostic/scene_burning_checkpoint.inc`

The separate runtime patch owns `scene.c`, `scene_npc_hit_location.inc`, and `scene_handheld_penetration.inc`. There are no shared edited files or new include-order requirements. The existing early `scene_hit_location_class_named` helper supplies the exact guard1/guard2/medic1 scope. No public runtime API or mutable gameplay field is added.

## Version and owner contract

RFNC24 uses precisely the RFNC23 wire layout. It extends the ordinary pain action grammar from -1/22/23 to -1/22..27, and qualifies the new residual playback admission. RFNC1..23 retain their prior core enum and scene ownership language. No new wire field, timer, saved handle, source hash or catalog hash is introduced.

`rf_npc_checkpoint_record.pain_residual_profile` is one **nonwire** boolean capability, not proof that a residual has an owner. Fresh capture sets it only for a retained ordinary24..27 hint or an otherwise-unowned clip actually admitted by the new residual rule. Decode sets it from component version>=24 on every candidate row, including unrelated rows and RFAP-owned burning rows. Core validates the boolean and promotes the writer to24 if any row requires it or has a new ordinary retained action. Direct scene restore also requires the capability for new hints/residual semantics.

The older latest-selected22/23 ordinary path remains unchanged for all classes. The older RFAP latest-selected0..44 path also remains unchanged; a burning latest24..27 action alone does not promote RFNC24. The new residual branch runs only after all prior script/combat/death/residual-fire claims fail. A residual-only ordinary actor gets the existing pain-present payload with its actual deadlines, actual selected action (possibly-1), and shared RNG. An active scripted animation does not acquire a redundant residual-only pain owner. Hidden ordinary actors can retain exact pain playback; visibility is not a new admission condition.

Burning continues to omit RFNC pain. Its residual slots stay unresolved until the composed join proves the actual RFAP target/source/cadence/pain owner. Only then does RFNC24 permit the exact residual proof. Existing RFNC/RFAP mutual exclusion, shared RNG equality, and burning visibility/terminal restrictions remain intact. Missing RFAP cannot discharge an obligation.

## Exact residual proof

The pure predicate requalifies the source actor, registry/entity full handles, actual model registration/list membership, non-owned live pose, skeleton/bones, and the actual class/current-or-saved weapon mapping. The unarmed medic uses its base mapping without a fabricated firearm/inventory prerequisite. It admits only actual mapped actions22..27 for the three selected classes.

All slots retain numeric and duplicate-motion checks. Requested residual proof requires real resident cache identity and exact entry size, authored comparison/loop equality, capture reference>=1 (candidate>=0), nonlooping positive finite-range header duration, phase[0,1], no frozen completion/freeze slot, requested weight[0,1] including zero, and start<=tick<end. Only the requested slot needs resident proof, preserving sequential candidate resource admission. Latest hint, timers, combat absence, primary slot and visibility are not inferred ownership tests. Other nonloops still need their own owner. No clipping completion or model-clock advance is forced.

## Replacement and section policy

No publication refactor is included. Existing RFNC staging validates the whole candidate and the composed RFAP obligations before its existing assignments. Ordinary NPC/pain assignments and RFAP burning assignment precede final storage close in the existing fresh-frame-zero world replacement path. This patch does not claim whole-world failure rollback or a new final-close-only pain transaction. The existing quickload preflight/replacement behavior remains unchanged.

RFCH has no pain lane. Genuine accepted section replacement reconstructs constructor pain deadlines/action and startup playback. This remains the explicit existing fresh-section policy, not exact cross-section continuation. No handoff wait, new timer, replayed pain start, weapon reset, sound or damage callback is added.

## Memory and limits (source arithmetic, not compiled measurement)

- Wire row maximum remains1768 bytes; header64; maximum count2048. Maximum theoretical RFNC component remains64+2048*1768=3,620,928 bytes.
- RFNC24 adds zero bytes compared with RFNC23 for the same rows. Version promotion still includes prior version tails: from RFNC15 to24 adds the existing164 bytes per row; from RFNC10 to24 adds the existing276 bytes per row. This can legitimately reach RF_RANGE earlier under the fixed transport cap.
- Stock profile0 `RF_CHECKPOINT_FILE_MAX` remains110,524 bytes. Expanded profile1's separate262,144-byte option is unchanged. The world staging budget remains2 MiB. No cap is enlarged.
- The nonwire capability adds4 bytes per in-memory row: at2048 entries,8192 bytes for a row array. NPC restore entries contain saved+before rows, adding8 bytes per entry,16,384 bytes at2048. A temporary decoded array plus those two stage copies adds at most24,576 bytes before row release. Existing `sizeof`-based world restore temp and NPC stage budget checks automatically charge these additions.
- Existing preliminary resource-demand decode/export arrays are bounded by actual row count/transport limit and use `sizeof`; they remain outside the composed-stage budget as before. This is not a newly claimed accounting correction. At the stock cap, even the smallest legacy528-byte row permits at most209 rows, so preliminary decode capability growth is at most836 bytes; current export600-byte minimum permits184 rows, at most736 bytes.
- No live NPC, RFCH history or retained resource storage grows. The binding proof has one automatic `rf_entity_motion_selection`: mapping71 words284 bytes,45 x86 pointers180 bytes, declarations8 bytes =472 bytes, plus scalar/pointer locals. Exact compiler stack impact is unmeasured; no recursion or dynamic allocation is added.

## Review evidence

`evidence/SOURCE-PROPOSAL.md` and the reviewer-owned `evidence/INDEPENDENT-REVIEW.md` retain the source addresses, actual named resource facts and bounded policy. Source-only audit covered core read/write/span/version promotion, two-pass decode, record initializers (zero/whole-copy only), sizeof-based stage budgets, ordinary and RFAP joins, include order and the unchanged publication boundary. No check execution is implied.

The source version audit also covered existing Python RFNC consumers. Their version-specific old fixture expectations remain untouched; no new24 states are fabricated into those recipes. Generic core preflight/decode now accepts through24 with the explicit pre24 action restriction. Existing record initializers are zero-initializers or whole-record copies, so the nonwire member does not create a positional-initializer omission.
