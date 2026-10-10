#ifndef RF_SCENE_AI_DRONE_SECONDARY_INCLUDED
#define RF_SCENE_AI_DRONE_SECONDARY_INCLUDED
/* Ordinary non-Fighter combat405af0 ->40628d calls primary401580 and then
 * secondary406330 ->406390 ->426ca0. This owner consumes only a genuinely
 * acquired live actor target and the exact owned Drone Smash/Missile pair.
 * Include after scene_ai_drone_smash.inc. The projectile implementation is
 * later in scene_ai_rocket.inc; no primary presentation/reset is requested.
 */
#define SCENE_AI_DRONE_SECONDARY_CAPACITY 64u
static int scene_ai_drone_missile_launch(campaign_npc_body *,const float *);
typedef struct scene_ai_drone_secondary_cooldown {
    uint64_t ticket;uint32_t phase,source;int32_t deadline;
} scene_ai_drone_secondary_cooldown;
typedef struct scene_ai_drone_secondary_pair {
    uint32_t source,source_slot,target,target_slot,order,order_target;
    int32_t primary,secondary,ai_action;
} scene_ai_drone_secondary_pair;
/* phase1 reserves across launch callbacks; phase2 owns the four-second wait.
 * Full handles prevent index/UID reuse from inheriting another actor's timer.
 * Target, order, selection, pain, holster and visibility changes do not reset
 * a living source's cooldown. Entries expire even while combat is suppressed.
 */
static scene_ai_drone_secondary_cooldown scene_ai_drone_secondary_cooldowns[SCENE_AI_DRONE_SECONDARY_CAPACITY];
static uint64_t scene_ai_drone_secondary_ticket;
static uint32_t scene_ai_drone_secondary_launching;
/* qualified attempts,launches,cooldown holds,geometry/LOS holds,pain holds,
 * unavailable/pool refusals,cooldown-bank full,last source,last target,
 * last launch frame,last deadline,last status */
uint32_t rf_scene_drone_secondary[12];

static int32_t scene_ai_drone_secondary_now(uint32_t frame)
{return (int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);}
static void scene_ai_drone_secondary_reset(void)
{
    memset(scene_ai_drone_secondary_cooldowns,0,sizeof(scene_ai_drone_secondary_cooldowns));
    /* Do not reuse tickets or reopen reentrant launch after publication. */
}
static int scene_ai_drone_secondary_service(uint32_t frame)
{
    uint32_t i;int32_t now=scene_ai_drone_secondary_now(frame);int expired,status;
    for(i=0;i<SCENE_AI_DRONE_SECONDARY_CAPACITY;i++) {
        scene_ai_drone_secondary_cooldown *p=scene_ai_drone_secondary_cooldowns+i;
        if(p->phase!=2)continue;
        status=rf_timer_expired(p->deadline,now,&expired);if(status)return status;
        if(expired)memset(p,0,sizeof(*p));
    }
    return RF_OK;
}
/* Save/export only. RFNC has no independent secondary-deadline field. A
 * failed save or load PREPARATION must never clear or postpone these entries.
 * Read-only expiry permits a save as soon as the real cooldown has elapsed,
 * even if the normal enemy service has not yet reclaimed its slot this tick.
 */
static uint32_t scene_ai_drone_secondary_save_pending(void)
{
    uint32_t i;int expired;
    if(scene_ai_drone_secondary_launching)return 1;
    for(i=0;i<SCENE_AI_DRONE_SECONDARY_CAPACITY;i++) {
        const scene_ai_drone_secondary_cooldown *p=scene_ai_drone_secondary_cooldowns+i;
        if(!p->phase)continue;
        if(p->phase==1 || combat_frame==UINT32_MAX ||
           rf_timer_expired(p->deadline,scene_ai_drone_secondary_now(combat_frame),&expired) || !expired)return 1;
    }
    return 0;
}
/* Requalify the accepted pair after any sight query. Only actual on-foot
 * actors are represented here; a point, hull or rider is never approximated
 * by an unrelated player body. Ordinary mode0 targets the acquired player;
 * explicit/reactive mode1/2 retains the exact combat_target handle.
 */
static uint32_t scene_ai_drone_secondary_live(const scene_ai_drone_secondary_pair *pair,
    const float player_eye[3],campaign_npc_body **source,const float **target_eye,
    const rf_physics_body **target_body)
{
    campaign_npc_body *o,*v;uint32_t k;float speed2=0;
    o=scene_ai_drone_smash_npc_identity(pair->source_slot,pair->source);
    if(!scene_ai_drone_smash_npc_live(o) || !o->combat_alert ||
       o->view.weapons[0]!=pair->primary || o->view.weapons[1]!=pair->secondary ||
       !campaign_enemy_drone_smash_weapon(pair->primary) ||
       !o->inventory.owned[pair->primary] || !o->inventory.owned[pair->secondary] ||
       o->combat_scripted!=pair->order || o->combat_target!=pair->order_target ||
       o->ai_mode.action_280!=pair->ai_action || o->view.linked_handle!=-1 ||
       o->script_once.count || o->script_shoot.active || o->script_animation.active ||
       campaign_npc_holster_blocks_fire(o) || scene_npc_seat_attached(pair->source) ||
       scene_turret_generated_activation_suppresses(pair->source) ||
       scene_turret_operator_suppresses(pair->source))return 0;
    if(pair->order>2 || pair->target==pair->source ||
       (pair->order?o->combat_target:campaign_player_object.handle)!=pair->target)return 0;
    if(pair->target_slot==UINT32_MAX) {
        if(!scene_ai_drone_smash_player_live(pair->target))return 0;
        *target_eye=player_eye;*target_body=&scene_actor_body;
    } else {
        v=scene_ai_drone_smash_npc_identity(pair->target_slot,pair->target);
        if(!scene_ai_drone_smash_npc_live(v) || v->view.linked_handle!=-1 ||
           scene_npc_seat_attached(pair->target))return 0;
        *target_eye=v->eye_position;*target_body=&v->body;
    }
    for(k=0;k<3;k++)speed2+=(*target_body)->state.velocity[k]*(*target_body)->state.velocity[k];
    if(!isfinite(speed2) || !campaign_enemy_mode_admits(o,speed2>.0001f,0))return 0;
    *source=o;return 1;
}
static int scene_ai_drone_secondary_sight(void *context,const float start[3],
    const float end[3],uint32_t *blocked)
{
    scene_stream *stream=context;float delta[3],fraction;uint32_t k,clutter;int status;
    for(k=0;k<3;k++)delta[k]=end[k]-start[k];
    status=combat_obstructed(stream,start,delta,blocked);if(status || *blocked)return status;
    status=campaign_clutter_firearm_select(start,delta,1,&clutter,&fraction);
    if(!status)*blocked=clutter!=UINT32_MAX;return status;
}
static uint32_t scene_ai_drone_secondary_geometry(const campaign_npc_body *owner,
    const float eye[3],const rf_physics_body *body,rf_weapon_ai_secondary_input *in)
{
    uint32_t k;
    if(!isfinite(owner->body.state.bounds.radius) || owner->body.state.bounds.radius<0 ||
       !isfinite(body->state.bounds.radius) || body->state.bounds.radius<0)return 0;
    for(k=0;k<3;k++) {
        if(!isfinite(owner->body.state.position[k]) || !isfinite(owner->eye_position[k]) ||
           !isfinite(owner->body.state.orientation[6+k]) || !isfinite(eye[k]))return 0;
        in->source[k]=owner->body.state.position[k];in->eye[k]=owner->eye_position[k];
        in->forward[k]=owner->body.state.orientation[6+k];in->target[k]=eye[k];
    }
    in->source_radius=owner->body.state.bounds.radius;in->target_radius=body->state.bounds.radius;
    return 1;
}
/* Dispatch after acquisition/live-target resolution, before ANY primary
 * ammo/cadence/range gates. Success must not continue the caller's NPC loop:
 * primary admission/service remain independent of an accepted missile.
 */
static int scene_ai_drone_secondary_tick(scene_stream *stream,uint32_t source_slot,
    uint32_t target,uint32_t frame,const float player_eye[3],uint32_t *fired)
{
    scene_ai_drone_secondary_pair pair={0};scene_ai_drone_secondary_cooldown *slot=NULL;
    scene_ai_drone_secondary_cooldown next={0};campaign_npc_body *owner;
    rf_weapon_ai_secondary_input in={0},again={0};const float *eye;const rf_physics_body *body;
    const rf_weapon_acquire_definition *ammo;int32_t weapon,deadline;uint32_t i,admitted;
    int status,pending;
    if(!stream || !player_eye || !fired || !campaign_npc_bodies || source_slot>=campaign_npc_body_count)return RF_RANGE;
    *fired=0;if(rf_scene_swim_test_enabled || scene_ai_drone_secondary_launching || !scene_drone_missile_resources)return RF_OK;
    weapon=rf_weapon_name_find(&campaign_weapon_supply.names,"Drone Missile");
    owner=campaign_npc_bodies+source_slot;
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=campaign_weapon_supply.names.count ||
       (uint32_t)weapon<campaign_weapon_supply.names.primary_count || owner->view.weapons[1]!=weapon ||
       !campaign_enemy_drone_smash_weapon(owner->view.weapons[0]))return RF_OK;
    ammo=campaign_weapon_supply.definitions+weapon;
    /* Resource admission owns authored metadata. Refuse inconsistent loaded
     * definitions rather than silently synthesizing a generic rocket. */
    if(campaign_drone_missile_primary.fire_seconds!=4.f || campaign_drone_missile_primary.ai_attack_range!=30.f ||
       campaign_drone_missile.damage_radius!=2.f || ammo->magazine!=0 || ammo->ammo_type<0 || ammo->ammo_type>=32)return RF_FORMAT;
    pair.source=owner->registration.handle;pair.source_slot=source_slot;
    pair.target=target;pair.target_slot=UINT32_MAX;pair.primary=owner->view.weapons[0];pair.secondary=weapon;
    pair.order=owner->combat_scripted;pair.order_target=owner->combat_target;pair.ai_action=owner->ai_mode.action_280;
    if(target!=campaign_player_object.handle) {
        for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==target){pair.target_slot=i;break;}
        if(i==campaign_npc_body_count)return RF_OK;
    }
    if(!scene_ai_drone_secondary_live(&pair,player_eye,&owner,&eye,&body))return RF_OK;
    ++rf_scene_drone_secondary[0];in.now_ms=scene_ai_drone_secondary_now(frame);in.deadline=in.now_ms;
    for(i=0;i<SCENE_AI_DRONE_SECONDARY_CAPACITY;i++) {
        scene_ai_drone_secondary_cooldown *p=scene_ai_drone_secondary_cooldowns+i;
        if(p->phase && p->source==pair.source){++rf_scene_drone_secondary[2];return RF_OK;}
        if(!p->phase && !slot)slot=p;
    }
    if(!slot){++rf_scene_drone_secondary[6];return RF_OK;}
    if(owner->inventory.reserve[ammo->ammo_type]<=0){++rf_scene_drone_secondary[5];return RF_OK;}
    status=rf_timer_pending(owner->pain.animation_lock,in.now_ms,&pending);if(status)return status;
    if(pending){++rf_scene_drone_secondary[4];return RF_OK;}
    in.weapon=weapon;in.global_enabled=1;in.ai_flags_530=1;in.target_present=1;in.visible_29c=1;
    in.maximum_range=campaign_drone_missile_primary.ai_attack_range;
    in.minimum_range=campaign_drone_missile.damage_radius;
    if(!scene_ai_drone_secondary_geometry(owner,eye,body,&in))return RF_OK;
    status=rf_weapon_ai_secondary_admit(&in,scene_ai_drone_secondary_sight,stream,&admitted);
    if(status)return status;if(!admitted){++rf_scene_drone_secondary[3];return RF_OK;}
    /* Never launch at a replacement, a different order or moved geometry
     * after callbacks. Copy the admitted target before the launcher runs. */
    if(!scene_ai_drone_secondary_live(&pair,player_eye,&owner,&eye,&body))return RF_OK;
    again=in;if(!scene_ai_drone_secondary_geometry(owner,eye,body,&again) || memcmp(&in,&again,sizeof(in)))return RF_OK;
    status=rf_timer_pending(owner->pain.animation_lock,in.now_ms,&pending);if(status)return status;if(pending)return RF_OK;
    status=rf_timer_set(&deadline,in.now_ms,(int32_t)(campaign_drone_missile_primary.fire_seconds*1000.f));if(status)return status;
    if(scene_ai_drone_secondary_ticket==UINT64_MAX)return RF_RANGE;
    next.ticket=++scene_ai_drone_secondary_ticket;next.phase=1;next.source=pair.source;next.deadline=deadline;*slot=next;
    scene_ai_drone_secondary_launching=1;
    status=scene_ai_drone_missile_launch(owner,in.target);
    scene_ai_drone_secondary_launching=0;rf_scene_drone_secondary[11]=(uint32_t)status;
    if(status) {
        if(slot->ticket==next.ticket)memset(slot,0,sizeof(*slot));
        if(status==RF_NOT_FOUND){++rf_scene_drone_secondary[5];return RF_OK;}return status;
    }
    /* A successful new timeline may have reset the reservation during a
     * callback. Never restore that old timer into the replacement timeline. */
    if(slot->ticket==next.ticket && slot->phase==1)slot->phase=2;
    *fired=1;++rf_scene_drone_secondary[1];++rf_scene_enemy_combat[2];
    rf_scene_drone_secondary[7]=pair.source;rf_scene_drone_secondary[8]=pair.target;
    rf_scene_drone_secondary[9]=frame;rf_scene_drone_secondary[10]=(uint32_t)deadline;
    if(rf_scene_combat_trace)printf("DRONE_SECONDARY_TRACE %u %u %u %d\n",frame,pair.source,pair.target,deadline);
    return RF_OK;
}
#endif
