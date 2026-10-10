#ifndef RF_SCENE_AI_SPIT_INCLUDED
#define RF_SCENE_AI_SPIT_INCLUDED
/* Include after scene_ai_drone_secondary.inc. Original426197..426273 owns
 * action2, per-weapon cadence/+4c0 delay and4257c0 debit. Due409340 clears
 * before factory4c77a0. Both actual Snake classes take the drools-slime
 * primary_1/current-target path41b350, with their independently loaded tags. */
#define SCENE_AI_SPIT_CAPACITY 16u
typedef struct scene_ai_spit_windup {
    uint64_t ticket;uint32_t phase,source,source_slot,start_frame;
    int32_t weapon,due_ms;
} scene_ai_spit_windup;
/* Each pending slot reserves its identically indexed flight slot. Pool
 * pressure is refused before acceptance/debit, never by losing a paid shot.
 * phase1 reserves across presentation; phase2 waits for the real deadline. */
static scene_ai_spit_windup scene_ai_spit_windups[SCENE_AI_SPIT_CAPACITY];
static scene_ai_laser_projectile scene_ai_spits[SCENE_AI_SPIT_CAPACITY];
static uint64_t scene_ai_spit_ticket,scene_ai_spit_epoch;
static uint32_t scene_ai_spit_servicing,scene_ai_spit_presenting,scene_ai_spit_updating;
/* accepted windups,armed,due releases,cancelled,pool full,duplicate holds,
 * invalid release,last source,last intended target,last onset,last due,status */
uint32_t rf_scene_spit[12];
/* Flight counters use the exact Laser flight meanings, in a distinct bank. */
uint32_t rf_scene_ai_spit[12];
/* Camera-independent actual player eye, published once for this frame only
 * after actor_listener_pose and before cinematic/staged camera overrides.
 * Parent invalidates before every view choice and only publishes on foot.
 * Full handle plus pool epoch reject identity/timeline reuse. */
static struct {
    uint64_t epoch;uint32_t valid,frame,handle;float eye[3];
} scene_ai_spit_player_eye;
static void scene_ai_spit_player_eye_invalidate(void)
{scene_ai_spit_player_eye.valid=0;}
static void scene_ai_spit_player_eye_publish(uint32_t frame,const float eye[3])
{
    uint32_t k;scene_ai_spit_player_eye.valid=0;
    if(!eye || campaign_player_view.linked_handle!=-1 ||
       !scene_ai_actor_penetration_player_live(campaign_player_object.handle))return;
    for(k=0;k<3;k++)if(!isfinite(eye[k]))return;
    memcpy(scene_ai_spit_player_eye.eye,eye,12);
    scene_ai_spit_player_eye.frame=frame;scene_ai_spit_player_eye.handle=campaign_player_object.handle;
    scene_ai_spit_player_eye.epoch=scene_ai_spit_epoch;scene_ai_spit_player_eye.valid=1;
}
static void scene_ai_spit_reset(void)
{
    scene_spit_visual_reset();
    scene_ai_spit_player_eye_invalidate();
    memset(scene_ai_spit_windups,0,sizeof(scene_ai_spit_windups));
    memset(scene_ai_spits,0,sizeof(scene_ai_spits));++scene_ai_spit_epoch;
    rf_scene_ai_spit[3]=0;
    /* Do not reopen an in-flight callback or reuse tickets on a new timeline. */
}
static void scene_ai_spit_cancel_source(uint32_t handle)
{
    uint32_t i;
    if(!handle || handle==UINT32_MAX)return;
    for(i=0;i<SCENE_AI_SPIT_CAPACITY;i++){
        scene_ai_spit_windup *p=scene_ai_spit_windups+i;
        if(!p->phase || p->source!=handle)continue;
        ++rf_scene_spit[3];memset(p,0,sizeof(*p));
    }
    /* No refund, and already released projectiles retain their own source. */
}
static uint32_t scene_ai_spit_source_pending(uint32_t handle)
{
    uint32_t i;
    if(!handle || handle==UINT32_MAX)return 0;
    for(i=0;i<SCENE_AI_SPIT_CAPACITY;i++)
        if(scene_ai_spit_windups[i].phase && scene_ai_spit_windups[i].source==handle)return 1;
    return 0;
}
static uint32_t scene_ai_spit_pending(void)
{
    uint32_t i;
    if(scene_ai_spit_servicing || scene_ai_spit_presenting || scene_ai_spit_updating)return 1;
    for(i=0;i<SCENE_AI_SPIT_CAPACITY;i++)
        if(scene_ai_spit_windups[i].phase || scene_ai_spits[i].flight.active)return 1;
    return 0;
}
static campaign_npc_body *scene_ai_spit_source(const scene_ai_spit_windup *pending)
{
    campaign_npc_body *owner=scene_ai_drone_smash_npc_identity(pending->source_slot,pending->source);
    if(!owner || (owner->object_flags&2u) || (owner->view.flags_810&1u) ||
       !isfinite(owner->damage.effects.health) || owner->damage.effects.health<=0 ||
       owner->view.weapons[0]!=pending->weapon || !campaign_enemy_spit_weapon(pending->weapon) ||
       !campaign_enemy_spit_owner(owner) ||
       pending->weapon<0 || pending->weapon>=64 || !owner->inventory.owned[pending->weapon])return NULL;
    return owner;
}
/* Caller already admitted ordinary on-foot actor range/aim/LOS. Reserve and
 * consume once here, then bypass all generic cadence/debit/presentation/hitscan.
 * Target is diagnostic only: due reads the current qualified combat target. */
static int scene_ai_spit_begin(uint32_t source_slot,uint32_t target,uint32_t frame,
    const float player_eye[3],uint32_t *started)
{
    scene_ai_spit_windup next={0},*pending=NULL;campaign_npc_body *owner;
    const rf_weapon_acquire_definition *ammo;const rf_weapon_primary_definition *primary;
    const scene_ai_spit_profile *profile;
    uint32_t i,remaining,due,burst_before;int32_t tag;int status;
    int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    if(!started || !player_eye || !campaign_npc_bodies || source_slot>=campaign_npc_body_count)return RF_RANGE;
    *started=0;
    if(rf_scene_swim_test_enabled || scene_ai_spit_servicing || scene_ai_spit_presenting)return RF_OK;
    owner=campaign_npc_bodies+source_slot;
    next.source_slot=source_slot;next.source=owner->registration.handle;
    next.weapon=owner->view.weapons[0];next.start_frame=frame;
    owner=scene_ai_spit_source(&next);if(!owner || (owner->object_flags&0x4000u))return RF_OK;
    profile=campaign_enemy_spit_profile(next.weapon);if(!profile)return RF_NOT_FOUND;
    if(owner->view.linked_handle!=-1 || scene_npc_seat_attached(next.source) ||
       campaign_npc_holster_blocks_fire(owner))return RF_OK;
    /* Required original creature muzzle must exist before charging a shot.
     * This is primary_1, not the handheld primary_weapon_1 cache. */
    status=rf_scene_corpse_file_tag(source_slot+1,"primary_1",0,&tag);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    ammo=campaign_weapon_supply.definitions+next.weapon;
    if(ammo->magazine || ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->capacity!=200 ||
       owner->inventory.loaded[next.weapon] || owner->inventory.reserve[ammo->ammo_type]<=0)return RF_OK;
    for(i=0;i<SCENE_AI_SPIT_CAPACITY;i++){
        scene_ai_spit_windup *p=scene_ai_spit_windups+i;
        if(p->phase && p->source==next.source){++rf_scene_spit[5];return RF_OK;}
        if(!p->phase && !scene_ai_spits[i].flight.active && !pending)pending=p;
    }
    if(!pending){++rf_scene_spit[4];++rf_scene_ai_spit[4];return RF_OK;}
    if(scene_ai_spit_ticket==UINT64_MAX)return RF_RANGE;
    status=rf_timer_set(&next.due_ms,now,profile->delay_ms);
    if(status)return status;
    primary=&profile->flight.primary;
    remaining=owner->combat_burst_remaining;due=owner->combat_due;
    status=campaign_enemy_cadence(primary,frame,&remaining,&due);
    if(status)return status;
    /* Callback-free acceptance: no failed operation follows the real debit
     * before cadence and reservation publication. Release never checks reserve. */
    status=rf_weapon_consume_shot(&owner->inventory,campaign_weapon_supply.definitions,
        campaign_weapon_supply.names.count,next.weapon);
    if(status)return status;
    next.ticket=++scene_ai_spit_ticket;next.phase=1;*pending=next;
    burst_before=owner->combat_burst_remaining;
    owner->combat_burst_remaining=remaining;owner->combat_due=due;
    *started=1;++rf_scene_spit[0];++rf_scene_enemy_combat[2];
    rf_scene_spit[7]=next.source;rf_scene_spit[8]=target;rf_scene_spit[9]=frame;
    if(rf_scene_combat_trace)printf("ENEMY_SHOT_TRACE %u %u\n",frame,campaign_seeds.records.items[source_slot].record.uid);
    scene_ai_spit_presenting=1;
    status=campaign_enemy_fire_presentation(source_slot,1,primary,burst_before,0);
    scene_ai_spit_presenting=0;rf_scene_enemy_fire[5]=(uint32_t)status;
    if(status==RF_NOT_FOUND)++rf_scene_enemy_fire[2];
    else if(status){if(pending->ticket==next.ticket)memset(pending,0,sizeof(*pending));return status;}
    if(pending->ticket!=next.ticket || pending->phase!=1)return RF_OK;
    if(!scene_ai_spit_source(pending)){scene_ai_spit_cancel_source(next.source);return RF_OK;}
    pending->phase=2;++rf_scene_spit[1];return RF_OK;
}
/* Current target interpretation is the existing port combat owner's policy:
 * explicit/opposed actor orders use combat_target; ordinary acquired attacks
 * use the player. No onset point is frozen and no new target is acquired here.
 * A stale/dead/hidden/nonactor target leaves the original eye-basis fallback;
 * it does not cancel a paid shot or retarget a reused full-handle slot. */
static void scene_ai_spit_target(const campaign_npc_body *owner,uint32_t frame,
    rf_weapon_aim_source *aim)
{
    uint32_t target=UINT32_MAX,i;
    if(owner->combat_scripted==1 || owner->combat_scripted==2)target=owner->combat_target;
    else if(!owner->combat_scripted && owner->combat_alert)target=campaign_player_object.handle;
    if(target==owner->registration.handle)return;
    if(scene_ai_actor_penetration_player_live(target) && campaign_player_view.linked_handle==-1 &&
       scene_ai_spit_player_eye.valid && scene_ai_spit_player_eye.frame==frame &&
       scene_ai_spit_player_eye.handle==target && scene_ai_spit_player_eye.epoch==scene_ai_spit_epoch){
        aim->target_present=aim->target_actor=1;
        memcpy(aim->target_position,scene_actor_body.state.position,12);
        memcpy(aim->target_eye,scene_ai_spit_player_eye.eye,12);return;
    }
    for(i=0;i<campaign_npc_body_count;i++)if(scene_ai_actor_penetration_npc_live(i,target)){
        aim->target_present=aim->target_actor=1;
        memcpy(aim->target_position,campaign_npc_bodies[i].body.state.position,12);
        memcpy(aim->target_eye,campaign_npc_bodies[i].eye_position,12);return;
    }
}
/* Exact retained primary_1 attachment on the current head pose. Original
 *41b3e4 places it,41b3f2..41b42c forms a target-body basis for drools slime,
 *and41b4c0 applies the existing pain/target-eye dot>0.8 refinement. The source
 *has no weapon-world model. Never call the handheld eye+0.3 fallback here. */
static int scene_ai_spit_prepare(uint32_t slot,campaign_npc_body *owner,
    uint32_t frame,const scene_ai_spit_profile *profile,scene_ai_laser_projectile *out)
{
    rf_entity_pose *pose=NULL;rf_entity_playback_model *model;
    rf_corpse_surface_source source={0};rf_weapon_aim_source aim={0};
    scene_ai_laser_projectile next={0};float transform[12],ray[3];
    int32_t tag;uint32_t k;int status,active;
    if(slot>=campaign_model_owner_count || !profile || !out)return RF_RANGE;
    status=campaign_model_pose(slot,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    status=campaign_model_pose_ready(slot,pose);if(status)return status;
    status=rf_scene_corpse_file_tag(slot+1,"primary_1",0,&tag);if(status)return status;
    source.model=slot+1;memcpy(source.position,campaign_model_owners[slot].position,12);
    memcpy(source.basis,campaign_model_owners[slot].basis,36);
    status=campaign_file_tag_pose(&source,tag,transform);if(status)return status;
    memcpy(aim.eye_basis,owner->look.orientation,36);
    memcpy(next.basis,aim.eye_basis,36);
    aim.local_related=(owner->object_flags&8u)!=0;
    scene_ai_spit_target(owner,frame,&aim);
    if(aim.target_present){
        for(k=0;k<3;k++)ray[k]=aim.target_position[k]-transform[9+k];
        if(ray[0]!=0 || ray[1]!=0 || ray[2]!=0){
            status=rf_entity_navigation_basis(ray,(float(*)[3])next.basis);if(status)return status;
        }
    }
    if(!aim.local_related && owner->pain.selected_action!=-1){
        if(!campaign_playback_resources.models || pose->skeleton>=campaign_playback_resources.model_count)return RF_RANGE;
        model=campaign_playback_resources.models+pose->skeleton;
        status=rf_motion_action_active(&pose->playback,model->resources,model->count,
            owner->selection.mapping.actions,owner->pain.selected_action,&active);if(status)return status;
        aim.animation_locked=(uint32_t)active;
        if(!active)owner->pain.selected_action=-1;
    }
    status=rf_weapon_target_aim(&aim,transform+9,next.basis);if(status)return status;
    status=rf_weapon_flight_launch(&next.flight,transform+9,next.basis+6,
        profile->flight.speed,profile->flight.lifetime,profile->flight.radius);if(status)return status;
    next.source=owner->registration.handle;next.weapon=profile->flight.weapon;
    next.damage=combat_enemy_primary_damage(&profile->flight.primary);
    next.damage_kind=profile->flight.primary.damage_kind;next.liquid.query_flags=0x1004u;
    next.blast_radius=profile->blast_radius;next.blast_kind=profile->blast_kind;
    /* Exact installed profile has no AI spread override. Zero spread is the
     * retained port default, not inferred from an absent undeviating flag. */
    *out=next;return RF_OK;
}
/* Service before ordinary ammo/fallback, target/mode/pain and cadence gates.
 * Last-round reserve0 cannot suppress a prepaid release. Hidden0x4000 defers
 *upstream41daf0->40a110 without restarting the deadline. Due never rechecks
 *range/LOS, debits/refunds, retries, or adds homing after launch. */
static int scene_ai_spit_service(uint32_t frame,const float player_eye[3])
{
    uint32_t i;int status=RF_OK;
    int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    if(!player_eye)return RF_RANGE;
    if(scene_ai_spit_servicing || scene_ai_spit_presenting)return RF_OK;
    scene_ai_spit_servicing=1;
    for(i=0;i<SCENE_AI_SPIT_CAPACITY;i++){
        scene_ai_spit_windup *p=scene_ai_spit_windups+i,accepted;
        scene_ai_laser_projectile shot;campaign_npc_body *owner;const scene_ai_spit_profile *profile;int expired;
        if(p->phase!=2)continue;
        owner=scene_ai_spit_source(p);
        if(!owner){scene_ai_spit_cancel_source(p->source);continue;}
        profile=campaign_enemy_spit_profile(p->weapon);if(!profile){status=RF_NOT_FOUND;break;}
        if(owner->object_flags&0x4000u)continue;
        status=rf_timer_expired(p->due_ms,now,&expired);if(status)break;if(!expired)continue;
        accepted=*p;memset(p,0,sizeof(*p)); /*4093aa consumes before factory. */
        ++rf_scene_spit[2];rf_scene_spit[10]=frame;
        status=scene_ai_spit_prepare(accepted.source_slot,owner,frame,profile,&shot);
        if(status){++rf_scene_spit[6];break;}
        shot.source=accepted.source;shot.weapon=accepted.weapon;scene_ai_spits[i]=shot;
        scene_spit_visual_accept(i,&shot,frame);
        ++rf_scene_ai_spit[0];++rf_scene_ai_spit[3];
        rf_scene_ai_spit[8]=accepted.source;rf_scene_ai_spit[10]=frame;
    }
    rf_scene_spit[11]=(uint32_t)status;scene_ai_spit_servicing=0;return status;
}
#endif
