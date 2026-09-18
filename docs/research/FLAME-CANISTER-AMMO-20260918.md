# Flamethrower alternate canister ammo — 2026-09-18

The delayed ammo callee **replaces the loaded clip with a fresh full clip and subtracts that full clip from reserve**. It does not simply empty the loaded clip. This is static instruction evidence, not a new original-game execution or complete input-admission proof.

Evidence: local `Installed_Game/RF.exe`, expected SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`; inspected mapped x86 instructions with existing local pefile/Capstone. Prior bounded evidence: `docs/research/secondary-re/weapons-remote-launch-ammo.md`, `tools/future_re/secondary/weapons_remote_launch_ammo.py`, and `tools/verify_weapon_supply_binding.py`. No new broad probe, game launch, build, or production edit.

## Exact arithmetic

`42c310` first checks player-object bit8 through `4895d0`. Weapon ID -1 and Riot Shield skip. `42c34d` compares the current weapon ID against global `87243c`, established as the named Flamethrower by `4c665e`.

For that branch:

- `42c355..42c35f`: compute weapon index times `0x550`.
- `42c362`: load active magazine from `85cd90 + index*0x550`.
- `42c368`: **overwrite** actor loaded ammo at `actor + 0x32c + 4*weapon` with that magazine value.
- `42c36f`: load ammo type from `85cd2c + index*0x550`.
- `42c375..42c386`: read reserve at `actor + 0x2ac + 4*ammo_type`, subtract the magazine, store, return.

Active magazine is descriptor+0x88, SP-selected from authored Clip Size by `4c2a20`, already covered by retained weapon-supply binding. Installed Flamethrower SP clip is100 (MP200), gas reserve capacity1000. Thus, for valid sufficient reserve:

```text
loaded_after = magazine                  # 100 in SP
reserve_after = reserve_before - magazine
```

The previous loaded amount is not read in this branch. Loaded37/reserve250 becomes loaded100/reserve150: the old partial tank is discarded as the projectile is thrown, then a fresh tank is supplied. Loaded100/reserve100 becomes loaded100/reserve0. No clamp, reserve check, or partial-refill branch exists inside42c310. The raw subtraction wraps at machine-word width; this does not prove ordinary input admits reserve below100. Do not reproduce negative reserves in the bounded port inventory.

## Timing and failure boundary

The existing delayed-launch caller attempts factory `4c77a0` at `4a28fa`. The null result at `4a2904` joins the same SP path as success. It reads the current weapon at `4a2966`, calls42c310 at `4a296f`, then post-debit handling4a6f10 at `4a297c`. Therefore original delayed ammo commit is **after the factory attempt, even if it returns null**, before post-debit notification. It is not an input-time debit or ordinary per-shot4257c0 operation. Keep the committed weapon identity stable across that attempt.

Installed table timing remains Alt Impact Delay1.8s / Alt Fire Wait4.0s. These authored values establish configuration, not a new end-to-end firing timing execution.

## Safe integration delta

In `scene_flame_canister.inc`, replace the current successful-release `loaded[weapon]=0` policy for the sufficient-reserve domain with the loaded/reserve assignment above; clear fractional gas remainder and publish ammo once. Do not call normal reload transfer: it would subtract only the missing loaded amount and produce different totals for a partial tank. The existing primary fuel drain must not also run on this release frame.

For reserve below a whole tank, upstream admission and post-debit4a6f10 need further proof. A practical explicit port policy is `refill=min(reserve,magazine); loaded=refill; reserve-=refill`, including empty reserve -> empty loaded; this is **not established original behavior**, but avoids inventing gas or negative inventory while retaining the usable thrown tank. Alternatively restrict the first-pass exact branch to sufficient reserve. Parent must choose and document that boundary.

The current adapter also debits only successful allocation. Keeping no-charge-on-failed-spawn is a defensible port policy but differs from the proven original caller merge. A pool-capacity refusal before attempting launch and a factory attempt returning null are distinct cases; do not silently retry an already committed release and debit twice.
