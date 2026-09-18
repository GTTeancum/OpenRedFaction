/* Include after scene_machine_pistol_mode.inc. Session-only sidecar to the
 * existing campaign handoff; NOT a disk format and not an inventory grant.
 * An exact exported-state match prevents a stale sidecar from attaching its
 * mode to unrelated imported inventory. Keep a pending copy before destroying
 * the old scene. Reset both copies for a new campaign or disk checkpoint load.
 * Capture only after rf_campaign_player_copy successfully published export;
 * restore only after incoming inventory/catalog is accepted and resources are
 * ready, before campaign_select_primary. Incomplete switches retain their last
 * completed mode; all transient input/timing state is cancelled on restore. */
#include "rf/campaign.h"
#include <string.h>
typedef struct scene_machine_pistol_carry {
    rf_campaign_player_state player;
    int32_t base_weapon,special_weapon;
    uint32_t valid,special;
} scene_machine_pistol_carry;
static void scene_machine_pistol_carry_reset(scene_machine_pistol_carry *carry)
{if(carry)memset(carry,0,sizeof(*carry));}
static int scene_machine_pistol_carry_capture(scene_machine_pistol_carry *carry,
    const rf_campaign_player_state *player,const scene_machine_pistol_mode_state *mode,
    int32_t base_weapon,int32_t special_weapon)
{
    scene_machine_pistol_carry value={0};int status;
    if(!carry || !player || !mode || base_weapon<0 || base_weapon>=64 || special_weapon<0 ||
       special_weapon>=64 || base_weapon==special_weapon || mode->special>1)return RF_RANGE;
    /* Special is a mode of an owned base gun, never an independent item. */
    if(player->inventory.owned[special_weapon] || player->weapon==(uint32_t)special_weapon)return RF_FORMAT;
    status=rf_campaign_player_copy(&value.player,player,player->catalog_hash);if(status)return status;
    value.base_weapon=base_weapon;value.special_weapon=special_weapon;
    value.special=player->inventory.owned[base_weapon]?mode->special:0;value.valid=1;
    *carry=value;return RF_OK;
}
static int scene_machine_pistol_carry_matches(const scene_machine_pistol_carry *carry,
    const rf_campaign_player_state *player,int32_t base_weapon,int32_t special_weapon)
{
    if(!carry || !player || !carry->valid || carry->base_weapon!=base_weapon ||
       carry->special_weapon!=special_weapon)return 0;
    return carry->player.catalog_hash==player->catalog_hash && carry->player.weapon==player->weapon &&
        carry->player.health==player->health && carry->player.armor==player->armor &&
        !memcmp(&carry->player.inventory,&player->inventory,sizeof(player->inventory));
}
static int scene_machine_pistol_carry_restore(const scene_machine_pistol_carry *carry,
    const rf_campaign_player_state *player,int32_t base_weapon,int32_t special_weapon,
    uint32_t alternate_held,scene_machine_pistol_mode_state *mode)
{
    scene_machine_pistol_mode_state restored={0};
    if(!mode)return RF_RANGE;
    if(!scene_machine_pistol_carry_matches(carry,player,base_weapon,special_weapon))return RF_NOT_FOUND;
    restored.special=carry->special;restored.held=alternate_held!=0;*mode=restored;return RF_OK;
}
/* Call in existing RFCP scope admission before encoding: the current format
 * cannot represent separate unowned special loaded rounds or retained mode.
 * Reserve alone is not a veto: its ammo class can be shared with another gun. */
static uint32_t scene_machine_pistol_disk_pending(const rf_weapon_inventory *inventory,
    const scene_machine_pistol_mode_state *mode,int32_t base_weapon,int32_t special_weapon)
{
    if(!inventory || !mode || base_weapon<0 || base_weapon>=64 || special_weapon<0 || special_weapon>=64)return 1;
    return inventory->owned[base_weapon] || inventory->owned[special_weapon] ||
        inventory->loaded[special_weapon]!=0 || mode->special || mode->pending;
}
