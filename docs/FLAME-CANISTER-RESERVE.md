# Flamethrower alternate full-reserve admission

Source-written against `546cf385` on 2026-10-10. Compilation and runtime remain
unverified pending the parent-coordinated Xbox batch. Existing focused unit
assertions are source-updated but unrun. No new gameplay fixture, build, test,
route, original-input edit, PC run or emulator execution was performed.

## Original evidence

Authority is read-only `Installed_Game/RF.exe`, 1.20 NA, SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Direct static x86 inspection establishes:

- `4a5168..4a5176` enters this special admission only for alternate mode and
  the Flamethrower ID at `87243c`. The name binding is `4c6654..4c665e` and
  the string at `5a335c` is `flamethrower`.
- `4a519a` reads mapped ammo type, `4a51a0` reads actor reserve and `4a51a7`
  reads the active magazine. `4a51ad..4a51af` requires signed reserve >=
  magazine; insufficient reserve immediately returns at `4a51b1..4a51b8`.
- Delayed launch `4a281b..4a2984` consumes expired timers, attempts the
  projectile factory at `4a28fa`, and calls ammo commit `42c310` at `4a296f`.
  There is no explicit reserve recheck in that caller or the Flame debit.
  The original assumes prior admission; it reaches the debit even when the
  factory returns null at `4a2904`.
- `42c362..42c386` overwrites loaded with the full active magazine and
  subtracts exactly that magazine from mapped reserve. It does not read the
  old loaded count or clamp/partially refill.
- `4a297c` then calls empty handling `4a6f10`. Its `4a6fea..4a6fff` check
  returns without replacement while loaded plus reserve is positive.

Read-only `Installed_Game/tables.vpp` contains `weapons.tbl` at archive offset
1013760, size 108890. The Flamethrower record specifies gas, clip 100 SP / 200 MP,
capacity 1000, alternate impact delay 1.8 seconds and alternate wait 4 seconds.
Thus an admitted SP throw with reserve 100 leaves loaded 100 / reserve 0. It
does not create final-ammo depletion. The previously documented low/zero-reserve
throw was explicit first-pass port policy, not a missing original replacement
consumer. No new ranked-selection request or operation kind is appropriate.

## Bounded correction

`scene_flame_canister_tick` now requires the actual mapped reserve to contain
a full magazine before accepting the alternate edge. A refusal does not start
the pending throw, cooldown, fire presentation counter or launch action sound.
Existing held-edge tracking remains in place, so holding a rejected alternate
does not create a later launch without another edge.

After the existing pending countdown expires, a second guard declines release
if current reserve no longer contains a full magazine. This is explicitly
defensive port safety for changed inventory or a restored legacy pending throw,
not a claim about a second original admission check. The pending throw has
expired; existing cooldown and held history remain, with no spawn, debit,
publication or release counter change.

Only after a successful bounded projectile launch does the adapter install the
full magazine and debit that amount from reserve, then clear fractional fuel
and publish ammunition through the existing path. Pool refusal and launch
failure still cost no ammunition. This retained success-only policy differs
from the original factory-failure merge and is not changed by this correction.

Existing positive-loaded, ownership, selected/blocked, death/swim/reload,
finite-pool and timing gates remain. Live canisters still update before action
admission, including off-weapon. No primary fuel/reload logic, burning owner,
input reset, selection request, allocation or save field changes. RFAP3 keeps
its current decoder and guards: legacy underfunded pending throws can still
restore, then fail the defensive live release check normally. Existing
historical runtime reports remain historical; they do not validate this change.
