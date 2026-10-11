# RFNC26 ordinary guard1 airborne: exact runtime and joint review

## Disposition

APPROVED for parent integration of the exact eight source files identified below. No remaining source-level blocker was found in the runtime/candidate/lifetime review or the separate codec cross-interface review. This is a static-source disposition, not a compilation, stock64 memory-fit, live qualification, save, or load result. No repository edit, import, test, syntax check, build, runtime, fixture, HDD operation, or activation was performed by this reviewer.

Baseline: clean `c3f4b9700b3a5a9d942bf7d4ff5f8c12e98959fb`. This is a documentation-only successor to game-source baseline `ec06e95d49bb11bcf49baa69b0c625ab70dd5fd5`. The source design is `COMPLETE-QUALIFIER-DESIGN.md`, SHA256 `604303a531d1e0105ed6d577b2026d25fdc21d70ca8f806b10089a90732ea934`, independently approved in `NPC929-COMPLETE-AIRBORNE-DESIGN-REVIEW.md`, SHA256 `7d692136d2d7e08b1a79607c270a3819662e72f2ec5bbcee3d96d2a5fdba056a`.

## Immutable composition

Joint root: `/workspace/shared/npc-airborne-joint-next`.

- `joint.patch`: `efc72ee037a92d1cc28ad6bcd2632d16e625be6cc6bb10138977bdd2cd4d5668`
- `manifest.json`: `28223cf599b8ef15940a65ce9e10cfd1396fba803e69b1bcb1f06609e2f111e5`

Runtime root: `/workspace/shared/npc-airborne-runtime-implementation/versions/v1`.

- `runtime.patch`: `b386689e8e004208be8537064bb445019b80aa812fac907fc367ecf44ff2ce84`
- `manifest.json`: `0a5b57c51668c7140f91a3e042af6e8b3a9af30a3deeb03f1d9f27a7718033af`
- `IMPLEMENTATION.md`: `5bab33201fcfdb8870a94abfeef08155af0dbe9e676e231c7178e8a0242e0e86`

Exact source SHA256 values:

| File | SHA256 |
|---|---|
| include/rf/npc_checkpoint.h | f0c6ab4ad03b84b0d222e67902abc8de9a46faaac9ecf81d08757abf24b3d19e |
| src/core/npc_checkpoint.c | 8ecb549f54741745a020350627d79dea05815a6d30fe721dc17daf3e4f877923 |
| src/diagnostic/scene_checkpoint_world.inc | 8feb6d04711b14d8912031b082272ad96422487316117e15e203e9e2e61584fc |
| src/diagnostic/scene_npc_airborne_checkpoint.inc | 222794e386cf38ce9068344e873fd8d37623549f3a774a268a7a840cad5cdb21 |
| src/diagnostic/scene_npc_checkpoint_capture.inc | 6a0913218178e7cc2314d927060a77f8d53378313bd0cc0c3e47e7a1f0e938ee |
| src/diagnostic/scene_npc_checkpoint_restore.inc | faf50f18561eacbf4034e52d4c6cc2e8f7898ad24d94dd7ac1c3e945ecfd3fbe |
| src/diagnostic/scene_world_load.inc | 831fd7639b2a9c6d482609daf4c3482a9a9f6ce8219be74a264fde0862b3a1fd |
| src/diagnostic/scene_world_restore.inc | 2f9c6d971dfbc91ead3fb0a13e179e65fe5d0d3129fba0671622732fd627c4a4 |

Every actual joint file was independently hashed and byte-compared with the frozen runtime or codec source. All eight matched. This approval does not extend to later changed source bytes.

Separate codec approval: `/workspace/shared/npc-airborne-codec-review-next/NPC-AIRBORNE-CODEC-REVIEW.md`, SHA256 `99baaeabb205ce77b6927a8ebc7e1d8aa8420a431a95f471e42585a2cf62c923`.

Final cross-interface approval: `/workspace/shared/npc-airborne-codec-review-next/NPC-AIRBORNE-CROSS-INTERFACE-ADDENDUM.md`, SHA256 `76a35e7c525af524c4c237092b37e6eebe1f0a7690d962c26c07b4e6bce4a4ed`. It binds this same runtime and joint composition and found no blocker.

## Runtime qualification and actual06 boundary

Actual06 classified the rejecting UID929 as living guard1/use-kind0, health80, movement3, body flags0x80000079, allocation396, absent support, scalar1441, word1640 and historical word168413. Actual06 did not observe every immutable class, tensor, basis, resource and candidate-placement condition. The new qualifier must still prove them at each live capture and fresh restore. It does not hardcode UID929, slot10, position, path, or face413.

The helper checks actual authored `ult2_guard.vcm` separately from the loaded compiled `ult2_guard.v3c`; exact class physics flags0x20019/flags2=1/use0/mass100/run1 and creation flags0; class-to-skeleton mapping, bones/count, seed pose and unowned model identity; full object and entity registry handles; living/nonretired/nondeath state; no attachment, seat, linked occupant, script-physics history, flight or robot-death ownership. Fixed-array persistence admission uses both count and RF_CAMPAIGN_ACTOR_SLOTS bounds, without an impossible array-null test.

Geometry qualification checks the three loaded immutable physical spheres, standing centers, ordered class radius receipt, allocation metadata, spring parameter-1/opaque0, Flesh material index and coefficients, resolved class speed/response, zero local tensor, reconstructed world tensor and current bounds. Original constructor baseline0x80000078 and fall bit1 are represented as exact current flags0x80000079. Prepared0x01000000 and all unproved flags remain rejected.

Current/next/published/model positions agree, current and next body bases agree exactly, and yaw/model/eye reconstruction is finite and tolerance-bounded. Eye reconstruction copies only the same saved angles87c into a zero-initialized eye state as the existing restore builder. Contact state1240, scalar1, reference-1, word1640 and zero command714 establish the committed boundary. All five physical vectors are retained as finite binary32 values without an invented magnitude cap; inherited support velocity alone retains the existing10000 bound.

Presence remains explicit for fully qualified zero dynamics. Once potential ordinary airborne ownership is identified, an exact-proof failure cannot fall through to the legacy quiet capture. Other existing typed owners retain their own admission.

## Contact-history ownership

The incoming face-history value is admitted only at the qualified inactive boundary. Retained source proof maps word168 to contact.word1f0 and the prior geometry face token; it is distinct from word164/pending flags. Ordinary current use-kind0 query consumers refresh contact before feedback. Fragment sphere response is not admitted for this exact class. Canonical inactive contact is therefore a narrowly proven gameplay continuation policy. It intentionally changes diagnostic history/hash observations; no bitwise diagnostic equivalence is claimed.

The fresh old compensation owner remains a stricter subset: old baseline0x80000078, old word1680, support handle0 and piece tag0. Static-floor material, exact old cached vectors, and the scalar old piece_reacquire are allowed and restored. Incoming absent support still requires material-1 and reacquire0. No old dynamic support pointer or borrowed piece owner is serialized or revived.

## Candidate and publication ordering

Scene restore validates the common record before the new scene admission. This preserves canonical positive-zero base carry and exact presence/movement values from the codec rather than relying only on weaker numeric comparisons in a helper. A full composed fresh load supplies fresh_boot1; the untyped/standalone scene boundary rejects new presence and refuses to erase a resident potential owner through legacy absence. Pure codec APIs remain pointer-free and may carry the new lane.

The candidate is built from separately qualified immutable fresh data. Saved vectors and flags are installed only after ordinary pose, bounds, tensor and publication normalization. Query flags come from the candidate for this lane. The typed no-support branch still runs full geometry volume, mover, prop and room checks. It cannot use authored-position, rotating or center-shift exceptions, and it does not query or invent floor support. The exact fall bit makes ordinary_no_contact false, leaving authored exception pointers null. Actor/player pair checks remain; only the final constructor-pose pair fallback is unavailable when either row is airborne, after unchanged independently typed route/pursuit checks.

NPC assignment copies the existing canonical base carry first, then installs the dedicated inherited support velocity. Body/contact/support/piece state is coherent with explicit absence and object/view bits agree. Successful finish does not clear these vectors: legacy absent-flight reconstruction requires a qualified flight owner, excluded here, and route publication does not replace the five dynamics vectors. No new event, animation evaluation, RNG draw or physics step is introduced by the qualifier.

## Lifetime, fallible publication and final-close compensation

The new loader-local receipt is inactive for all-absent or empty stages. With presence, it captures the existing64-bit scene_robot_death_epoch and owner-array identity/count, then validates the existing fresh owner/pose snapshots. It rechecks identity immediately before NPC assignment, after earlier fallible owner publication. Its assigned bit is set immediately after the callback-free void NPC assignment and before later fallible vehicle publication.

The common failure path runs after final native storage close, so both later publication failure and a close-only failure reach compensation. The initial error remains authoritative. Before any compensation write, an all-row preflight checks epoch/array/count, row bounds, UID/class, full wrapper/view/registry handles, persistence/model/pose identities, source-body allocation, sphere pointer/count/bytes and pose storage identity. The epoch and owner-array fences precede old owner dereferences. If one row fails, compensation writes none of the new aliases.

Only after the full pass succeeds does a second pass restore the new body flags/state124, five vectors, inactive contact fields, collision extras, support/carry/piece/reacquire and object/view mask aliases from existing owner_before snapshots. Position, route, inventory, animation, resources and whole-scene state are not rolled back by this helper. This is deliberately limited compensation inside the existing fresh-scene loader failure policy, not transactional restoration of the discarded timeline. There are no new persistent lifetime tickets or per-live-NPC arrays.

## Interface, API and storage review

Include order already makes capture's new helper visible before restore, world restore and world load; required existing types, caches and speed resolver precede it. Actual struct fields, pointer/array distinctions, bounds, helper signatures, mask constants and row member names match the baseline plus approved header. The final cross-interface review confirms all six vectors, canonical base fields, version promotion and variable flight-tail offsets match the runtime consumer.

RFNC1–25 decode zero presence. RFNC26 promotion is monotonic; version>=25 flight and version>=24 pain semantics continue to apply. Flight and airborne presence cannot coexist in a valid row. The unchanged110524-byte file cap and2MiB staging cap still enforce real limits.

Source accounting is consistent: logical record+84, saved/before stage+168*N, simultaneous decoded plus stage+252*N; relative RFNC25 wire growth4*N+80*A; maximum structural row2004 and envelope4104256. Capture/prepare current-row-plus-wire growth is88 bytes before additional runtime qualifier locals. The runtime capture lane adds84 bytes, the world context adds4, and the loader receipt is24 declared Xbox32 bytes, conservatively at most32 with alignment. Existing owner/pose snapshots are reused. The largest new helper's declared payload is156 bytes; the stated512-byte conservative runtime-branch allowance is source accounting only and excludes unchanged callees, ABI/compiler spills and measured fit. No compiled-stack or stock64 runtime-fit claim is made.

## Documentation and remaining verification

Reviewed integration prose `/workspace/shared/npc-airborne-integration-docs-next/NPC-AIRBORNE-CHECKPOINT.md`, SHA256 `bfc82a84ee9728e300581ef3b0ceca9d71a872dc56d88e992c449e68632c09f2`, and capacity note `CAPACITY-AND-INTEGRATION.md`, SHA256 `0c275de0c03da1b61a0731d1cb4898a9f9a4b15651c72b4163c7f9eed7e1c48f`. Their scope is accurate: actual06 classified the source actor but save still rejected; the separate fresh-control phase neither saved nor loaded; successful new save/load, later rows, placement and memory fit remain unverified. Their limited compensation and diagnostic-divergence statements agree with source. Adding these exact review references is an administrative documentation change, not a source approval expansion.

Parent may integrate only the approved source bytes under existing authority. A later build proves compilation only. A successful ordinary producer, preserved exact save artifact, and separate consumer are still required to establish persistence behavior. This does not recover the absent October9 L11S3 regression image and does not authorize any broader actor/profile admission.
