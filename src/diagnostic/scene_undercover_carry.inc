#ifndef RF_SCENE_UNDERCOVER_CARRY_INC
#define RF_SCENE_UNDERCOVER_CARRY_INC
/* Include after scene_undercover_mode.inc. In-memory sidecar only: capture
 * after validated campaign player export; copy pending sidecar before scene
 * teardown. Reset both copies on a new campaign or disk checkpoint import.
 * Restore after inventory acceptance and fresh suppressor resources opening.
 * No inventory grants, persistence format, animation/input carry or tag copy. */
#include "rf/campaign.h"
typedef struct scene_undercover_carry {
    rf_campaign_player_state player;int32_t weapon;uint32_t valid,attached;
} scene_undercover_carry;
static void scene_undercover_carry_reset(scene_undercover_carry *carry)
{if(carry)memset(carry,0,sizeof(*carry));}
static int scene_undercover_carry_capture(scene_undercover_carry *carry,
    const rf_campaign_player_state *player,int32_t weapon,const scene_undercover_mode *mode)
{
    scene_undercover_carry value={0};int status;
    if(!carry || !player || !mode || weapon<0 || weapon>=64 || mode->attached>1)return RF_RANGE;
    status=rf_campaign_player_copy(&value.player,player,player->catalog_hash);if(status)return status;
    value.weapon=weapon;value.valid=1;
    /* Incomplete detach keeps attached; incomplete attach stays detached. */
    value.attached=player->inventory.owned[weapon]?mode->attached:0;
    *carry=value;return RF_OK;
}
static uint32_t scene_undercover_carry_matches(const scene_undercover_carry *carry,
    const rf_campaign_player_state *player,int32_t weapon)
{
    if(!carry || !player || !carry->valid || carry->weapon!=weapon || carry->attached>1)return 0;
    return carry->player.catalog_hash==player->catalog_hash && carry->player.weapon==player->weapon &&
        carry->player.health==player->health && carry->player.armor==player->armor &&
        !memcmp(&carry->player.inventory,&player->inventory,sizeof(player->inventory));
}
static int scene_undercover_carry_restore(const scene_undercover_carry *carry,
    const rf_campaign_player_state *player,int32_t weapon,scene_undercover_resources *resources,
    rf_player_weapon *view,uint32_t alternate_held,uint32_t *restored_held)
{
    if(!scene_undercover_carry_matches(carry,player,weapon))return RF_NOT_FOUND;
    if(!resources || !view || !restored_held || !resources->actions || resources->actions->view!=view ||
       !resources->render.part_count || !resources->materials.count ||
       resources->actions->active>=0 || view->payloads[3] || resources->mode.pending ||
       resources->mode.attachment.parent<0 || (uint32_t)resources->mode.attachment.parent>=view->bone_count)return RF_RANGE;
    resources->mode.attached=carry->attached;resources->mode.target=carry->attached;resources->mode.pending=0;
    *restored_held=!!alternate_held;return RF_OK;
}
#endif
