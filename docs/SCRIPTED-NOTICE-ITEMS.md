# Scripted keycard and Brainstem notices

Source-written on 2026-10-09. The helper is parent-integrated and awaits the next hourly compilation. No build, test, syntax check, emulator run, fixture, grant, image,
campaign-route search, or original-input modification was performed.

## Concrete remaining consumer

The old TO-DO statement that all non-weapon scripted grants are unsupported is
obsolete. Current `campaign_give_item` and `campaign_apply_item_grant` already
handle Medical Kit, First Aid Kit, Suit Repair, Miner Envirosuit and Doctor
Uniform. Their bounded startup queue, inventory-replay guard, class caps and
difficulty-scaled vital grants remain in place.

A read-only inspection of the original event records in installed
`levels1.vpp`, `levels2.vpp` and `levels3.vpp` found seven Give_Item_To_Player
records across 68 RFL files. Five item names already resolve. The two remaining
original records are in `levels2.vpp/L8S3.rfl`:

- UID10085: `keycard`
- UID10380: `Brainstem`

Both currently return RF_NOT_FOUND from the no-weapon name mapping. The shared
event dispatcher counts them as other targets and does not stop generic link
propagation. This is missing authored pickup feedback, not evidence that an
unimplemented key inventory blocks a door or campaign progression. No event
links or campaign route were traversed for this inspection.

## Original evidence and scope

Installed `tables.vpp/items.tbl` defines both classes with Count1 and no
`Ammo For`, `Gives Weapon` or `no_pickup` flag. Their authored notices are
"Keycard picked up" and "Brainstem picked up".

Original RF.exe458960 clears the class callback fields, then4589a6..458a3d
registers Suit Repair, Miner Envirosuit, Medical Kit, First Aid Kit and the
multiplayer powerups. It registers neither keycard nor Brainstem. Give_Item
4bb73e checks the class callback at+0x48; absent that callback,4bb769 calls
45a3d0. With both weapon/ammo fields absent,45a4cc..45a4ed calls45a100 for a
player. That function formats the authored pickup message and calls4383c0;
it does not grant a weapon, retain a key bit, change a goal, equip a form or
play a pickup sound. This helper follows that existing Doctor Uniform policy
for these two explicitly authored names only.

## Exact parent integration

Parent owns `src/diagnostic/scene.c`; this worker changed no existing file.

1. Include `scene_scripted_notice_items.inc` immediately after the body of
   `campaign_pickup_notice_grant`, before `campaign_miner_suit_grant` and
   `campaign_apply_item_grant`.
2. In `campaign_apply_item_grant`, after the existing request->weapon==-1
   strip branch and before the suit/weapon dispatch, add:

   ```c
   if(scene_scripted_notice_item_apply(request))return RF_OK;
   ```

3. In `campaign_give_item`, replace only the final `:-7` in its no-weapon
   class-name selection with:

   ```c
   :scene_scripted_notice_item_id(name);
   ```

   Preserve the following `if(request.weapon==-7)return RF_NOT_FOUND`, the
   parsed count/messages, no_pickup rejection, queue capacity check and
   `campaign_startup_inventory_replay` guard unchanged.

The new request tags are-8/-9; the existing-7 unknown sentinel is untouched.
The helper handles those tags before weapon-index access. Successful requests
use the existing notice lane and counters. Non-weapon diagnostic kinds0x100
and0x101 identify these scripted-only notices; they are never placed-item
resource indices and do not change SCENE_PICKUP_CLASSES.

No new memory allocation, persistence field, startup ledger, inventory state,
ownership change, item model, world pickup, event activation, level transition,
or sound request is introduced. Startup grants still wait until inventory
initialization and repeat startup remains suppressed by the existing guard.
Ordinary repeated events may repeat their notice, matching the existing
notice-only dispatch rather than fabricating permanent acquisition state.

The parent hourly Xbox batch is the first compilation opportunity. This patch
does not claim runtime or rendered-HUD validation.
