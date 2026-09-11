# Entity views and locomotion predicates

`include/rf/entity.h` exposes compact, caller-owned views of the original fields
required by the current predicates. They are not binary-compatible RF.exe
objects or fully initialized gameplay entities. The registry contains 1,024
pointers (4 KiB on the supported 32-bit targets); no predicate allocates memory.
All views, seat arrays and attachment snapshots must remain stable during a call.

Object lookup follows 0x40a0e0: -1 is invalid; the low 16 handle bits must be
below 1,024; the indexed pointer must exist and its complete handle must match.
Negative handles other than -1 may be valid. Entity lookup adds the type-zero
requirement from 0x426fc0. Entity class is info +1b4, as used by 0x486c90 for
type-zero objects; the view does not implement other object classifications.

Weapon presence follows 0x408dc0. Either weapon index at entity +2a4/+2a8
differing from -1 is sufficient. Otherwise the separate owner pointer at +2a0
must refer to an owner whose class base speed (+294 -> +50) is zero, matching
0x40a2a0. 0x427da0 supplies the first occupant handle differing from -1 from
the owner's +8cc seat list. Only that handle is resolved, then weapon presence
is checked recursively. A stale first occupant does not cause a search for
another seat. The original repeats failed traversal twice; stable views allow
the same result from one traversal. Non-finite owner speeds are rejected;
cyclic chains return RF_FORMAT after the registry bound instead of overflowing
the original call stack. Null input has no weapon in the shared API.

Readiness follows 0x41f950. Entity +810 bit 0x10 immediately succeeds. Otherwise
a weapon is required; +7c bit 8 or an attached type-zero entity whose linked
handle (+200) equals this entity's full handle succeeds next (0x48aaf0).
The attachment input is an ordered snapshot of handles from the original
0x7c75cc circular list, not a new ownership system. With no such attachment,
actions +520 equal to 7/16 or +7d0 bit 4 reject readiness. Bit 2 succeeds for
a living entity; otherwise bit 0 supplies the result (0x408e90).

The full combat gate also requires +810 bit 0 clear (0x427020), and rejects
+810 bit 0x800 unless a valid linked type-zero entity has class 4 (0x428e60,
0x429f90). Ready and eligible are distinct outputs: a forced-ready or attached
entity can still fail the complete gate. Outputs remain unchanged on malformed
views. Slot allocation, generation changes, seat/list mutations, class loading
and gameplay entity creation are not implemented by these APIs.

`tools/verify_entity_predicates.py` executes all original callees unchanged for
2,007 fixtures, comparing object/entity lookup, weapon presence, readiness and
eligibility. It includes generation mismatches, slot 1,023/1,024 boundaries,
negative valid handles, null/stale pointers represented through the registry,
seat chains and attachment links. A separate C-only fixture rejects a cycle.
The local report is `artifacts/entity-predicates-verification.json`.

The shared animation diagnostic now uses an equipped-weapon index and +7d0 bit 2
to derive eligibility, replacing its forced combat flag. It checks both outputs
against original instructions in all 64 frames and includes them in its state
hash. It remains a scripted miner-rig fixture; later physics/state selection,
reset/audio adapters and actual gameplay initialization remain open.


Factory vitals assignments (2026-09-11)
--------------------------------------

rf_entity_creation_vitals reconstructs two numeric blocks in422360, using a
compact caller-owned state. Original422a80..422a9e copies class+48 to entity+38
(armor) when the low network-mode byte is zero; otherwise it writes positive
zero. Original422cb4..422cea compares class+44 health against zero with x87,
copies class+764 to entity+840, and assigns health/object flags. Negative or
unordered health produces100 and ORs bit4 into object+7c. Nonnegative health,
including negative zero, copies its original bits without clearing existing bit4.
The class+764/entity+840 field is retained without a guessed semantic name.

These blocks occur at different points in the factory with other calls between
them. The helper composes only their numeric writes using caller-supplied class
values; it does not reconstruct those intervening calls, generic allocation,
class parsing, later overrides or a finished entity. It does not reset unrelated
damage state. Actual source metadata and persistent NPC ownership remain required
before attaching the existing damage/burn runtime. The current scene registers
only its compact player view; raw level NPC records are not live NPCs.

verify_entity_creation_vitals.py executes both original blocks and checks every
other byte in the0x1500-byte actor remains unchanged. It compares1,536 cases with
PC and actual NXDK-linked C, spanning initial object flags, signed zero, positive/
negative health, infinities, quiet/signaling NaNs and low-byte network values.
This includes bit-preserving SP armor copying. PC rf_entity_probe
--creation-vitals consumes32-byte cases and returns16-byte projected states.
Report:artifacts/entity-creation-vitals.json. Both full builds and nine CTests pass.
No native XEMU actor construction or new visual behavior is claimed.
