#ifndef RF_SCENE_REMOTE_SELECTION_INCLUDED
#define RF_SCENE_REMOTE_SELECTION_INCLUDED
#include "rf/remote_checkpoint.h"

/* Included inside scene_remote_gameplay.inc after its pool/scheduler owners.
 * Original4a56d3..4a571c schedules the Detonator after charge fire/alt duration
 * minus0.1s;4a53dc..4a5415 schedules Charge after Detonator fire duration.
 * These are player+b8 follow-ups, independent of the general Autoswitch option.
 * The port retains original millisecond rounding, then rounds upward to its
 * fixed60Hz simulation. The existing throw/cooldown and projectile pool remain
 * separate owners. RFRM2 persists only this bounded target/countdown pair. */
static uint32_t scene_remote_followup_target,scene_remote_followup_ticks;

static void scene_remote_selection_cancel(void)
{scene_remote_followup_target=scene_remote_followup_ticks=0;}

static uint32_t scene_remote_selection_admitted(scene_stream *s)
{
    return s && scene_remote_resources && scene_weapon_available(8) &&
        scene_weapon_available(9) && s->player_weapon[8] && s->player_weapon[9] &&
        campaign_remote_id>=0 && campaign_remote_id<64 &&
        (uint32_t)campaign_remote_id<campaign_weapon_supply.names.count &&
        campaign_player_inventory.owned[campaign_remote_id] &&
        !campaign_explicit_unarmed && campaign_player_damage.state.effects.health>0 &&
        !scene_driller_active(s) && !scene_turret_player_active() &&
        !campaign_player_form.active;
}

/* Read already-resident action timing; never load another motion or advance
 * playback. target2 follows a successful charge throw/place; target1 follows
 * an accepted detonation request. ticks0 means this optional adapter is not
 * admitted and leaves the existing remote action usable without a follow-up. */
static int scene_remote_selection_prepare(scene_stream *s,uint32_t target,
    uint32_t alternate,uint32_t *ticks)
{
    rf_player_weapon *w;const rf_motion_weight_envelope *clip;
    uint32_t slot,action,milliseconds,result;double seconds;
    if(!s || !ticks || target<1 || target>2 || alternate>1 ||
       (target==1 && alternate))return RF_RANGE;
    *ticks=0;slot=target==2?8u:9u;action=alternate?3u:1u;
    if(!scene_remote_selection_admitted(s) || campaign_equipped_slot!=slot)return RF_OK;
    w=s->player_weapon[slot];
    if(action>=w->clip_count || !w->payloads[action])return RF_NOT_FOUND;
    clip=&w->resources[action].comparison;
    if(clip->end_tick<=clip->start_tick)return RF_FORMAT;
    seconds=rf_motion_duration(clip->start_tick,clip->end_tick);
    if(target==2)seconds-=(double)0.1f;
    if(!isfinite(seconds) || seconds<=0 || seconds>60.0)return RF_FORMAT;
    /*4a56f4/4a53e8: round to milliseconds before applying port frame policy. */
    milliseconds=(uint32_t)(seconds*1000.0+0.5);
    result=(milliseconds*60u+999u)/1000u;if(!result)result=1;
    if(result>RF_REMOTE_FOLLOWUP_MAX_TICKS ||
       (target==2 && result<=(alternate?15u:21u)))return RF_FORMAT;
    *ticks=result;return RF_OK;
}

static void scene_remote_selection_arm(uint32_t target,uint32_t ticks)
{
    scene_remote_selection_cancel();
    if(ticks){scene_remote_followup_target=target;scene_remote_followup_ticks=ticks;}
}

/* Called exactly once inside remote input, before accepting a new action.
 * A newly armed countdown is first decremented on the following frame. Manual
 * selection also cancels through campaign_select_primary (original4acd50).
 * mode0 includes mounted/dead/unarmed owners and cannot keep a follow-up alive. */
static void scene_remote_selection_input_tick(scene_stream *s,uint32_t mode)
{
    uint32_t expected;
    if(!scene_remote_followup_target)return;
    expected=scene_remote_followup_target==2?1u:2u;
    if(mode!=expected || !scene_remote_selection_admitted(s)){
        scene_remote_selection_cancel();return;
    }
    if(scene_remote_followup_ticks)--scene_remote_followup_ticks;
}

/* Exact selection-remap subset4a4aeb..4a4b32: matching owner/type, positive
 * fuse and no retired flag2. It does not require attachment, nor consult the
 * diagnostic live count or the broader availability helper's life predicate. */
static uint32_t scene_remote_selection_has_charge(void)
{
    uint32_t i;
    for(i=0;i<RF_REMOTE_CHARGE_CAPACITY;i++){
        const rf_remote_charge *charge=scene_remote_charges+i;
        if(charge->flight.lifecycle.active &&
           charge->owner==campaign_player_object.handle &&
           charge->type==(uint32_t)campaign_remote_id &&
           !(charge->object_flags&2u) && charge->flight.lifecycle.fuse>0)return 1;
    }
    return 0;
}

/* Only the existing successful Detonator action's due target-1 reaches here.
 * Original 4a5420 calls 4a6f10 immediately after scheduling presentation; this
 * port waits for its full action deadline after world processing. Unlike the
 * original ready-charge 4c9e30 gate, any active exact owned charge blocks this
 * replacement, including flying or requested-but-not-retired records. */
static int scene_remote_empty_ranked_slot(scene_stream *s,int32_t *slot)
{
    rf_weapon_supply supply[64];uint32_t flags_264[64];int32_t preference[32];
    rf_weapon_empty_input input={0};rf_weapon_empty_action action;
    const rf_weapon_acquire_definition *current;
    uint32_t i,source=campaign_player_object.handle;int32_t ammo;int status;
    if(!s || !slot)return RF_RANGE;*slot=-1;
    if(!scene_remote_selection_admitted(s) || campaign_equipped_slot!=9 ||
       campaign_selected_weapon()!=campaign_remote_id || scene_remote_pending ||
       !source || source==UINT32_MAX || campaign_player_object.view!=&campaign_player_view ||
       (uint32_t)campaign_player_view.handle!=source || campaign_player_view.type!=0 ||
       campaign_player_damage.state.effects.handle!=source ||
       rf_object_registry_lookup(&campaign_registry,source)!=&campaign_player_object ||
       rf_entity_lookup(&campaign_entities,(int32_t)source)!=&campaign_player_view ||
       !scene_actor_body.allocated_bytes ||
       ((campaign_player_view.flags_7c|campaign_player_damage.object_flags)&(2u|0x4000u)) ||
       campaign_cutscene_runtime.active ||
       !isfinite(campaign_player_damage.state.effects.health) || rf_scene_enemy_combat[6] ||
       (campaign_player_view.flags_810&(1u|0x800u)) || rf_scene_combat[6])return RF_OK;
    if(campaign_weapon_supply.names.count>64 || campaign_weapon_reset.names.count>64 ||
       campaign_detonator_id<0 || campaign_detonator_id>=64 ||
       campaign_detonator_id==campaign_remote_id ||
       (uint32_t)campaign_detonator_id>=campaign_weapon_supply.names.count ||
       rf_weapon_name_find(&campaign_weapon_supply.names,"Remote Charge Detonator")!=campaign_detonator_id)
        return RF_FORMAT;
    /* Slot 9 deliberately aliases Charge for ownership. The empty decision
     * instead needs the actual Detonator descriptor, sharing its ammo bank;
     * it has positive capacity but no conventional magazine requirement. */
    current=campaign_weapon_supply.definitions+campaign_detonator_id;ammo=current->ammo_type;
    if(current->capacity<=0 || ammo<0 || ammo>=32 ||
       ammo!=campaign_weapon_supply.definitions[campaign_remote_id].ammo_type)return RF_FORMAT;
    if(campaign_player_inventory.loaded[campaign_detonator_id]!=0 ||
       campaign_player_inventory.reserve[ammo]!=0)return RF_OK;
    if(!scene_grenade_empty_selection.ready)return RF_NOT_FOUND;
    memset(supply,0,sizeof(supply));memset(flags_264,0,sizeof(flags_264));
    for(i=0;i<campaign_weapon_supply.names.count;i++){
        int32_t descriptor=rf_weapon_name_find(&campaign_weapon_reset.names,
            campaign_weapon_supply.names.names[i]);
        if(descriptor<0)return RF_FORMAT;
        supply[i].ammo_type=campaign_weapon_supply.definitions[i].ammo_type;
        supply[i].capacity=campaign_weapon_supply.definitions[i].capacity;
        supply[i].flags_268=campaign_weapon_reset.definitions[descriptor].flags_268;
        flags_264[i]=campaign_weapon_reset.definitions[descriptor].flags_264;
    }
    for(i=0;i<32;i++){
        int32_t weapon=scene_grenade_empty_selection.preference[i];
        preference[i]=scene_weapon_empty_rank_slot(s,weapon)>=0?weapon:-1;
    }
    input.current=input.block_weapon=campaign_detonator_id;
    input.always_weapon=campaign_grenade_id;input.excluded_weapon=campaign_remote_id;
    input.paired_first=input.paired_second=-1;
    input.automatic_enabled=1;input.defer_flag=0; /* Existing verified factory policy. */
    for(i=0;i<RF_REMOTE_CHARGE_CAPACITY;i++){
        const rf_remote_charge *charge=scene_remote_charges+i;
        if(charge->flight.lifecycle.active && charge->owner==source &&
           charge->type==(uint32_t)campaign_remote_id){input.special_block=1;break;}
    }
    status=rf_weapon_decide_empty(&campaign_player_inventory,NULL,supply,flags_264,
        campaign_weapon_supply.names.count,preference,&input,&action);if(status)return status;
    if(action.kind!=RF_WEAPON_EMPTY_SELECT)return RF_OK;
    *slot=scene_weapon_empty_rank_slot(s,action.weapon);
    return *slot<0?RF_FORMAT:RF_OK;
}

/* Parent calls after ALL special projectile/world ticks and before remaining
 * handheld input. Never switch inside the remote action using captured mode/
 * trigger arguments. A due state is consumed this frame, before save capture. */
static int scene_remote_selection_after_world(scene_stream *s,uint32_t frame,
    uint32_t on_foot,uint32_t *changed)
{
    uint32_t target,slot;int32_t ammo,replacement;int status;
    if(!s || !changed || on_foot>1)return RF_RANGE;*changed=0;
    if(!scene_remote_followup_target)return RF_OK;
    target=scene_remote_followup_target;
    if(!on_foot || target>2 || !scene_remote_selection_admitted(s) ||
       campaign_equipped_slot!=(target==2?8u:9u)){
        scene_remote_selection_cancel();return RF_OK;
    }
    if(scene_remote_followup_ticks)return RF_OK;
    /* Installed timing always expires after publication. Do not select after
     * a malformed/restored pending throw or leave an unsaveable due owner. */
    scene_remote_selection_cancel();
    if(scene_remote_pending)return RF_FORMAT;
    slot=target==2?9u:8u;
    if(target==1){
        ammo=campaign_weapon_supply.definitions[campaign_remote_id].ammo_type;
        if(ammo<0 || ammo>=32)return RF_FORMAT;
        if(campaign_player_inventory.reserve[ammo]<0)return RF_OK;
        if(!campaign_player_inventory.reserve[ammo]){
            /* The timer is already consumed, including every declined empty
             * decision. Never retain target 1/ticks 0 or retry from idle input. */
            status=scene_remote_empty_ranked_slot(s,&replacement);if(status)return status;
            if(replacement<0 || replacement==9)return RF_OK;
            slot=(uint32_t)replacement;
        }else if(scene_remote_selection_has_charge())return RF_OK;
        /* Positive reserve retains 4a4ade..4a4b34's existing ready-charge
         * remap. Empty reserve uses the separately qualified consumer above. */
    }
    campaign_select_primary(slot);scene_player_auto_select_inputs();
    s->player_pose_frame=0;s->player_slot=UINT32_MAX;
    s->player_shots=rf_scene_combat[0];s->player_reload=0;
    campaign_ammo_publish();rf_scene_weapon_selection[0]=slot;
    ++rf_scene_weapon_selection[1];*changed=1;
    if(rf_scene_combat_trace)printf("REMOTE_AUTO_SELECT %u %u %u\n",frame,target,slot);
    return RF_OK;
}
#endif
