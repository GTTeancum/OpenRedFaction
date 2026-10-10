#ifndef RF_SCENE_AI_CANE_INCLUDED
#define RF_SCENE_AI_CANE_INCLUDED
/* Include after scene_ai_drone_secondary.inc. Original accepted windup
 * 426197..426273 owns action2,3s cadence,+4c0 delay and4257c0 reserve debit.
 * Due409340 clears +4c0 before factory4c77a0. Capek calls handheld41b040 for position, then passes CURRENT
 * eye basis at409557..40956b, not a retained target or onset ray. */
#define SCENE_AI_CANE_CAPACITY 16u
typedef struct scene_ai_cane_windup {
    uint64_t ticket;uint32_t phase,source,source_slot,start_frame;
    int32_t weapon,due_ms;
} scene_ai_cane_windup;
/* Each pending slot reserves its identically indexed flight slot. Pool
 * pressure is refused before acceptance/debit, never by losing a paid shot.
 * phase1 reserves across presentation; phase2 waits for the real deadline. */
static scene_ai_cane_windup scene_ai_cane_windups[SCENE_AI_CANE_CAPACITY];
static scene_ai_laser_projectile scene_ai_canes[SCENE_AI_CANE_CAPACITY];
static uint64_t scene_ai_cane_flight_tickets[SCENE_AI_CANE_CAPACITY];
typedef struct scene_ai_cane_homing {
    uint32_t target,target_slot;int32_t scan_due;
} scene_ai_cane_homing;
static scene_ai_cane_homing scene_ai_cane_locks[SCENE_AI_CANE_CAPACITY];
static uint64_t scene_ai_cane_ticket,scene_ai_cane_epoch;
static uint32_t scene_ai_cane_servicing,scene_ai_cane_presenting,scene_ai_cane_updating;
/* accepted windups,armed,due releases,cancelled,pool full,duplicate holds,
 * invalid release,last source,last intended target,last onset,last due,status */
uint32_t rf_scene_cane[12];
/* Flight counters use the exact Laser flight meanings, in a distinct bank. */
uint32_t rf_scene_ai_cane[12];
/* This is the physical on-foot eye before cinematic/view-camera overrides.
 * It never persists through a frame/timeline/identity boundary. Mounted
 * acquisition is deferred until an independent actual-eye owner is available. */
static struct {
    uint64_t epoch;uint32_t valid,frame,handle;float eye[3];
} scene_ai_cane_player_eye;
static void scene_ai_cane_player_eye_invalidate(void)
{scene_ai_cane_player_eye.valid=0;}
static void scene_ai_cane_player_eye_publish(uint32_t frame,const float eye[3])
{
    uint32_t k;scene_ai_cane_player_eye.valid=0;
    if(!eye || campaign_player_view.linked_handle!=-1 ||
       !scene_ai_actor_penetration_player_live(campaign_player_object.handle) ||
       !isfinite(campaign_player_damage.state.effects.health))return;
    for(k=0;k<3;k++)if(!isfinite(eye[k]))return;
    memcpy(scene_ai_cane_player_eye.eye,eye,12);
    scene_ai_cane_player_eye.frame=frame;scene_ai_cane_player_eye.handle=campaign_player_object.handle;
    scene_ai_cane_player_eye.epoch=scene_ai_cane_epoch;scene_ai_cane_player_eye.valid=1;
}
static void scene_ai_cane_reset(void)
{
    scene_cane_flight_audio_forget();
    memset(scene_ai_cane_flight_tickets,0,sizeof(scene_ai_cane_flight_tickets));
    scene_cane_visual_reset();
    scene_ai_cane_player_eye_invalidate();
    memset(scene_ai_cane_locks,0,sizeof(scene_ai_cane_locks));
    memset(scene_ai_cane_windups,0,sizeof(scene_ai_cane_windups));
    memset(scene_ai_canes,0,sizeof(scene_ai_canes));++scene_ai_cane_epoch;
    rf_scene_ai_cane[3]=0;
    /* Do not reopen an in-flight callback or reuse tickets on a new timeline. */
}
static void scene_ai_cane_cancel_source(uint32_t handle)
{
    uint32_t i;
    if(!handle || handle==UINT32_MAX)return;
    for(i=0;i<SCENE_AI_CANE_CAPACITY;i++){
        scene_ai_cane_windup *p=scene_ai_cane_windups+i;
        if(!p->phase || p->source!=handle)continue;
        ++rf_scene_cane[3];memset(p,0,sizeof(*p));
    }
    /* No refund, and already released projectiles retain their own source. */
}
static uint32_t scene_ai_cane_source_pending(uint32_t handle)
{
    uint32_t i;
    if(!handle || handle==UINT32_MAX)return 0;
    for(i=0;i<SCENE_AI_CANE_CAPACITY;i++)
        if(scene_ai_cane_windups[i].phase && scene_ai_cane_windups[i].source==handle)return 1;
    return 0;
}
static uint32_t scene_ai_cane_pending(void)
{
    uint32_t i;
    if(scene_ai_cane_servicing || scene_ai_cane_presenting || scene_ai_cane_updating)return 1;
    for(i=0;i<SCENE_AI_CANE_CAPACITY;i++)
        if(scene_ai_cane_windups[i].phase || scene_ai_canes[i].flight.active)return 1;
    return 0;
}
static campaign_npc_body *scene_ai_cane_source(const scene_ai_cane_windup *pending)
{
    campaign_npc_body *owner=scene_ai_drone_smash_npc_identity(pending->source_slot,pending->source);
    if(!owner || (owner->object_flags&2u) || (owner->view.flags_810&1u) ||
       !isfinite(owner->damage.effects.health) || owner->damage.effects.health<=0 ||
       owner->view.weapons[0]!=pending->weapon || !campaign_enemy_cane_weapon(pending->weapon) ||
       pending->weapon<0 || pending->weapon>=64 || !owner->inventory.owned[pending->weapon])return NULL;
    return owner;
}
/* Required actual held-model/tag admission, before paying and at release.
 * rf_weapon_muzzle_pose has an original eye+.3 fallback; this bounded Cane
 * slice does not silently use that fallback if resource admission is absent. */
static int scene_ai_cane_muzzle_ready(const campaign_npc_body *owner,uint32_t *ready)
{
    int32_t tag;int status;uint32_t model;
    if(!owner || !ready)return RF_RANGE;*ready=0;
    if(!owner->weapon_hand_count || owner->weapon_hand_count>2 || owner->weapon_hands[0]<0)return RF_OK;
    model=rf_weapon_world_model_token(campaign_weapon_models.weapons,owner->view.weapons[0]);
    if(!model)return RF_OK;if(model>campaign_weapon_models.count)return RF_FORMAT;
    status=rf_weapon_world_tag(campaign_weapon_models.weapons,owner->view.weapons[0],1,
        campaign_weapon_tag,NULL,&tag);if(status)return status;
    *ready=tag>=0;return RF_OK;
}
/* Caller already admitted ordinary on-foot actor range/aim/LOS. Reserve and
 * consume once here, then bypass all generic cadence/debit/presentation/hitscan.
 * Target is diagnostic only: due follows the owner's current eye direction. */
static int scene_ai_cane_begin(uint32_t source_slot,uint32_t target,uint32_t frame,
    const float player_eye[3],uint32_t *started)
{
    scene_ai_cane_windup next={0},*pending=NULL;campaign_npc_body *owner;
    const rf_weapon_acquire_definition *ammo;const rf_weapon_primary_definition *primary;
    uint32_t i,remaining,due,burst_before,muzzle_ready;int status;
    int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    if(!started || !player_eye || !campaign_npc_bodies || source_slot>=campaign_npc_body_count)return RF_RANGE;
    *started=0;
    if(rf_scene_swim_test_enabled || scene_ai_cane_servicing || scene_ai_cane_presenting || scene_ai_cane_updating)return RF_OK;
    owner=campaign_npc_bodies+source_slot;
    next.source_slot=source_slot;next.source=owner->registration.handle;
    next.weapon=owner->view.weapons[0];next.start_frame=frame;
    owner=scene_ai_cane_source(&next);if(!owner || (owner->object_flags&0x4000u))return RF_OK;
    if(owner->view.linked_handle!=-1 || scene_npc_seat_attached(next.source) ||
       campaign_npc_holster_blocks_fire(owner))return RF_OK;
    status=scene_ai_cane_muzzle_ready(owner,&muzzle_ready);if(status)return status;
    if(!muzzle_ready)return RF_OK;
    ammo=campaign_weapon_supply.definitions+next.weapon;
    if(ammo->magazine || ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->capacity!=200 ||
       owner->inventory.loaded[next.weapon] || owner->inventory.reserve[ammo->ammo_type]<=0)return RF_OK;
    for(i=0;i<SCENE_AI_CANE_CAPACITY;i++){
        scene_ai_cane_windup *p=scene_ai_cane_windups+i;
        if(p->phase && p->source==next.source){++rf_scene_cane[5];return RF_OK;}
        if(!p->phase && !scene_ai_canes[i].flight.active && !pending)pending=p;
    }
    if(!pending){++rf_scene_cane[4];++rf_scene_ai_cane[4];return RF_OK;}
    if(scene_ai_cane_ticket==UINT64_MAX)return RF_RANGE;
    status=rf_timer_set(&next.due_ms,now,scene_ai_cane_definition.delay_ms);
    if(status)return status;
    primary=&scene_ai_cane_definition.flight.primary;
    remaining=owner->combat_burst_remaining;due=owner->combat_due;
    status=campaign_enemy_cadence(primary,frame,&remaining,&due);
    if(status)return status;
    /* Callback-free acceptance: no failed operation follows the real debit
     * before cadence and reservation publication. Release never checks reserve. */
    status=rf_weapon_consume_shot(&owner->inventory,campaign_weapon_supply.definitions,
        campaign_weapon_supply.names.count,next.weapon);
    if(status)return status;
    next.ticket=++scene_ai_cane_ticket;next.phase=1;*pending=next;
    burst_before=owner->combat_burst_remaining;
    owner->combat_burst_remaining=remaining;owner->combat_due=due;
    *started=1;++rf_scene_cane[0];++rf_scene_enemy_combat[2];
    rf_scene_cane[7]=next.source;rf_scene_cane[8]=target;rf_scene_cane[9]=frame;
    if(rf_scene_combat_trace)printf("ENEMY_SHOT_TRACE %u %u\n",frame,campaign_seeds.records.items[source_slot].record.uid);
    scene_ai_cane_presenting=1;
    status=campaign_enemy_fire_presentation(source_slot,1,primary,burst_before,0);
    scene_ai_cane_presenting=0;rf_scene_enemy_fire[5]=(uint32_t)status;
    if(status==RF_NOT_FOUND)++rf_scene_enemy_fire[2];
    else if(status){if(pending->ticket==next.ticket)memset(pending,0,sizeof(*pending));return status;}
    if(pending->ticket!=next.ticket || pending->phase!=1)return RF_OK;
    if(!scene_ai_cane_source(pending)){scene_ai_cane_cancel_source(next.source);return RF_OK;}
    pending->phase=2;++rf_scene_cane[1];return RF_OK;
}
/* Service before all ordinary ammo/fallback, target, mode, pain and cadence
 * gates. A pending source must also bypass the generic ammo block afterward,
 * since accepting its last cell leaves reserve zero during this real delay.
 * No target lookup, visibility/cover retest, range gate or second debit.
 * Homing begins only after factory publication, never during the paid delay.
 * Hidden0x4000 defers upstream at41daf0->40a110; keep the old deadline
 * and release once on a visible update if it has already expired. */
static int scene_ai_cane_service(uint32_t frame,const float player_eye[3])
{
    uint32_t i,k;uint64_t epoch=scene_ai_cane_epoch;int status=RF_OK;
    int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    if(!player_eye)return RF_RANGE;
    if(scene_ai_cane_servicing || scene_ai_cane_presenting || scene_ai_cane_updating)return RF_OK;
    scene_ai_cane_servicing=1;
    for(i=0;i<SCENE_AI_CANE_CAPACITY;i++){
        scene_ai_cane_windup *p=scene_ai_cane_windups+i,accepted;
        scene_ai_laser_projectile shot;campaign_npc_body *owner;
        rf_entity_pose *pose=NULL;float muzzle[3],basis[9];uint32_t muzzle_ready;int expired;
        if(p->phase!=2)continue;
        owner=scene_ai_cane_source(p);
        if(!owner){scene_ai_cane_cancel_source(p->source);continue;}
        if(owner->object_flags&0x4000u)continue;
        status=rf_timer_expired(p->due_ms,now,&expired);
        if(status)break;
        if(!expired)continue;
        accepted=*p;memset(p,0,sizeof(*p)); /* Original4093aa consumes before factory. */
        ++rf_scene_cane[2];rf_scene_cane[10]=frame;
        /* Original41b25d->41b5a0 uses the real held Cane position. Keep its
         * pose current;409557 replaces only the returned aim basis. */
        status=scene_ai_cane_muzzle_ready(owner,&muzzle_ready);
        if(status){++rf_scene_cane[6];break;}
        if(!muzzle_ready){++rf_scene_cane[6];continue;}
        status=campaign_model_pose(accepted.source_slot,&pose);
        if(!status && !pose)status=RF_NOT_FOUND;
        if(!status)status=campaign_model_pose_ready(accepted.source_slot,pose);
        if(!status)status=rf_scene_npc_muzzle(accepted.source,accepted.weapon,0,-1,muzzle,basis);
        if(status){++rf_scene_cane[6];break;}
        if(epoch!=scene_ai_cane_epoch)break;
        owner=scene_ai_cane_source(&accepted);
        if(!owner){++rf_scene_cane[6];continue;}
        for(k=0;k<9;k++)if(!isfinite(owner->look.orientation[k]))break;
        if(k!=9){status=RF_RANGE;break;}
        memset(&shot,0,sizeof(shot));
        status=rf_weapon_flight_launch(&shot.flight,muzzle,owner->look.orientation+6,
            scene_ai_cane_definition.flight.speed,scene_ai_cane_definition.flight.lifetime,
            scene_ai_cane_definition.flight.radius);
        if(status){++rf_scene_cane[6];break;}
        memcpy(shot.basis,owner->look.orientation,sizeof(shot.basis));
        shot.source=accepted.source;shot.weapon=accepted.weapon;
        shot.damage=combat_enemy_primary_damage(&scene_ai_cane_definition.flight.primary);
        shot.damage_kind=scene_ai_cane_definition.flight.primary.damage_kind;
        shot.liquid.query_flags=0x1004u;
        scene_ai_canes[i]=shot;
        scene_ai_cane_flight_tickets[i]=accepted.ticket;
        scene_ai_cane_locks[i].target=scene_ai_cane_locks[i].target_slot=UINT32_MAX;
        scene_ai_cane_locks[i].scan_due=now; /*4c7c17->4fa360: zero OFFSET, due now.*/
        ++rf_scene_ai_cane[0];++rf_scene_ai_cane[3];
        rf_scene_ai_cane[8]=accepted.source;rf_scene_ai_cane[10]=frame;
        scene_cane_flight_audio_accept(i,accepted.source,accepted.ticket);
        if(epoch!=scene_ai_cane_epoch)break;
        scene_cane_visual_accept(i,&scene_ai_canes[i],frame);
    }
    rf_scene_cane[11]=(uint32_t)status;scene_ai_cane_servicing=0;return status;
}
#endif
