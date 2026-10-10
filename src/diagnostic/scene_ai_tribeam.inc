#ifndef RF_SCENE_AI_TRIBEAM_INCLUDED
#define RF_SCENE_AI_TRIBEAM_INCLUDED
/* Include after scene_ai_drone_secondary.inc. Original accepted windup
 * 426197..426273 owns action2,5s cadence,+4c0 delay and4257c0 reserve debit.
 * Due409340 clears +4c0 before factory4c77a0. Actual Spike passes CURRENT
 * eye basis at409557..40956b, not a retained target or onset ray. */
#define SCENE_AI_TRIBEAM_CAPACITY 64u
typedef struct scene_ai_tribeam_windup {
    uint64_t ticket;uint32_t phase,source,source_slot,start_frame;
    int32_t weapon,due_ms;
} scene_ai_tribeam_windup;
/* Each pending slot reserves its identically indexed flight slot. Pool
 * pressure is refused before acceptance/debit, never by losing a paid shot.
 * phase1 reserves across presentation; phase2 waits for the real deadline. */
static scene_ai_tribeam_windup scene_ai_tribeam_windups[SCENE_AI_TRIBEAM_CAPACITY];
static scene_ai_laser_projectile scene_ai_tribeams[SCENE_AI_TRIBEAM_CAPACITY];
static uint64_t scene_ai_tribeam_ticket,scene_ai_tribeam_epoch;
static uint32_t scene_ai_tribeam_servicing,scene_ai_tribeam_presenting,scene_ai_tribeam_updating;
/* accepted windups,armed,due releases,cancelled,pool full,duplicate holds,
 * invalid release,last source,last intended target,last onset,last due,status */
uint32_t rf_scene_tribeam[12];
/* Flight counters use the exact Laser flight meanings, in a distinct bank. */
uint32_t rf_scene_ai_tribeam[12];
static int scene_ai_laser_prepare(campaign_npc_body *,const float *,
    const scene_ai_laser_profile *,rf_random_state *,scene_ai_laser_projectile *);
static void scene_ai_tribeam_reset(void)
{
    memset(scene_ai_tribeam_windups,0,sizeof(scene_ai_tribeam_windups));
    memset(scene_ai_tribeams,0,sizeof(scene_ai_tribeams));++scene_ai_tribeam_epoch;
    rf_scene_ai_tribeam[3]=0;
    /* Do not reopen an in-flight callback or reuse tickets on a new timeline. */
}
static void scene_ai_tribeam_cancel_source(uint32_t handle)
{
    uint32_t i;
    if(!handle || handle==UINT32_MAX)return;
    for(i=0;i<SCENE_AI_TRIBEAM_CAPACITY;i++){
        scene_ai_tribeam_windup *p=scene_ai_tribeam_windups+i;
        if(!p->phase || p->source!=handle)continue;
        ++rf_scene_tribeam[3];memset(p,0,sizeof(*p));
    }
    /* No refund, and already released projectiles retain their own source. */
}
static uint32_t scene_ai_tribeam_source_pending(uint32_t handle)
{
    uint32_t i;
    if(!handle || handle==UINT32_MAX)return 0;
    for(i=0;i<SCENE_AI_TRIBEAM_CAPACITY;i++)
        if(scene_ai_tribeam_windups[i].phase && scene_ai_tribeam_windups[i].source==handle)return 1;
    return 0;
}
static uint32_t scene_ai_tribeam_pending(void)
{
    uint32_t i;
    if(scene_ai_tribeam_servicing || scene_ai_tribeam_presenting || scene_ai_tribeam_updating)return 1;
    for(i=0;i<SCENE_AI_TRIBEAM_CAPACITY;i++)
        if(scene_ai_tribeam_windups[i].phase || scene_ai_tribeams[i].flight.active)return 1;
    return 0;
}
static campaign_npc_body *scene_ai_tribeam_source(const scene_ai_tribeam_windup *pending)
{
    campaign_npc_body *owner=scene_ai_drone_smash_npc_identity(pending->source_slot,pending->source);
    if(!owner || (owner->object_flags&2u) || (owner->view.flags_810&1u) ||
       !isfinite(owner->damage.effects.health) || owner->damage.effects.health<=0 ||
       owner->view.weapons[0]!=pending->weapon || !campaign_enemy_tribeam_weapon(pending->weapon) ||
       pending->weapon<0 || pending->weapon>=64 || !owner->inventory.owned[pending->weapon])return NULL;
    return owner;
}
/* Caller already admitted ordinary on-foot actor range/aim/LOS. Reserve and
 * consume once here, then bypass all generic cadence/debit/presentation/hitscan.
 * Target is diagnostic only: due follows the owner's current eye direction. */
static int scene_ai_tribeam_begin(uint32_t source_slot,uint32_t target,uint32_t frame,
    const float player_eye[3],uint32_t *started)
{
    scene_ai_tribeam_windup next={0},*pending=NULL;campaign_npc_body *owner;
    const rf_weapon_acquire_definition *ammo;const rf_weapon_primary_definition *primary;
    uint32_t i,remaining,due,burst_before;int status;
    int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    if(!started || !player_eye || !campaign_npc_bodies || source_slot>=campaign_npc_body_count)return RF_RANGE;
    *started=0;
    if(rf_scene_swim_test_enabled || scene_ai_tribeam_servicing || scene_ai_tribeam_presenting)return RF_OK;
    owner=campaign_npc_bodies+source_slot;
    next.source_slot=source_slot;next.source=owner->registration.handle;
    next.weapon=owner->view.weapons[0];next.start_frame=frame;
    owner=scene_ai_tribeam_source(&next);if(!owner || (owner->object_flags&0x4000u))return RF_OK;
    if(owner->view.linked_handle!=-1 || scene_npc_seat_attached(next.source) ||
       campaign_npc_holster_blocks_fire(owner))return RF_OK;
    ammo=campaign_weapon_supply.definitions+next.weapon;
    if(ammo->magazine || ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->capacity!=200 ||
       owner->inventory.loaded[next.weapon] || owner->inventory.reserve[ammo->ammo_type]<=0)return RF_OK;
    for(i=0;i<SCENE_AI_TRIBEAM_CAPACITY;i++){
        scene_ai_tribeam_windup *p=scene_ai_tribeam_windups+i;
        if(p->phase && p->source==next.source){++rf_scene_tribeam[5];return RF_OK;}
        if(!p->phase && !scene_ai_tribeams[i].flight.active && !pending)pending=p;
    }
    if(!pending){++rf_scene_tribeam[4];++rf_scene_ai_tribeam[4];return RF_OK;}
    if(scene_ai_tribeam_ticket==UINT64_MAX)return RF_RANGE;
    status=rf_timer_set(&next.due_ms,now,scene_ai_tribeam_definition.delay_ms);
    if(status)return status;
    primary=&scene_ai_tribeam_definition.flight.primary;
    remaining=owner->combat_burst_remaining;due=owner->combat_due;
    status=campaign_enemy_cadence(primary,frame,&remaining,&due);
    if(status)return status;
    /* Callback-free acceptance: no failed operation follows the real debit
     * before cadence and reservation publication. Release never checks reserve. */
    status=rf_weapon_consume_shot(&owner->inventory,campaign_weapon_supply.definitions,
        campaign_weapon_supply.names.count,next.weapon);
    if(status)return status;
    next.ticket=++scene_ai_tribeam_ticket;next.phase=1;*pending=next;
    burst_before=owner->combat_burst_remaining;
    owner->combat_burst_remaining=remaining;owner->combat_due=due;
    *started=1;++rf_scene_tribeam[0];++rf_scene_enemy_combat[2];
    rf_scene_tribeam[7]=next.source;rf_scene_tribeam[8]=target;rf_scene_tribeam[9]=frame;
    if(rf_scene_combat_trace)printf("ENEMY_SHOT_TRACE %u %u\n",frame,campaign_seeds.records.items[source_slot].record.uid);
    scene_ai_tribeam_presenting=1;
    status=campaign_enemy_fire_presentation(source_slot,1,primary,burst_before,0);
    scene_ai_tribeam_presenting=0;rf_scene_enemy_fire[5]=(uint32_t)status;
    if(status==RF_NOT_FOUND)++rf_scene_enemy_fire[2];
    else if(status){if(pending->ticket==next.ticket)memset(pending,0,sizeof(*pending));return status;}
    if(pending->ticket!=next.ticket || pending->phase!=1)return RF_OK;
    if(!scene_ai_tribeam_source(pending)){scene_ai_tribeam_cancel_source(next.source);return RF_OK;}
    pending->phase=2;++rf_scene_tribeam[1];return RF_OK;
}
/* Service before all ordinary ammo/fallback, target, mode, pain and cadence
 * gates. A pending source must also bypass the generic ammo block afterward,
 * since accepting its last cell leaves reserve zero during this real delay.
 * No target lookup, visibility/cover retest, range gate, homing or second debit.
 * Hidden0x4000 defers upstream at41daf0->40a110; keep the old deadline
 * and release once on a visible update if it has already expired. */
static int scene_ai_tribeam_service(uint32_t frame,const float player_eye[3])
{
    uint32_t i,k;int status=RF_OK;
    int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    if(!player_eye)return RF_RANGE;
    if(scene_ai_tribeam_servicing || scene_ai_tribeam_presenting)return RF_OK;
    scene_ai_tribeam_servicing=1;
    for(i=0;i<SCENE_AI_TRIBEAM_CAPACITY;i++){
        scene_ai_tribeam_windup *p=scene_ai_tribeam_windups+i,accepted;
        scene_ai_laser_projectile shot;campaign_npc_body *owner;
        rf_random_state random;float target[3];int expired;
        if(p->phase!=2)continue;
        owner=scene_ai_tribeam_source(p);
        if(!owner){scene_ai_tribeam_cancel_source(p->source);continue;}
        if(owner->object_flags&0x4000u)continue;
        status=rf_timer_expired(p->due_ms,now,&expired);
        if(status)break;
        if(!expired)continue;
        accepted=*p;memset(p,0,sizeof(*p)); /* Original4093aa consumes before factory. */
        ++rf_scene_tribeam[2];rf_scene_tribeam[10]=frame;
        for(k=0;k<9;k++)if(!isfinite(owner->look.orientation[k]))break;
        if(k!=9){status=RF_RANGE;break;}
        for(k=0;k<3;k++)target[k]=owner->eye_position[k]+owner->look.orientation[6+k]*30.f;
        random=campaign_enemy_spread_random;
        status=scene_ai_laser_prepare(owner,target,&scene_ai_tribeam_definition.flight,&random,&shot);
        if(status){++rf_scene_tribeam[6];break;}
        /* Actual Spike factory is given its full current eye basis. Retain
         * that roll/orientation for rendering as well as its launch forward. */
        memcpy(shot.basis,owner->look.orientation,sizeof(shot.basis));
        shot.source=accepted.source;shot.weapon=accepted.weapon;
        campaign_enemy_spread_random=random;scene_ai_tribeams[i]=shot;
        ++rf_scene_ai_tribeam[0];++rf_scene_ai_tribeam[3];
        rf_scene_ai_tribeam[8]=accepted.source;rf_scene_ai_tribeam[10]=frame;
    }
    rf_scene_tribeam[11]=(uint32_t)status;scene_ai_tribeam_servicing=0;return status;
}
#endif
