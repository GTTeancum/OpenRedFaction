# Settled airlock pressure checkpoint extension

Status: source-written only against `6a3d0446dd9885f799ac0430235abd11e3e8a389`.
No compilation, syntax check, tests, fixtures, route, emulator run or runtime
claim accompanies this codec slice. Independent source review and parent
integration precede the scheduled parent-owned Xbox batch.

## Owner and compatibility

`rf_campaign_trigger_state` appends two `uint32_t` fields:
`airlock_chamber_uid` and `airlock_pressure`. The latter is an encoding, not the
raw pressure: `0` means legacy/unowned, `1` means dynamic pressure zero, and `2`
means dynamic pressure one. An absent lane is canonical `(0,0)`. An explicit
lane accepts any chamber UID except `UINT32_MAX`, including UID zero.

The full-session trigger ledger remains the sole persistent owner, keyed by
canonical section name and authored trigger UID. All retained explicit rows
with the same `(level,chamber UID)` must agree on pressure. This applies to
previous sections as well as the current section and does not depend on the
trigger's disabled, exhausted or removed flags. No pressure state is duplicated
in RFCH or RFEN, and original static room geometry is never modified.

The original RF.exe behavior distinguishes dynamic chamber pressure at room
`+44` from the serialized static atmosphere byte. Actual selected L11S3 triggers
2658 and 6298 share chamber 2453, whose initial dynamic pressure is one. Settled
pressure changes affect whether subsequent accepted work needs equalization.
These authored facts are supplied by the parallel source inspection; the codec
only validates portable state and does not hard-code this level or its UIDs.

## Wire format

The 64-byte little-endian RFTC header is unchanged: magic, version, total byte
count, FNV-1a checksum (its own four bytes treated as zero), level count, row
count, 32-byte identity and eight reserved zero bytes. Each level name occupies
64 bytes. Existing 40-byte row offsets remain unchanged:

- 0: level index; 4: trigger UID; 8: captured/retired marker
- 12: flags; 16: activation count; 20: object flags
- 24: activation clock bits; 28: signed activation limit
- 32: signed cooldown remaining; 36: signed contact remaining

RFTC2 uses 48-byte rows, appending chamber UID at 40 and pressure encoding at 44.
RFTC1 remains the output whenever no retained row contains explicit pressure;
otherwise the writer uses RFTC2 for the entire full-session ledger. RFTC1
reader sets both appended fields to zero. The decoder also accepts an RFTC2
stream containing only canonical absent lanes; a subsequent encode normalizes
that stream to RFTC1. Unknown versions and inconsistent version/row lengths
are rejected.

Existing API signatures are unchanged: encode, preflight and decode all retain
their identity parameter and error behavior. No buffer or output owner is
published before all validation succeeds. Decode zeroes its destination only
after complete preflight. Encode leaves `written` and the destination untouched
on error. Buffers and owners must remain disjoint, as before.

All original bounds, name validity and case-insensitive uniqueness, row-key
uniqueness, non-sentinel trigger UID, captured marker range, contact-bit64
exclusion, timer bounds, reserved bytes, identity and checksum checks remain.
Pressure adds only canonical absence, encoding range, non-sentinel chamber UID
and explicit-peer consistency checks. Count, object flags, activation bits and
limit remain the original opaque state fields; their authored/gameplay meaning
continues to belong to the scene and ordinary trigger logic.

## Scene integration obligations

The generic `rf_runtime_trigger_save` now zero-initializes its local result so
new fields cannot contain uninitialized bytes. It intentionally does not know
about chamber ownership. Scene capture must add the current settled pressure
after generic capture for handoff, ordinary snapshot and diagnostic checkpoint
probe, preserving earlier sections in the existing ledger.

The codec cannot prove a trigger's authored chamber, current loaded room,
supported door topology or completeness of the chamber peer set. The scene
must prove exact current-level authored membership and require all relevant
peers to carry coherent explicit values or the coherent legacy baseline.
Mixed legacy/explicit peers cannot be inferred from a canonical absent row
alone, since that row has no chamber identity. Disabled/removed triggers remain
pressure peers and must not silently drop the chamber's history.

Legacy absence reconstructs the original authored chamber baseline; it must
not inherit a pressure already mutated in the live scene. Loading replaces the
whole session trigger ledger, so earlier pressure omitted by an old checkpoint
cannot leak through. Candidate-first pressure publication belongs
to scene integration; installation must wait until the load and storage close
fully succeed, or restore prior owner state on failure.

Accepted actor identity and equalization/dispatch deadlines are runtime-only.
Save and section handoff must reject owned pending/faulted work before capture;
this extension does not serialize handles, actor identities, raw timers, links,
controller state or queued side effects. The parent/runtime slice owns those
guards and original ordered dispatch, including a Load_Level link.

## Stock-64-MiB budget

Sizes below are source-derived fixed-layout budgets, not runtime measurements:

- Per trigger state: 28 to 36 bytes, plus 8 bytes.
- Full 4096-slot in-memory ledger: 172,040 to 204,808 bytes, plus 32,768 bytes.
- RFTC1 maximum: 64 + 128*64 + 4096*40 = 172,096 bytes, unchanged.
- RFTC2 maximum: 64 + 128*64 + 4096*48 = 204,864 bytes, plus 32,768 bytes.
- An RFTC2 stream grows by 8 bytes per retained row, even rows with absent
  pressure; a one-level/two-row stream is 224 bytes instead of 208.
- The codec performs no allocation and keeps only one 36-byte temporary state
  during preflight. Existing bounded duplicate checks remain quadratic; peer
  comparison shares those loops instead of adding another owner or table.
- Each live, snapshot, candidate or rollback ledger copy grows by 32 KiB. Parent
  scene integration must include simultaneous copies in its peak budget; this
  codec change does not claim an end-to-end memory or save-capacity pass.

The unchanged `RF_CHECKPOINT_FILE_MAX` is 110,524 bytes for the entire ordinary
checkpoint, already below the theoretical full RFTC1 ledger maximum. The new
per-row wire cost can reduce remaining capacity in a long session. The codec
retains exact capacity rejection and this slice does not enlarge that global
limit, truncate older history or claim full-capacity campaign saves.


## 14:00 build evidence

The 2026-10-10 14:00 Xbox build compiled this source at `1fd5bfba`. The accompanying original L3S1 neutral startup passed on stock 64 MiB, but that level has no authored airlock. Airlock action, room-owner admission and pressure/save restoration remain runtime-unverified. See [the hourly report](HOURLY-20261010-1400.md).
