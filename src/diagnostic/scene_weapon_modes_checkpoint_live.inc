#ifndef RF_SCENE_WEAPON_MODES_CHECKPOINT_LIVE_INC
#define RF_SCENE_WEAPON_MODES_CHECKPOINT_LIVE_INC
#include "rf/weapon_modes_checkpoint.h"
/* Scene admission is separate from RFWM framing: all fallible ownership and
 * resource checks precede world publication. No pointers enter the save. */
static int scene_weapon_modes_checkpoint_actions(const scene_weapon_custom_actions *actions,
    const rf_player_weapon *view,uint32_t count)
{
    uint32_t i;
    if(!actions || !view || actions->view!=view || actions->count!=count || actions->active!=-1 ||
       !view->bone_count || view->payloads[3] || (view->initialized && (view->current==2 || view->current==3)))return RF_RANGE;
    for(i=0;i<count;i++)if(!actions->payloads[i])return RF_RANGE;
    return RF_OK;
}
static int scene_weapon_modes_checkpoint_prepare(scene_stream *s,const rf_campaign_player_state *player,
    const rf_weapon_modes_checkpoint *value)
{
    int32_t base=campaign_extra_ids[0],special=campaign_machine_special_id,undercover=campaign_extra_ids[3];
    uint32_t i;int status;
    if(!s || !player || !value || (value->flags&~3u) || base<0 || base>=64 || special<0 || special>=64 ||
       undercover<0 || undercover>=64 || base==special || base==undercover || special==undercover)return RF_RANGE;
    if(player->inventory.owned[special] || player->weapon==(uint32_t)special ||
       (((value->flags&RF_WEAPON_MODES_MACHINE_PISTOL_SPECIAL) || player->inventory.loaded[special]) && !player->inventory.owned[base]) ||
       ((value->flags&RF_WEAPON_MODES_UNDERCOVER_SUPPRESSOR_ATTACHED) && !player->inventory.owned[undercover]))return RF_FORMAT;
    if(campaign_machine_mode.pending || (s->undercover && s->undercover->mode.pending))return RF_RANGE;
    if(player->inventory.owned[base]){
        for(i=0;i<2;i++){
            if(!s->machine_transition_ticks[i] || s->machine_transition_ticks[i]>3600)return RF_RANGE;
            status=scene_weapon_modes_checkpoint_actions(s->machine_custom[i],s->player_weapon[i?17:13],1);if(status)return status;
        }
    }
    if(player->inventory.owned[undercover]){
        scene_undercover_resources *r=s->undercover;rf_player_weapon *view=s->player_weapon[16];
        if(!r || !view || !r->render.part_count || !r->materials.count || !s->undercover_textures ||
           r->mode.attachment.parent<0 || (uint32_t)r->mode.attachment.parent>=view->bone_count)return RF_RANGE;
        status=scene_weapon_modes_checkpoint_actions(r->actions,view,2);if(status)return status;
    }
    return RF_OK;
}
static int scene_weapon_modes_checkpoint_capture(scene_stream *s,rf_weapon_modes_checkpoint *out)
{
    rf_weapon_modes_checkpoint value={0};rf_campaign_player_state player={0};int status;
    if(!s || !out || campaign_machine_mode.special>1 || (s->undercover && s->undercover->mode.attached>1))return RF_RANGE;
    value.flags=campaign_machine_mode.special?RF_WEAPON_MODES_MACHINE_PISTOL_SPECIAL:0;
    if(s->undercover && s->undercover->mode.attached)value.flags|=RF_WEAPON_MODES_UNDERCOVER_SUPPRESSOR_ATTACHED;
    value.conventional_rng=campaign_conventional_random.value;player.inventory=campaign_player_inventory;
    player.weapon=campaign_explicit_unarmed?UINT32_MAX:(uint32_t)campaign_slot_weapon(campaign_equipped_slot);
    status=scene_weapon_modes_checkpoint_prepare(s,&player,&value);if(status)return status;
    *out=value;return RF_OK;
}
/* Publish after ordinary import sidecar resets and before selecting the saved
 * weapon. The current input edge is consumed; no saved animation is replayed. */
static void scene_weapon_modes_checkpoint_assign(scene_stream *s,const rf_weapon_modes_checkpoint *value)
{
    uint32_t special=!!(value->flags&RF_WEAPON_MODES_MACHINE_PISTOL_SPECIAL);
    campaign_machine_mode.special=campaign_machine_mode.target=special;
    campaign_machine_mode.pending=campaign_machine_mode.due=0;campaign_machine_mode.held=!!player_input.alt_fire;
    if(s->undercover){
        s->undercover->mode.attached=s->undercover->mode.target=!!(value->flags&RF_WEAPON_MODES_UNDERCOVER_SUPPRESSOR_ATTACHED);
        s->undercover->mode.pending=0;s->undercover_alt_held=!!player_input.alt_fire;
    }
    campaign_conventional_random.value=value->conventional_rng;
}
#endif
