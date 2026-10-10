# Authored prop destruction watchers

Source-written 2026-10-09 after the parent's successful 07:00 Xbox batch on
`f531af1a`. This slice was not compiled or run; it awaits the consolidated
08:00 batch. No test, emulator, fixture, original-code execution or campaign
traversal was performed. The parent owns the separate `scene.c` hook.

## Concrete gameplay gap

`src/core/event.c::runtime_death_poll` already processes `When_Dead` type16.
It treats an unknown linked object as unresolved and withholds every outgoing
effect. `src/diagnostic/scene.c::campaign_death_query` previously recognized
turrets, NPCs and vehicles only. Registered clutter therefore remained unknown
even after ordinary weapon damage, the new `Slay_Object` adapter or scripted
removal had retired it.

Read-only decoding of the three installed original SP archives found 21
clutter-linked watchers: 19 campaign records and two training records. This is
an authored missing gameplay consumer, rather than an unsupported hypothetical
object type. Representative chains:

| Archive / level | Watcher | Required clutter | Outgoing gameplay |
| --- | --- | --- | --- |
| levels1.vpp / L5S2 | 4402 | Pump01 4037,4072 | Alarm4403, Goto_Player4471/4472, Message4407, Switch4404, UnHide4470 |
| levels1.vpp / L6S1 | 5424 | DesktopPC_CPU4774 | Slay_Object5425 destroys monitor4771 |
| levels2.vpp / L7S2 | 5330 | Console_Small01 4968 | Slay_Object5331 destroys button4974 |
| levels2.vpp / L8S2 | 9384 | DesktopPC_CPU6793 | Slay_Object9385 destroys monitor6792 |
| levels3.vpp / L17S2 | 20675 | Eight CentralComp_Slave01 placements | Message20676, Goal_Check20678 for pod, Delay20684, Goal_Set20787 for goblow |
| levels1.vpp / train01 | 7213 | Sixteen Ultor Barrel Yellow placements | Message8046, Invert8765, Remove_Object8773 |
| levels1.vpp / train02 | 7536 | Twelve beerbottle01 placements | Message8255, Invert9055, Remove_Object9070 |

L17S2's exact eight authored targets, in watcher20675 link order, are
18504,18506,18509,18507,18454,18455,18452,18453. The existing class table assigns
CentralComp_Slave01 life300 and `collide_object`; Pump01 has life400 and an
explosive damage factor of3. Both use ordinary static model classes. Their
damage consumer already exists; the watcher did not receive their life state.

The remaining watchers are L3S1:7022; L3S2:8792,8797,8802;
L14S3:10210; L15S1:9721; L15S4:19837; and
L17S2:18561,18562,18563,18564,18576,20668,20670. L14S3:10210 uses the
optional any-missing condition, making retained death distinct from actual
registry absence. This slice does not claim that optional path is complete.

The existing read-only parser layouts are documented by
`tools/inspect_events.py::inspect`, `tools/inspect_levels.py::inspect`,
`tools/inventory.py::archive` and `src/core/level.c::clutter_scan`. The source
review decoded bytes in memory without extracting assets or writing inventories.

## Original event contract and bounded port policy

`docs/WHEN-DEAD.md` records the verified original executable's type16 constructor
`4be6c0`, vtable `5899ec`, update `4bb3a0`, registry lookup `40a0e0` and
living-bit predicate `48b450`. An active timer or fired byte suppresses polling;
otherwise no linked object may be living, with authored flags[0] additionally
allowing any missing object. `4bb43c` sets one-shot state before effects. The
automatic poll does not acquire the watcher's authored delay; downstream
events keep their own existing schedules. This slice does not change any of
those shared dispatcher rules or claim exact original cleanup timing.

`scene_clutter_death_query.inc::campaign_clutter_uid_life` adds only the scene's
missing borrowed life query:

- Resolve the authored UID in the retained clutter slots, then require the
  current generation-qualified registry handle to resolve to that exact owner.
- Require matching owner UID, initialized damage-binding handle/class and class
  definition pointer. Mismatched identity, nonordinary classes, absent model
  owners and stale generations remain unknown; they are never invented deaths.
  Nonfinite health returns an error. Every unsuccessful result leaves both
  outputs unchanged.
- A validated registered ordinary owner is present. Positive health without
  retirement bit2 makes it living. Hidden bit0x4000 affects neither decision.
- Retirement bit2 reports nonliving state while retaining presence. Ordinary
  lethal damage/Slay, `Remove_Object` and GeoMod retirement use this bit; the
  latter paths can retain positive health, so health alone is insufficient.
  Neither bit2 nor zero health proves registry absence for the optional
  any-missing gate. The shared poll already recognizes an actually missing
  previously resolved handle before calling this UID query.

This follows the port's retained dead turret/active-vehicle query policy and
avoids prematurely firing an optional watcher while another object is living.
Clutter currently has no persisted cause distinguishing explicit removal from
damage/GeoMod retirement. Consequently mixed living plus explicitly removed
props under flags[0]'s any-missing mode remain outside this conservative slice;
adding that behavior requires a separately justified lifetime distinction.
All concrete pump, computer, training and upstream Slay chains above use
flags[0]=0 and need only the supplied nonliving state.

The helper does not unregister props, replay breaks, change health, move an
owner, create a corpse, alter visibility or infer death from a missing resource.
It allocates nothing and retains no new state. `break_pending` and killing type
are deliberately not used as life truth because they describe effects/history
and can be cleared or retained independently of death.

## Parent scene hook

Include `scene_clutter_death_query.inc` immediately before
`campaign_death_query`, after its nearby `scene_vehicle_uid_life` forward
declaration. Replace only that query's final `return RF_NOT_FOUND;` with
`return campaign_clutter_uid_life(uid,present,alive);`. Preserve all existing
turret/NPC/vehicle branches and the shared core poll.

## Existing save ownership

`scene_clutter_checkpoint_live.inc` captures/restores health and mask0x204002,
which includes retirement bit2 and hidden bit0x4000, while retaining owner/UID
and class identity. It rejects pending break work on capture and clears
`break_pending` on restore. The query reads these restored values directly.

`src/core/event_checkpoint.c` already serializes the watcher's `death_fired`
and `death_time`; no new watcher state or save-format field is introduced.
The existing `scene_world_load.inc` commit publishes clutter at its assignment
phase and subsequently assigns event state before returning to simulation.
A restored fired watcher therefore retains the shared one-shot guard, and an
unfired watcher uses the joined restored prop state on its next ordinary poll.
No new cross-section prop persistence is claimed.

## Verification boundary

The written helper and separate parent hook have source-review evidence only.
The 07:00 grate destruction/release pass preceded this change and cannot prove
death-watcher dispatch. Existing core watcher coverage lives in
`tests/mission_goal_tests.c::death_watch_check`; it was read, not run or changed.
Authored prop watcher runtime, downstream goal/Slay effects and ordinary-save
continuation remain unverified until the parent's scheduled Xbox validation.
