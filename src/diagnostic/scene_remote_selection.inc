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

/* Parent calls after ALL special projectile/world ticks and before remaining
 * handheld input. Never switch inside the remote action using captured mode/
 * trigger arguments. A due state is consumed this frame, before save capture. */
static int scene_remote_selection_after_world(scene_stream *s,uint32_t frame,
    uint32_t on_foot,uint32_t *changed)
{
    uint32_t target,slot;int32_t ammo;
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
        /*4a4ade..4a4b34 resolves a requested Charge back to Detonator when
         * reserve is empty or a real owned charge is still ready. */
        if(campaign_player_inventory.reserve[ammo]<=0 ||
           scene_remote_selection_has_charge())return RF_OK;
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
