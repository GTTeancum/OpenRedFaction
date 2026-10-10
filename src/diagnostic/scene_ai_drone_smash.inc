#ifndef RF_SCENE_AI_DRONE_SMASH_INCLUDED
#define RF_SCENE_AI_DRONE_SMASH_INCLUDED
/* Drone-only first-playable delayed melee, separate from immediate creature
 * melee. Original UID844 in L3S4 selects Drone Smash. RF.exe42606f..4260ce
 * starts action2, then schedules int(seconds*1000+.5); authored delay is400ms.
 * Include after scene_ai_melee_contact.inc and scene_authored_holster.inc.
 * Pending entries are runtime-only, never RFNC animation state. Save/export
 * must reject a live reservation/strike; load preparation must not clear it.
 * All cancellation uses full registry handles, never an authored UID/index.
 */
#define SCENE_AI_DRONE_SMASH_PENDING 64u
static struct {
    rf_weapon_primary_definition primary;int32_t weapon,delay_ms;uint32_t ready;
} scene_ai_drone_smash_definition;
typedef struct scene_ai_drone_smash_strike {
    uint64_t ticket;
    uint32_t phase,source,source_slot,target,target_slot,order,order_target;
    int32_t weapon,ai_action,due_ms;
} scene_ai_drone_smash_strike;
/* phase0 is not pending;1 reserves a slot across action callbacks;2 is armed.
 * A consumed slot temporarily retains its ticket/handles during contact, so
 * cancel/reset/reentrant reuse can invalidate the copied strike after a
 * callback. No damage is pending in phase0 and no stored pointer owns an NPC. */
static scene_ai_drone_smash_strike scene_ai_drone_smash_pending[SCENE_AI_DRONE_SMASH_PENDING];
static uint64_t scene_ai_drone_smash_ticket;
static uint32_t scene_ai_drone_smash_servicing;
/* starts,armed,due contacts,body contacts,positive damage,cancelled,misses,
 * pool full,last source,last target,last impact frame,last status */
uint32_t rf_scene_drone_smash[12];

static void scene_ai_drone_smash_reset(void)
{
    memset(scene_ai_drone_smash_pending,0,sizeof(scene_ai_drone_smash_pending));
    /* Never reuse a ticket across a callback which publishes a new timeline. */
}
static void scene_ai_drone_smash_cancel_source(uint32_t handle)
{
    uint32_t i;if(!handle || handle==UINT32_MAX)return;
    for(i=0;i<SCENE_AI_DRONE_SMASH_PENDING;i++) {
        scene_ai_drone_smash_strike *p=scene_ai_drone_smash_pending+i;
        if(p->source!=handle)continue;
        rf_scene_drone_smash[5]+=p->phase!=0;memset(p,0,sizeof(*p));
    }
}
static void scene_ai_drone_smash_cancel_handle(uint32_t handle)
{
    uint32_t i;if(!handle || handle==UINT32_MAX)return;
    for(i=0;i<SCENE_AI_DRONE_SMASH_PENDING;i++) {
        scene_ai_drone_smash_strike *p=scene_ai_drone_smash_pending+i;
        if(p->source!=handle && p->target!=handle)continue;
        rf_scene_drone_smash[5]+=p->phase!=0;memset(p,0,sizeof(*p));
    }
}
static uint32_t scene_ai_drone_smash_save_pending(void)
{
    uint32_t i;if(scene_ai_drone_smash_servicing)return 1; /* No reentrant save mid-impact. */
    for(i=0;i<SCENE_AI_DRONE_SMASH_PENDING;i++)
        if(scene_ai_drone_smash_pending[i].phase)return 1;
    return 0;
}
static int campaign_enemy_drone_smash_open(rf_vpp *tables,
    const rf_weapon_supply_catalog *supply,const rf_weapon_reset_catalog *reset)
{
    rf_weapon_primary_definition primary;rf_weapon_impact_delays delays;
    const rf_weapon_descriptor *flags;int32_t weapon,delay;int status;
    if(!tables || !supply || !reset || supply->names.count>64 ||
       supply->names.primary_count>supply->names.count ||
       reset->names.count!=supply->names.count ||
       reset->names.primary_count!=supply->names.primary_count)return RF_RANGE;
    weapon=rf_weapon_name_find(&supply->names,"Drone Smash");
    if(weapon<0)return RF_NOT_FOUND;
    if(weapon>=64 || (uint32_t)weapon>=supply->names.primary_count ||
       rf_weapon_name_find(&reset->names,"Drone Smash")!=weapon)return RF_FORMAT;
    flags=reset->definitions+weapon;
    if(flags->flags_264!=0x620u || flags->flags_268!=8u ||
       supply->definitions[weapon].magazine!=0)return RF_FORMAT;
    status=rf_weapon_primary_load(tables,"Drone Smash",128*1024,&primary);if(status)return status;
    status=rf_weapon_impact_delays_load(tables,"Drone Smash",128*1024,&delays);if(status)return status;
    if(primary.magazine!=0 || primary.reload_seconds!=0 || primary.projectiles!=1 ||
       primary.damage_kind!=0 || primary.damage!=1600.f || primary.fire_seconds!=1.5f ||
       primary.ai_attack_range!=5.f || primary.burst_count!=1 || primary.burst_seconds!=0 ||
       primary.piercing_enabled || !isfinite(primary.ai_damage_scale[0]) ||
       primary.ai_damage_scale[0]<=0 ||
       delays.primary_count!=1 || delays.alt_count!=0 || delays.primary_seconds[0]!=.4f)return RF_FORMAT;
    delay=(int32_t)(delays.primary_seconds[0]*1000.f+.5f);
    if(delay!=400)return RF_FORMAT;
    /* Publish only fully admitted metadata. This is NOT a timeline reset and
     * failure must not erase any existing pending strike. Called at startup. */
    scene_ai_drone_smash_definition.primary=primary;
    scene_ai_drone_smash_definition.weapon=weapon;
    scene_ai_drone_smash_definition.delay_ms=delay;
    scene_ai_drone_smash_definition.ready=1;return RF_OK;
}
static uint32_t campaign_enemy_drone_smash_weapon(int32_t weapon)
{return scene_ai_drone_smash_definition.ready && weapon==scene_ai_drone_smash_definition.weapon;}
static campaign_npc_body *scene_ai_drone_smash_npc_identity(uint32_t slot,uint32_t handle)
{
    campaign_npc_body *o;
    if(!campaign_npc_bodies || slot>=campaign_npc_body_count || !handle || handle==UINT32_MAX)return NULL;
    o=campaign_npc_bodies+slot;
    if(o->registration.handle!=handle || o->registration.view!=&o->view ||
       !o->body.allocated_bytes || (uint32_t)o->view.handle!=handle ||
       o->damage.effects.handle!=handle ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&o->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&o->view)return NULL;
    return o;
}
static uint32_t scene_ai_drone_smash_npc_live(const campaign_npc_body *o)
{
    return o && !(o->object_flags&(2u|0x4000u)) && !(o->view.flags_810&1u) &&
        isfinite(o->damage.effects.health) && o->damage.effects.health>0;
}
static int campaign_enemy_drone_smash_select(const campaign_npc_body *owner,
    const rf_weapon_supply_catalog *supply,campaign_enemy_weapon_selection *out)
{
    campaign_enemy_weapon_selection selected={0};uint32_t handle;int32_t weapon;
    if(!owner || !supply || !out || supply->names.count>64 ||
       supply->names.primary_count>supply->names.count)return RF_RANGE;
    weapon=owner->view.weapons[0];if(!campaign_enemy_drone_smash_weapon(weapon))return RF_NOT_FOUND;
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=supply->names.primary_count)return RF_RANGE;
    handle=owner->registration.handle;
    if(!owner->inventory.owned[weapon] || !handle || handle==UINT32_MAX ||
       owner->registration.view!=&owner->view || !owner->body.allocated_bytes ||
       (uint32_t)owner->view.handle!=handle || owner->damage.effects.handle!=handle ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view ||
       !scene_ai_drone_smash_npc_live(owner))return RF_NOT_FOUND;
    selected.primary=&scene_ai_drone_smash_definition.primary;
    selected.slot=UINT32_MAX;selected.melee=1; /* Delayed branch MUST intercept. */
    selected.ammo=NULL;selected.penetrates_world=0;selected.reserve_fed=0;
    *out=selected;return RF_OK;
}
static uint32_t scene_ai_drone_smash_player_live(uint32_t handle)
{
    return handle && handle!=UINT32_MAX && campaign_player_object.handle==handle &&
        campaign_player_object.view==&campaign_player_view && scene_actor_body.allocated_bytes &&
        campaign_player_view.linked_handle==-1 &&
        (uint32_t)campaign_player_view.handle==handle &&
        campaign_player_damage.state.effects.handle==handle &&
        rf_object_registry_lookup(&campaign_registry,handle)==&campaign_player_object &&
        rf_entity_lookup(&campaign_entities,(int32_t)handle)==&campaign_player_view &&
        !(campaign_player_view.flags_7c&(2u|0x4000u)) && !(campaign_player_view.flags_810&1u) &&
        isfinite(campaign_player_damage.state.effects.health) && campaign_player_damage.state.effects.health>0;
}
/* Resolve only the accepted pair. Ordinary mode0 means the same player;
 * explicit/reactive mode1/2 means the exact retained combat_target. */
static uint32_t scene_ai_drone_smash_live(const scene_ai_drone_smash_strike *strike,
    const float player_eye[3],campaign_npc_body **source,campaign_npc_body **victim,
    const float **target_eye,const rf_physics_body **target_body)
{
    campaign_enemy_weapon_selection selected={0};campaign_npc_body *o,*v=NULL;
    float speed2=0;uint32_t k;
    if(!strike || !player_eye || rf_scene_swim_test_enabled)return 0;
    o=scene_ai_drone_smash_npc_identity(strike->source_slot,strike->source);
    if(!o || campaign_enemy_drone_smash_select(o,&campaign_weapon_supply,&selected) ||
       o->view.weapons[0]!=strike->weapon || o->ai_mode.action_280!=strike->ai_action ||
       !o->combat_alert || o->combat_scripted!=strike->order ||
       o->combat_target!=strike->order_target || o->script_once.count ||
       o->script_shoot.active || o->script_animation.active || o->view.linked_handle!=-1 ||
       campaign_npc_holster_blocks_fire(o) ||
       scene_npc_seat_attached(strike->source) ||
       scene_turret_generated_activation_suppresses(strike->source) ||
       scene_turret_operator_suppresses(strike->source))return 0;
    if(strike->order>2 || (strike->order?o->combat_target:campaign_player_object.handle)!=strike->target)return 0;
    if(strike->target==strike->source)return 0;
    if(strike->target_slot==UINT32_MAX) {
        if(!scene_ai_drone_smash_player_live(strike->target))return 0;
        *target_eye=player_eye;*target_body=&scene_actor_body;
    } else {
        v=scene_ai_drone_smash_npc_identity(strike->target_slot,strike->target);
        if(!scene_ai_drone_smash_npc_live(v) || v->view.linked_handle!=-1 ||
           scene_npc_seat_attached(strike->target))return 0;
        *target_eye=v->eye_position;*target_body=&v->body;
    }
    for(k=0;k<3;k++)speed2+=(*target_body)->state.velocity[k]*(*target_body)->state.velocity[k];
    if(!isfinite(speed2) || !campaign_enemy_mode_admits(o,speed2>.0001f,0))return 0;
    *source=o;*victim=v;return 1;
}
/* Conservative full 3D 30-degree cone and eye-distance bound. This is a port
 * contact policy, not retail flight steering; existing pursuit stays horizontal. */
static uint32_t scene_ai_drone_smash_geometry(const campaign_npc_body *source,
    const float target_eye[3],float delta[3],float *distance)
{
    float squared=0,forward=0,length=0,reach=scene_ai_drone_smash_definition.primary.ai_attack_range;
    uint32_t k;
    for(k=0;k<3;k++) {
        float f=source->look.orientation[6+k];
        if(!isfinite(source->eye_position[k]) || !isfinite(target_eye[k]) || !isfinite(f))return 0;
        delta[k]=target_eye[k]-source->eye_position[k];
        squared+=delta[k]*delta[k];forward+=f*delta[k];length+=f*f;
    }
    if(!isfinite(squared) || !isfinite(forward) || !isfinite(length) ||
       squared<.0001f || squared>reach*reach || length<=0 || forward<=0 ||
       forward*forward<.75f*length*squared)return 0;
    *distance=squared;return 1;
}
static int scene_ai_drone_smash_begin(scene_stream *stream,uint32_t source_slot,
    uint32_t target,uint32_t frame,const float player_eye[3],uint32_t *started)
{
    scene_ai_drone_smash_strike next={0},*pending=NULL;campaign_npc_body *owner,*victim;
    const rf_physics_body *body;const float *eye;float delta[3],distance,maximum;
    uint32_t i,remaining,due,blocked,burst_before;int32_t deadline;
    int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);int status;
    if(!stream || !player_eye || !started || source_slot>=campaign_npc_body_count || !campaign_npc_bodies)return RF_RANGE;
    *started=0;owner=campaign_npc_bodies+source_slot;
    next.source=owner->registration.handle;next.source_slot=source_slot;
    next.target=target;next.target_slot=UINT32_MAX;next.weapon=owner->view.weapons[0];
    next.order=owner->combat_scripted;next.order_target=owner->combat_target;next.ai_action=owner->ai_mode.action_280;
    if(target!=campaign_player_object.handle) {
        for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==target){next.target_slot=i;break;}
        if(i==campaign_npc_body_count)return RF_OK; /* No point/vehicle approximation. */
    }
    if(!scene_ai_drone_smash_live(&next,player_eye,&owner,&victim,&eye,&body) ||
       !scene_ai_drone_smash_geometry(owner,eye,delta,&distance))return RF_OK;
    for(i=0;i<SCENE_AI_DRONE_SMASH_PENDING;i++) {
        scene_ai_drone_smash_strike *p=scene_ai_drone_smash_pending+i;
        if(p->phase && p->source==next.source)return RF_OK;
        if(!p->phase && !pending)pending=p;
    }
    if(!pending){++rf_scene_drone_smash[7];return RF_OK;}
    status=combat_obstructed(stream,owner->eye_position,delta,&blocked);if(status || blocked)return status;
    status=rf_timer_set(&deadline,now,scene_ai_drone_smash_definition.delay_ms);if(status)return status;
    remaining=owner->combat_burst_remaining;due=owner->combat_due;
    status=campaign_enemy_cadence(&scene_ai_drone_smash_definition.primary,frame,&remaining,&due);if(status)return status;
    if(scene_ai_drone_smash_ticket==UINT64_MAX)return RF_RANGE;
    next.ticket=++scene_ai_drone_smash_ticket;next.phase=1;next.due_ms=-1;*pending=next;
    burst_before=owner->combat_burst_remaining;owner->combat_burst_remaining=remaining;owner->combat_due=due;
    *started=1;++rf_scene_drone_smash[0];++rf_scene_enemy_melee[0];++rf_scene_enemy_combat[2];
    memcpy(&maximum,rf_scene_enemy_melee+3,4);if(distance>maximum)memcpy(rf_scene_enemy_melee+3,&distance,4);
    if(rf_scene_combat_trace)printf("ENEMY_SHOT_TRACE %u %u\n",frame,campaign_seeds.records.items[source_slot].record.uid);
    status=campaign_enemy_fire_presentation(source_slot,1,&scene_ai_drone_smash_definition.primary,burst_before,0);
    rf_scene_enemy_fire[5]=(uint32_t)status;
    if(status==RF_NOT_FOUND)++rf_scene_enemy_fire[2];
    else if(status){if(pending->ticket==next.ticket)memset(pending,0,sizeof(*pending));return status;}
    /* Cancellation during action/audio callbacks cannot re-arm the old shot. */
    if(pending->ticket!=next.ticket || pending->phase!=1)return RF_OK;
    if(!scene_ai_drone_smash_live(&next,player_eye,&owner,&victim,&eye,&body)) {
        scene_ai_drone_smash_cancel_source(next.source);return RF_OK;
    }
    pending->due_ms=deadline;pending->phase=2;++rf_scene_drone_smash[1];return RF_OK;
}
static int scene_ai_drone_smash_impact(scene_stream *stream,uint32_t frame,
    const float player_eye[3],scene_ai_drone_smash_strike *slot,
    const scene_ai_drone_smash_strike *strike)
{
    campaign_npc_body *owner,*victim;const rf_physics_body *body;const float *eye;
    float delta[3],distance,amount=0,seconds=(float)frame/60;
    float source_eye[3],source_basis[9],target_eye[3];rf_physics_body_state target_state;
    uint32_t contact,clock_bits;int status;
    int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    combat_feedback feedback={now,0};
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&feedback};
    rf_damage_request request={combat_enemy_primary_damage(&scene_ai_drone_smash_definition.primary),
        strike->source,scene_ai_drone_smash_definition.primary.damage_kind,0,UINT32_MAX,0};
    if(slot->ticket!=strike->ticket || !scene_ai_drone_smash_live(strike,player_eye,&owner,&victim,&eye,&body) ||
       !scene_ai_drone_smash_geometry(owner,eye,delta,&distance)){++rf_scene_drone_smash[6];return RF_OK;}
    memcpy(source_eye,owner->eye_position,12);memcpy(source_basis,owner->look.orientation,36);
    memcpy(target_eye,eye,12);target_state=body->state;
    status=campaign_enemy_melee_contact(stream,source_eye,delta,
        scene_ai_drone_smash_definition.primary.ai_attack_range,body,request.amount,&contact);
    if(status)return status;
    if(!contact){++rf_scene_drone_smash[6];return RF_OK;}
    /* Rubble contact can invoke mutation callbacks. Requalify both full
     * handles, exact selection/order and geometry before dispatching damage.
     * A changed transform is a miss, never a second damaging contact query. */
    if(slot->ticket!=strike->ticket || !scene_ai_drone_smash_live(strike,player_eye,&owner,&victim,&eye,&body) ||
       !scene_ai_drone_smash_geometry(owner,eye,delta,&distance) ||
       memcmp(source_eye,owner->eye_position,12) || memcmp(source_basis,owner->look.orientation,36) ||
       memcmp(target_eye,eye,12) || memcmp(&target_state,&body->state,sizeof(target_state))) {
        ++rf_scene_drone_smash[6];return RF_OK;
    }
    ++rf_scene_drone_smash[3];++rf_scene_enemy_damage_kinds[request.kind];
    memcpy(&clock_bits,&seconds,4);
    if(victim) {
        status=rf_scene_npc_damage(strike->target,&request,1,clock_bits,&effects,&amount);
        if(!status)status=feedback.status;if(status)return status;
        /* Never dereference the pre-damage target after its callbacks. */
        victim=scene_ai_drone_smash_npc_identity(strike->target_slot,strike->target);
        if(victim && victim->damage.effects.health<=0) {
            uint32_t entered;status=rf_scene_npc_death_entry(strike->target,&entered);if(status)return status;
            if(entered && scene_ai_drone_smash_npc_identity(strike->target_slot,strike->target)) {
                victim=campaign_npc_bodies+strike->target_slot;victim->script_move.active=0;
                status=combat_death_start(strike->target_slot);if(status)return status;
            }
        }
    } else {
        status=rf_scene_player_damage(strike->target,&request,1,clock_bits,&effects,&amount);
        if(!status)status=feedback.status;if(status)return status;
        if(amount>0 && campaign_player_object.handle==strike->target)
            campaign_combat_event(frame,1,strike->source,amount,campaign_player_damage.state.effects.health);
    }
    if(amount>0){++rf_scene_drone_smash[4];++rf_scene_enemy_combat[3];}
    if(strike->order && strike->source==rf_scene_script_attack[11] &&
       strike->target==rf_scene_script_attack[3]) {
        float health=0;
        victim=strike->target_slot==UINT32_MAX?NULL:
            scene_ai_drone_smash_npc_identity(strike->target_slot,strike->target);
        if(victim)health=victim->damage.effects.health;
        else if(strike->target_slot==UINT32_MAX && campaign_player_object.handle==strike->target)
            health=campaign_player_damage.state.effects.health;
        ++rf_scene_script_attack[5];memcpy(rf_scene_script_attack+6,&amount,4);memcpy(rf_scene_script_attack+8,&health,4);
    }
    return RF_OK;
}
/* Service once at the very start of enemy_tick, before its swim/owner/mode/
 * combat_due/pain early-outs. Invalid sources/targets cancel even before due.
 * A due strike is cleared BEFORE geometry/rubble/damage/death callbacks.
 * Pain lock alone does not defer impact; an actual weapon-reset interruption
 * is separately cancelled by its lifecycle hook. No current-target lookup can
 * redirect this strike to a replacement NPC or the player. */
static int scene_ai_drone_smash_service(scene_stream *stream,uint32_t frame,const float player_eye[3])
{
    uint32_t i;int status=RF_OK;
    int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    if(!stream || !player_eye)return RF_RANGE;
    if(scene_ai_drone_smash_servicing)return RF_OK;
    scene_ai_drone_smash_servicing=1;
    for(i=0;i<SCENE_AI_DRONE_SMASH_PENDING;i++) {
        scene_ai_drone_smash_strike *p=scene_ai_drone_smash_pending+i,strike;
        campaign_npc_body *owner,*victim;const float *eye;const rf_physics_body *body;int expired;
        if(p->phase!=2)continue;
        if(!scene_ai_drone_smash_live(p,player_eye,&owner,&victim,&eye,&body)) {
            scene_ai_drone_smash_cancel_source(p->source);continue;
        }
        status=rf_timer_expired(p->due_ms,now,&expired);if(status)break;if(!expired)continue;
        strike=*p;p->phase=0; /* Consumed before the first callback. */
        ++rf_scene_drone_smash[2];rf_scene_drone_smash[8]=strike.source;
        rf_scene_drone_smash[9]=strike.target;rf_scene_drone_smash[10]=frame;
        status=scene_ai_drone_smash_impact(stream,frame,player_eye,p,&strike);
        if(p->ticket==strike.ticket)memset(p,0,sizeof(*p));
        if(status)break;
    }
    scene_ai_drone_smash_servicing=0;rf_scene_drone_smash[11]=(uint32_t)status;return status;
}
#endif
