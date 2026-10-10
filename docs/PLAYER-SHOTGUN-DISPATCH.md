# Player Shotgun shell dispatch

## Scope and status

Source-written against `28ad4190a4337950efe29c5c5b164ba0616034de`, integrated after the completed 06:00 UTC batch and independently source-reviewed.
No new build, test, emulator session, fixture or fixture-ammo change was performed.
Compilation and runtime behavior remain unverified until the next parent-owned
hourly Xbox batch.

Only the admitted player slot-3 dispatch in `campaign_combat_tick` changes.
Shared weapon APIs, NPC firing, input/trigger ownership, reload, save formats,
resource admission, range, per-ray damage and collision ownership are unchanged.

## Original evidence and authored data

The reviewed original `RF.exe` dispatch establishes separate requested and
effective modes:

- `0x4259aa..0x4259bf` captures the requested firing wait before fallback.
- `0x425c0c..0x425c38` turns an accepted Shotgun primary with exactly one loaded
  shell into effective alternate; actual alternate stays alternate.
- `0x425c39..0x425c6d` owns the effective-primary double-shot bit `0x1000`.
- `0x4265af..0x4265ca` doubles the authored projectile count for effective primary.
- `0x4267c4..0x4267db` performs the additional primary shell debit.
- `0x426b2c..0x426b37` uses the previously captured requested firing wait.
- `0x4262bd..0x4262cf`, `0x42695e..0x426997` and `0x426c0b..0x426c14`
  select spread, launch sound and action from the effective firing mode.

The existing installed-table bindings retain four authored projectiles,
primary/alternate spread of 3/6 degrees, primary/alternate waits of
1.5/0.225 seconds, an eight-shell magazine and 40 damage per ray in both modes.
Those bindings are recorded in `ENEMY-AI.md` under Shotgun resource/rules
support. The patch does not hardcode replacements or change the table parser.

Accepted-fire outcomes with positive pre-debit loaded ammunition are:

| Requested mode | Loaded | Effective mode | Shell debit | Rays | Spread | Requested wait |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| Primary | At least 2 | Primary | 2 | 8 | 3 degrees | 1.5 seconds |
| Primary | 1 | Alternate | 1 | 4 | 6 degrees | 1.5 seconds |
| Alternate | At least 1 | Alternate | 1 | 4 | 6 degrees | 0.225 seconds |

The existing 60 Hz trigger quantization remains: primary uses 90 ticks and
actual alternate uses `ceil(0.225 * 60)`, or 14 ticks. A one-shell primary does
not acquire alternate cadence. Simultaneous buttons retain primary precedence.
Zero shells remain in the existing dry/reload path and never enter this table.

## Wiring and failure semantics

The requested `alt` calculation and `rf_weapon_trigger_step` rules are left
verbatim, including cooldown, inhibit, held-input and ammo admission. Effective
mode is resolved only after reload, dry and no-fire returns, immediately before
real consumption. Thus an idle one-shell weapon cannot manufacture held
alternate input or an unsolicited shot.

At that accepted site, slot 3 uses
`alt = requested_alt || pre_debit_loaded == 1` and records a call-local shell
count of one for effective alternate or two for effective primary. The existing
consumer runs once, followed by a second call only for effective primary and
only if the first succeeded. Both calls precede the sole
`scene_weapon_empty_note(..., SCENE_WEAPON_EMPTY_SHOT)` and
`campaign_ammo_publish()`.

`rf_weapon_consume_shot` in `src/core/weapon.c` is not an admission helper:
invalid out-of-range weapon IDs return `RF_OK` without a debit, malformed
pointers/count/ammo mapping return `RF_RANGE` before mutation, and valid
magazine debits clamp the resulting amount at zero rather than signaling an
empty magazine. Existing slot/ownership/resource admission supplies a valid
Shotgun magazine. The positive pre-debit count selects the one-shell fallback
before either debit; all other admitted primary counts are at least two.
There is no callback or metadata mutation between the two identical helper
calls, so the second cannot encounter a new validation failure after the first
succeeds. The first failure skips the second, receipt and publication. No
speculative debit, rollback or helper/API change is introduced.

This port deliberately retains its existing debit-before-rays order: primary
now completes both real debits before sound/hearing and the pellet loop. This
is not a claim to reproduce the original's second-debit side-effect ordering.
A later spread/contact/hearing error retains the existing nontransactional
shot semantics; it does not refund consumed ammo or invoke empty selection.

Effective `alt` feeds the existing `Shotgun Fire`/`Shotgun Fire 2` sound choice,
`campaign_last_alt`, 3/6-degree spread choice and authored per-ray damage choice.
The existing first-person action consumer reads that accepted mode. There is
still one COMBAT shot increment, one launch-sound decision and one hearing
operation per admitted shot. No second trigger or action publication is added.
The ordinary post-combat empty-selection owner still runs once after all
outgoing rays succeed and retains its existing presentation behavior.

The loop bound becomes authored projectiles multiplied by the shell count.
Each ray still makes exactly one call to the existing deterministic sampler,
starts with the same 100-unit eye-forward delta and enters the unchanged
shield/prop/vehicle/fragment/world/body contact and damage path. Primary emits
eight successive samples; effective alternate emits four. No ray is copied,
resampled or changed to an aggregate damage pulse.

## Diagnostics and historical expectations

`SHOTGUN[0]` keeps its shell meaning and now adds the actual shell count.
`SHOTGUN[4]` counts effective-alternate shells, including one-shell requested
primary fallback. Pellet, hit, kill, RNG and status words keep their existing
meanings. Non-Shotgun behavior retains the single-ray/single-debit path.

Historical primary-shot runtime expectations are obsolete for this corrected
source and must not be presented as validation of it:

- `tools/replay_shotgun.py` expects five shells/twenty rays after one primary
  and four alternates, plus three shells remaining. Those values and dependent
  damage/RNG outcomes reflect the former one-shell/four-ray primary.
- The primary branches in `tools/replay_shotgun_state.py` expect seven shells
  after a shot and one-shell manual transfer; its return case preserves seven
  shells. Those primary-dependent counts require reconciliation. The
  alternate-only automatic-reload case is structurally unchanged, but its
  historical run does not validate this revision.
- `docs/ENEMY-AI.md` now explicitly labels its September 14 primary/reload/
  transition evidence as historical. Older route records such as
  `docs/MAIN-SHAFT.md` also retain prior primary pellet/ammo/damage/RNG behavior;
  no campaign replay is authorized or needed for this source slice.

The historical tools and their ammo inputs are preserved, not adjusted to force
a pass. New primary, true-alternate and last-shell runtime evidence remains
pending within the single parent-owned scheduled batch workflow. No validation
result or new gameplay fixture is claimed by this change.
