# Secondary weapon request admission

This core-code slice is based on combined vehicle checkpoint
c29d6f23f978e50eacfe957aca1f8e9f846707bb and was integrated after its12:00 UTC
NXDK build. This helper awaits the13:00 batch; no helper compile, test,
original executable run, fixture or emulator session was performed.

## Implemented boundary

`rf_weapon_secondary_request_admit` reconstructs the recorded426ca0..426d73
admission prefix with explicit state corresponding to owner+4bc/+504/+508:

1. Dying predicate AL exactly1 rejects.
2. A non-bypass request requires the shared wrapped deadline to be due;
   a disabled deadline does not count as due.
3. Negative secondary weapon ID or nonpositive available ammo writes
   now+500 through the existing timer helper and requests local-player-only
   empty feedback. Missing inventory ownership rejects without that backoff.
4. A non-bypass request with weapon flags+264 bit0x08000000 writes the
   scheduled timestamp and copies descriptor+444 unchanged into pending count.
5. Otherwise it returns direct admission to the later muzzle-selection stage.

The original low-byte bypass skips only timing and scheduling. It does not
skip death, selected-weapon identity, inventory ownership or available ammo.
The helper consumes stable, already-resolved predicate/ammo/descriptor facts;
resolving live owners and invoking downstream effects remain caller-owned.
A successful direct result is not a spawned projectile or ammunition debit.
No affiliation, target acquisition or Goto_Player heuristic appears in this API.

## Evidence and recovered scheduler

The original recorded admission audit remains in
`research/FUTURE-VEHICLES-SECONDARY-ADMISSION-20260915.md`. Its historical
576 cases were not rerun here. The original RF.exe was recovered after this
slice; the earlier cloud-transfer blocker is resolved. See
`VEHICLE-AI-REFERENCE.md` for the exact source hash, actual Goto_Player/default
combat chain, scheduled-shot consumer and secondary AI gate now reconstructed.

The shared request gate still does not launch a projectile, choose an AI target
or debit ammunition. Downstream live-owner integration remains separate.
