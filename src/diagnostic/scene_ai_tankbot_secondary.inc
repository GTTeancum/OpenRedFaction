#ifndef RF_SCENE_AI_TANKBOT_SECONDARY_INCLUDED
#define RF_SCENE_AI_TANKBOT_SECONDARY_INCLUDED
/* Ordinary non-Fighter combat405af0 ->406294 calls secondary406330,
 * admitted by406390 and launched by426ca0. Only an already acquired actor
 * and the exact owned Tankbot Chaingun/Missile pair enter autonomously.
 * Include after scene_ai_drone_secondary.inc: its identity, geometry and
 * world/clutter sight helpers are weapon-independent first-playable policy.
 * Existing queued Shoot_Once mode1 remains forced, including other primaries.
 */
static int scene_ai_tankbot_missile_launch(campaign_npc_body *,const float *);
typedef struct scene_ai_tankbot_secondary_pair {
    uint32_t source,source_slot,target,target_slot,order,order_target;
    int32_t primary,secondary,ai_action;
} scene_ai_tankbot_secondary_pair;
/* Runtime ownership lives in each allocated NPC, not a separately exhaustible
 * bank. Source is the full registry handle; zero means no deadline. A global
 * callback guard plus reset epoch permits accepted-only publication without
 * temporarily replacing an older forced-shot deadline. Reset never reopens
 * the guard while a launcher is still on the stack. */
static uint64_t scene_ai_tankbot_secondary_epoch;
static uint32_t scene_ai_tankbot_secondary_launching;
/* qualified attempts,autonomous launches,cooldown holds,geometry/LOS holds,
 * pain holds,unavailable/pool refusals,forced launches,last source,last target,
 * last launch frame,last deadline,last status */
uint32_t rf_scene_tankbot_secondary[12];

static int32_t scene_ai_tankbot_secondary_now(uint32_t frame)
{return (int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);}
static void scene_ai_tankbot_secondary_reset(void)
{
    uint32_t i;++scene_ai_tankbot_secondary_epoch;
    if(!campaign_npc_bodies)return;
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_bodies[i].tankbot_secondary_source=0;
        campaign_npc_bodies[i].tankbot_secondary_deadline=0;
    }
}
static int scene_ai_tankbot_secondary_service(uint32_t frame)
{
    uint32_t i;int32_t now=scene_ai_tankbot_secondary_now(frame);int expired,status;
    if(campaign_npc_body_count && !campaign_npc_bodies)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *o=campaign_npc_bodies+i;
        if(!o->tankbot_secondary_source)continue;
        if(o->tankbot_secondary_source!=o->registration.handle)expired=1;
        else {status=rf_timer_expired(o->tankbot_secondary_deadline,now,&expired);if(status)return status;}
        if(expired){o->tankbot_secondary_source=0;o->tankbot_secondary_deadline=0;}
    }
    return RF_OK;
}
/* SAVE/EXPORT ONLY. RFNC cannot encode this independent wait. No guard or
 * clearing belongs in shared candidate capture, NPC assign, or load prepare.
 * Read-only expiry allows saving immediately once the real wait ends. */
static uint32_t scene_ai_tankbot_secondary_save_pending(void)
{
    uint32_t i;int expired;
    if(scene_ai_tankbot_secondary_launching)return 1;
    if(campaign_npc_body_count && !campaign_npc_bodies)return 1;
    for(i=0;i<campaign_npc_body_count;i++) {
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(!o->tankbot_secondary_source || o->tankbot_secondary_source!=o->registration.handle)continue;
        if(combat_frame==UINT32_MAX || rf_timer_expired(o->tankbot_secondary_deadline,
            scene_ai_tankbot_secondary_now(combat_frame),&expired) || !expired)return 1;
    }
    return 0;
}
/* Forced mode1 needs a registered eye/ammo owner, not an allocated physics
 * body. Autonomous admission below retains its stricter body requirement. */
static campaign_npc_body *scene_ai_tankbot_secondary_identity(uint32_t slot,uint32_t handle)
{
    campaign_npc_body *o;
    if(!campaign_npc_bodies || slot>=campaign_npc_body_count || !handle || handle==UINT32_MAX)return NULL;
    o=campaign_npc_bodies+slot;
    if(o->registration.handle!=handle || o->registration.view!=&o->view ||
       (uint32_t)o->view.handle!=handle || o->damage.effects.handle!=handle ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&o->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&o->view)return NULL;
    return o;
}
/* Both real Tankbot launch paths call here. Original Shoot_Once mode1 clears
 * the secondary deadline at4badd5..4baddd before calling426ca0. Port FIFO
 * execution preserves that forced priority but refreshes only after actual
 * finite-ammo publication, never on enqueue, expiry, dry fire or pool refusal.
 * The autonomous exact-primary restriction deliberately does NOT apply here.
 */
static int scene_ai_tankbot_secondary_launch(uint32_t source_slot,
    const float target[3],uint32_t frame,uint32_t forced)
{
    campaign_npc_body *owner;uint32_t source;uint64_t epoch;
    int32_t now,deadline;int expired,status;
    if(!target || !campaign_npc_bodies || source_slot>=campaign_npc_body_count || forced>1)return RF_RANGE;
    if(scene_ai_tankbot_secondary_launching || !scene_tankbot_missile_resources)return RF_NOT_FOUND;
    source=campaign_npc_bodies[source_slot].registration.handle;
    owner=scene_ai_tankbot_secondary_identity(source_slot,source);
    if(!scene_ai_drone_smash_npc_live(owner))return RF_NOT_FOUND;
    if(campaign_tankbot_missile_primary.fire_seconds!=4.f)return RF_FORMAT;
    now=scene_ai_tankbot_secondary_now(frame);
    if(!forced && owner->tankbot_secondary_source==source) {
        status=rf_timer_expired(owner->tankbot_secondary_deadline,now,&expired);if(status)return status;
        if(!expired)return RF_NOT_FOUND;
    }
    status=rf_timer_set(&deadline,now,4000);if(status)return status;
    epoch=scene_ai_tankbot_secondary_epoch;scene_ai_tankbot_secondary_launching=1;
    status=scene_ai_tankbot_missile_launch(owner,target);
    scene_ai_tankbot_secondary_launching=0;rf_scene_tankbot_secondary[11]=(uint32_t)status;
    if(status)return status; /* The prior deadline was never modified. */
    /* Re-resolve, never dereference a pre-callback owner. A successful load
     * or teardown cannot inherit a deadline from the replaced timeline. */
    if(epoch==scene_ai_tankbot_secondary_epoch) {
        owner=scene_ai_tankbot_secondary_identity(source_slot,source);
        if(owner){owner->tankbot_secondary_source=source;owner->tankbot_secondary_deadline=deadline;}
    }
    rf_scene_tankbot_secondary[6]+=forced;rf_scene_tankbot_secondary[7]=source;
    rf_scene_tankbot_secondary[8]=forced?UINT32_MAX:0;
    rf_scene_tankbot_secondary[9]=frame;rf_scene_tankbot_secondary[10]=(uint32_t)deadline;
    return RF_OK;
}
static uint32_t scene_ai_tankbot_secondary_live(const scene_ai_tankbot_secondary_pair *pair,
    const float player_eye[3],campaign_npc_body **source,const float **target_eye,
    const rf_physics_body **target_body)
{
    campaign_npc_body *o,*v;uint32_t k;float speed2=0;
    o=scene_ai_drone_smash_npc_identity(pair->source_slot,pair->source);
    if(!scene_ai_drone_smash_npc_live(o) || !o->combat_alert ||
       o->view.weapons[0]!=pair->primary || o->view.weapons[1]!=pair->secondary ||
       !campaign_enemy_tankbot_primary_weapon(pair->primary) ||
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
/* Before primary ammo/cadence/range exits. Success must not continue the NPC
 * loop: missile and Chaingun admission/consumption remain independent. */
static int scene_ai_tankbot_secondary_tick(scene_stream *stream,uint32_t source_slot,
    uint32_t target,uint32_t frame,const float player_eye[3],uint32_t *fired)
{
    scene_ai_tankbot_secondary_pair pair={0};campaign_npc_body *owner;
    rf_weapon_ai_secondary_input in={0},again={0};const float *eye;const rf_physics_body *body;
    const rf_weapon_acquire_definition *ammo;int32_t weapon;uint32_t i,admitted;
    uint64_t epoch;int status,pending,expired;
    if(!stream || !player_eye || !fired || !campaign_npc_bodies || source_slot>=campaign_npc_body_count)return RF_RANGE;
    *fired=0;if(rf_scene_swim_test_enabled || scene_ai_tankbot_secondary_launching || !scene_tankbot_missile_resources)return RF_OK;
    weapon=rf_weapon_name_find(&campaign_weapon_supply.names,"Tankbot Missile");
    owner=campaign_npc_bodies+source_slot;
    if(campaign_weapon_supply.names.count>64 || weapon<0 || weapon>=64 ||
       (uint32_t)weapon>=campaign_weapon_supply.names.count ||
       (uint32_t)weapon<campaign_weapon_supply.names.primary_count || owner->view.weapons[1]!=weapon ||
       owner->view.weapons[0]<0 || owner->view.weapons[0]>=64 ||
       !campaign_enemy_tankbot_primary_weapon(owner->view.weapons[0]))return RF_OK;
    ammo=campaign_weapon_supply.definitions+weapon;
    /* Installed weapons.tbl2633..2660. Use existing loaded definitions and
     * finite 36-round reserve; never fabricate a replacement rocket profile. */
    if(campaign_tankbot_missile_primary.fire_seconds!=4.f || campaign_tankbot_missile_primary.ai_attack_range!=150.f ||
       campaign_tankbot_missile.damage_radius!=3.f || ammo->magazine!=0 || ammo->capacity!=36 ||
       ammo->ammo_type<0 || ammo->ammo_type>=32 || owner->inventory.loaded[weapon]!=0)return RF_FORMAT;
    pair.source=owner->registration.handle;pair.source_slot=source_slot;
    pair.target=target;pair.target_slot=UINT32_MAX;pair.primary=owner->view.weapons[0];pair.secondary=weapon;
    pair.order=owner->combat_scripted;pair.order_target=owner->combat_target;pair.ai_action=owner->ai_mode.action_280;
    if(target!=campaign_player_object.handle) {
        for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==target){pair.target_slot=i;break;}
        if(i==campaign_npc_body_count)return RF_OK;
    }
    if(!scene_ai_tankbot_secondary_live(&pair,player_eye,&owner,&eye,&body))return RF_OK;
    ++rf_scene_tankbot_secondary[0];in.now_ms=scene_ai_tankbot_secondary_now(frame);
    in.deadline=owner->tankbot_secondary_source==pair.source?owner->tankbot_secondary_deadline:in.now_ms;
    status=rf_timer_expired(in.deadline,in.now_ms,&expired);if(status)return status;
    if(!expired){++rf_scene_tankbot_secondary[2];return RF_OK;}
    if(owner->inventory.reserve[ammo->ammo_type]<=0){++rf_scene_tankbot_secondary[5];return RF_OK;}
    status=rf_timer_pending(owner->pain.animation_lock,in.now_ms,&pending);if(status)return status;
    if(pending){++rf_scene_tankbot_secondary[4];return RF_OK;}
    /* Existing acquired/alert state is the port's readiness approximation;
     * actual current LOS below supplies visibility, with no unseen-fire flag. */
    in.weapon=weapon;in.global_enabled=1;in.ai_flags_530=1;in.target_present=1;in.visible_29c=1;
    in.maximum_range=campaign_tankbot_missile_primary.ai_attack_range;
    in.minimum_range=campaign_tankbot_missile.damage_radius;
    if(!scene_ai_drone_secondary_geometry(owner,eye,body,&in))return RF_OK;
    epoch=scene_ai_tankbot_secondary_epoch;
    status=rf_weapon_ai_secondary_admit(&in,scene_ai_drone_secondary_sight,stream,&admitted);
    if(status)return status;if(!admitted){++rf_scene_tankbot_secondary[3];return RF_OK;}
    if(epoch!=scene_ai_tankbot_secondary_epoch ||
       !scene_ai_tankbot_secondary_live(&pair,player_eye,&owner,&eye,&body))return RF_OK;
    again=in;if(!scene_ai_drone_secondary_geometry(owner,eye,body,&again) || memcmp(&in,&again,sizeof(in)))return RF_OK;
    status=rf_timer_pending(owner->pain.animation_lock,in.now_ms,&pending);if(status)return status;if(pending)return RF_OK;
    status=scene_ai_tankbot_secondary_launch(source_slot,in.target,frame,0);
    if(status==RF_NOT_FOUND){++rf_scene_tankbot_secondary[5];return RF_OK;}if(status)return status;
    *fired=1;++rf_scene_tankbot_secondary[1];++rf_scene_enemy_combat[2];
    rf_scene_tankbot_secondary[8]=pair.target;
    if(rf_scene_combat_trace)printf("TANKBOT_SECONDARY_TRACE %u %u %u %u\n",frame,pair.source,pair.target,rf_scene_tankbot_secondary[10]);
    return RF_OK;
}
#endif
