#ifndef RF_SCENE_AI_OPPOSED_ACQUISITION_INCLUDED
#define RF_SCENE_AI_OPPOSED_ACQUISITION_INCLUDED
#include <float.h>
/* Include after extra weapon selection and before campaign_enemy_tick.
 * First-pass sight policy matches ordinary player awareness: 20 units,
 * 120-degree cone, one staggered sample/30 frames, 30-frame reaction.
 * Opposed 0/2 affiliations reuse the turret scene policy; neutral/outcast
 * actors are not automatic targets. Mode2 is existing reactive ownership,
 * not an authored Attack order. No allocation or new persistent target bank.
 */
/* samples,candidates,acquired,player priority,cover,mode rejects,source UID,
 * target UID,source handle,target handle,last frame,errors,
 * friendly-to-hostile,hostile-to-friendly,neutral candidates skipped */
uint32_t rf_scene_enemy_opposed[15];
/* Last acquired pair: two rows of UID,handle,target,mode,health bits,armor
 * bits,position xyz,eye xyz. Sampling changes no gameplay state. */
uint32_t rf_scene_npc_opposed_probe[24];
/* Immutable assignment snapshot: count, two rows of UID,targetUID,mode,due,
 * reload_due,burst,RNG,health bits,armor bits,current loaded rounds. */
uint32_t rf_scene_enemy_reactive_restore[21];
static uint32_t scene_npc_opposed_probe_slots[2]={UINT32_MAX,UINT32_MAX};
static void campaign_enemy_opposed_reset(void)
{
    memset(rf_scene_enemy_opposed,0,sizeof(rf_scene_enemy_opposed));
    memset(rf_scene_npc_opposed_probe,0,sizeof(rf_scene_npc_opposed_probe));
    memset(rf_scene_enemy_reactive_restore,0,sizeof(rf_scene_enemy_reactive_restore));
    scene_npc_opposed_probe_slots[0]=scene_npc_opposed_probe_slots[1]=UINT32_MAX;
}
static void scene_npc_opposed_probe(void)
{
    for(uint32_t i=0;i<2;i++) {
        uint32_t slot=scene_npc_opposed_probe_slots[i],*p=rf_scene_npc_opposed_probe+i*12;
        if(slot>=campaign_npc_body_count||slot>=campaign_seeds.records.count)continue;
        const campaign_npc_body *o=campaign_npc_bodies+slot;
        /* Stable authored slots remain after death/unregister. Keep the last
         * real handle as identity while exposing its terminal vitals. */
        p[0]=(uint32_t)campaign_seeds.records.items[slot].record.uid;
        if(o->registration.view)p[1]=o->registration.handle;
        p[2]=o->combat_target;p[3]=o->combat_scripted;
        memcpy(p+4,&o->damage.effects.health,4);memcpy(p+5,&o->damage.effects.armor,4);
        memcpy(p+6,o->registration.view?o->body.state.position:o->published,12);
        memcpy(p+9,o->eye_position,12);
    }
}

static uint32_t campaign_enemy_opposed_live(const campaign_npc_body *o)
{
    return o->registration.view==&o->view&&o->body.allocated_bytes&&
        rf_object_registry_lookup(&campaign_registry,o->registration.handle)==&o->registration&&
        rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)==&o->view&&
        !(o->object_flags&(2u|0x4000u))&&!(o->view.flags_810&1u)&&
        isfinite(o->damage.effects.health)&&o->damage.effects.health>0;
}
static void scene_npc_reactive_restore_probe(void)
{
    uint32_t i,j,count=0;
    memset(rf_scene_enemy_reactive_restore,0,sizeof(rf_scene_enemy_reactive_restore));
    scene_npc_opposed_probe_slots[0]=scene_npc_opposed_probe_slots[1]=UINT32_MAX;
    for(i=0;i<campaign_npc_body_count;i++) {
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->combat_scripted!=2||!campaign_enemy_opposed_live(o))continue;
        for(j=0;j<campaign_npc_body_count;j++)if(campaign_npc_bodies[j].registration.view&&
            campaign_npc_bodies[j].registration.handle==o->combat_target)break;
        if(j==campaign_npc_body_count)continue;
        if(count<2) {
            uint32_t *p=rf_scene_enemy_reactive_restore+1+10*count;
            p[0]=(uint32_t)campaign_seeds.records.items[i].record.uid;
            p[1]=(uint32_t)campaign_seeds.records.items[j].record.uid;
            p[2]=o->combat_scripted;p[3]=o->combat_due;p[4]=o->combat_reload_due;
            p[5]=o->combat_burst_remaining;p[6]=campaign_enemy_spread_random.value;
            memcpy(p+7,&o->damage.effects.health,4);memcpy(p+8,&o->damage.effects.armor,4);
            p[9]=o->view.weapons[0]>=0&&o->view.weapons[0]<64?
                (uint32_t)o->inventory.loaded[o->view.weapons[0]]:UINT32_MAX;
        }
        if(!count){scene_npc_opposed_probe_slots[0]=i;scene_npc_opposed_probe_slots[1]=j;}
        ++count;
    }
    rf_scene_enemy_reactive_restore[0]=count;scene_npc_opposed_probe();
}
static uint32_t campaign_enemy_opposed_cone(const campaign_npc_body *o,
    const float point[3],float delta[3],float *distance)
{
    float d=0,forward=0;
    for(uint32_t k=0;k<3;k++){
        delta[k]=point[k]-o->eye_position[k];d+=delta[k]*delta[k];
        forward+=o->look.orientation[6+k]*delta[k];
    }
    if(!isfinite(d)||!isfinite(forward)||d<.0001f||d>400.f||
       forward<=0||forward*forward<d*.25f)return 0;
    *distance=d;return 1;
}
static uint32_t campaign_enemy_opposed_moving(const float velocity[3])
{
    float speed=0;for(uint32_t k=0;k<3;k++)speed+=velocity[k]*velocity[k];
    return isfinite(speed)&&speed>.0001f;
}
static int campaign_enemy_opposed_acquire(scene_stream *stream,uint32_t slot,
    uint32_t frame,const float player_eye[3])
{
    campaign_npc_body *owner,*target=NULL;uint32_t blocked,target_slot=UINT32_MAX;
    float delta[3],distance,best=FLT_MAX;int status;
    campaign_enemy_weapon_selection weapon={0};
    const int32_t ids[8]={campaign_pistol_id,campaign_rifle_id,campaign_riot_id,
        campaign_shotgun_id,campaign_rocket_id,campaign_grenade_id,campaign_sniper_id,campaign_rail_id};
    if(!stream||!player_eye||slot>=campaign_npc_body_count)return RF_RANGE;
    owner=campaign_npc_bodies+slot;
    if(owner->combat_alert||owner->combat_scripted||owner->script_once.count||
       owner->script_shoot.active||!campaign_enemy_opposed_live(owner)||
       (owner->damage.effects.affiliation!=0&&owner->damage.effects.affiliation!=2)||
       owner->view.weapons[0]<0||
       (frame%30+campaign_enemy_sight_phase_offset)%30!=slot%30)return RF_OK;
    /* Candidate-specific motion is checked below. Passing moving1 here only
     * permits Motion11's search; Catatonic and active Waypoints remain vetoes. */
    if(!campaign_enemy_mode_admits(owner,1,0))return RF_OK;
    status=campaign_enemy_weapon_select(owner->view.weapons[0],ids,campaign_primary,
        campaign_weapon_supply.definitions,campaign_weapon_supply.names.count,&weapon);
    if(status==RF_NOT_FOUND)status=campaign_enemy_extra_weapon_select(owner->view.weapons[0],&campaign_weapon_supply,&weapon);
    if(status==RF_NOT_FOUND&&((owner->view.weapons[0]==campaign_grenade_id&&scene_grenade_resources)||
       (owner->view.weapons[0]==campaign_rocket_id&&scene_rocket_resources)))status=RF_OK;
    if(status==RF_NOT_FOUND)return RF_OK;if(status)goto fail;
    ++rf_scene_enemy_opposed[0];
    /* Preserve the existing hostile's opportunity to acquire the player.
     * Player affiliation is not a team ID (the ordinary default is also0).
     * Keep this preflight identical to ordinary sight, including its cover
     * query, rather than adding a new player-awareness policy here. */
    if(owner->damage.effects.affiliation==0&&campaign_player_object.view==&campaign_player_view&&
       rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)==&campaign_player_object&&
       rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)==&campaign_player_view&&
       campaign_player_damage.state.effects.health>0&&
       !(campaign_player_form.active&&!campaign_player_form.compromised)&&
       campaign_enemy_mode_admits(owner,campaign_enemy_opposed_moving(scene_actor_body.state.velocity),0)&&
       campaign_enemy_opposed_cone(owner,player_eye,delta,&distance)) {
        status=combat_obstructed(stream,owner->eye_position,delta,&blocked);if(status)goto fail;
        if(!blocked){++rf_scene_enemy_opposed[3];return RF_OK;}
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *candidate=campaign_npc_bodies+i;uint32_t clutter;float fraction;
        if(i==slot||!campaign_enemy_opposed_live(candidate))continue;
        if(candidate->damage.effects.affiliation==1){++rf_scene_enemy_opposed[14];continue;}
        if(!((owner->damage.effects.affiliation==0&&candidate->damage.effects.affiliation==2)||
             (owner->damage.effects.affiliation==2&&candidate->damage.effects.affiliation==0))||
           scene_npc_seat_related(owner->registration.handle,candidate->registration.handle)||
           scene_turret_generated_related(owner->registration.handle,candidate->registration.handle))continue;
        if(!campaign_enemy_mode_admits(owner,campaign_enemy_opposed_moving(candidate->body.state.velocity),0)){
            ++rf_scene_enemy_opposed[5];continue;
        }
        if(!campaign_enemy_opposed_cone(owner,candidate->eye_position,delta,&distance)||distance>=best)continue;
        ++rf_scene_enemy_opposed[1];
        status=combat_obstructed(stream,owner->eye_position,delta,&blocked);if(status)goto fail;
        if(!blocked){
            status=campaign_clutter_firearm_select(owner->eye_position,delta,1,&clutter,&fraction);if(status)goto fail;
            blocked=clutter!=UINT32_MAX;
        }
        if(blocked){++rf_scene_enemy_opposed[4];continue;}
        target=candidate;target_slot=i;best=distance;
    }
    if(!target)return RF_OK;
    if(frame>UINT32_MAX-30){status=RF_RANGE;goto fail;}
    status=campaign_animation_cancel(owner);if(status)goto fail;
    if(!campaign_script_movement_owns(owner)){campaign_pursuit_stop(owner);owner->script_move.active=0;}
    owner->combat_scripted=2;owner->combat_target=target->registration.handle;owner->combat_alert=1;
    owner->combat_navigation_due=owner->combat_burst_remaining=0;owner->combat_due=frame+30;
    ++rf_scene_enemy_opposed[2];++rf_scene_enemy_combat[1];
    ++rf_scene_enemy_opposed[owner->damage.effects.affiliation==2?12:13];
    rf_scene_enemy_opposed[6]=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    rf_scene_enemy_opposed[7]=(uint32_t)campaign_seeds.records.items[target_slot].record.uid;
    rf_scene_enemy_opposed[8]=owner->registration.handle;rf_scene_enemy_opposed[9]=target->registration.handle;
    rf_scene_enemy_opposed[10]=frame;
    scene_npc_opposed_probe_slots[0]=slot;scene_npc_opposed_probe_slots[1]=target_slot;
    scene_npc_opposed_probe();return RF_OK;
fail:++rf_scene_enemy_opposed[11];return status;
}
#endif
