#ifndef RF_SCENE_AI_MELEE_SHIELD_INCLUDED
#define RF_SCENE_AI_MELEE_SHIELD_INCLUDED
/* Actor-directed NPC melee only. Include after both physical-shield
 * adapters and NPC pose services, before delayed melee.
 * Keep the old geometry-only helper/API intact for its existing callers.
 *
 * Original4c7edd melee uses object/model collision, including the real shield
 * clutter admitted at48c2c7. This adapter instead retains the existing finite
 * AI-range ray and directional physical-shield policy; it is NOT the original
 * swept0.5-radius collider or exact original collision ordering.
 */
typedef struct scene_ai_melee_pair {
    uint32_t source_slot,source,target_slot,target;
} scene_ai_melee_pair;
/* Read-only, no callbacks/mutation. UINT32_MAX target_slot denotes the player.
 * Full handles, registry wrappers and entity views must all agree. */
static campaign_npc_body *scene_ai_melee_npc_identity(uint32_t slot,uint32_t handle)
{
    campaign_npc_body *o;
    if(!campaign_npc_bodies || slot>=campaign_npc_body_count || !handle || handle==UINT32_MAX)return NULL;
    o=campaign_npc_bodies+slot;
    if(o->registration.handle!=handle || o->registration.view!=&o->view ||
       (uint32_t)o->view.handle!=handle || o->damage.effects.handle!=handle ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&o->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&o->view || !o->body.allocated_bytes)return NULL;
    return o;
}
static campaign_npc_body *scene_ai_melee_npc_live(uint32_t slot,uint32_t handle)
{
    campaign_npc_body *o=scene_ai_melee_npc_identity(slot,handle);
    return o && !(o->object_flags&(2u|0x4000u)) && !(o->view.flags_810&1u) &&
        isfinite(o->damage.effects.health) && o->damage.effects.health>0?o:NULL;
}
static uint32_t scene_ai_melee_player_identity(uint32_t handle)
{
    return handle && handle!=UINT32_MAX && campaign_player_object.handle==handle &&
        campaign_player_object.view==&campaign_player_view && (uint32_t)campaign_player_view.handle==handle &&
        campaign_player_damage.state.effects.handle==handle && scene_actor_body.allocated_bytes &&
        rf_object_registry_lookup(&campaign_registry,handle)==&campaign_player_object &&
        rf_entity_lookup(&campaign_entities,(int32_t)handle)==&campaign_player_view;
}
static uint32_t scene_ai_melee_pair_live(const scene_ai_melee_pair *pair,
    campaign_npc_body **source,campaign_npc_body **victim,const rf_physics_body **body)
{
    campaign_npc_body *o,*v=NULL;
    if(!pair || !source || !victim || !body || pair->source==pair->target)return 0;
    o=scene_ai_melee_npc_live(pair->source_slot,pair->source);if(!o)return 0;
    if(pair->target_slot==UINT32_MAX) {
        if(!scene_ai_melee_player_identity(pair->target) ||
           (campaign_player_view.flags_7c&(2u|0x4000u)) || (campaign_player_view.flags_810&1u) ||
           !isfinite(campaign_player_damage.state.effects.health) || campaign_player_damage.state.effects.health<=0)return 0;
        *body=&scene_actor_body;
    } else {
        v=scene_ai_melee_npc_live(pair->target_slot,pair->target);if(!v)return 0;
        *body=&v->body;
    }
    *source=o;*victim=v;return 1;
}
/* Ordinary fire captures this BEFORE presentation. Delayed callers capture at
 * the accepted due impact and additionally guard their ticket/phase/order.
 * No pointer here owns an actor; every validity check resolves full handles.
 * This retains geometry, not a newly aimed ray or a newly selected target. */
typedef struct scene_ai_melee_guard {
    scene_ai_melee_pair pair;const float *player_eye;
    rf_physics_body_state source_state,target_state;
    float source_eye[3],source_basis[9],target_eye[3],target_basis[9];
    uint32_t order,order_target,alert,once,shoot,animation,weapon_owned;
    int32_t weapon,action,source_link,target_link;
} scene_ai_melee_guard;
static uint32_t scene_ai_melee_guard_capture(const scene_ai_melee_pair *pair,
    const float player_eye[3],scene_ai_melee_guard *out)
{
    scene_ai_melee_guard value={0};campaign_npc_body *o,*v;const rf_physics_body *body;
    if(!player_eye || !out || !scene_ai_melee_pair_live(pair,&o,&v,&body))return 0;
    value.pair=*pair;value.player_eye=player_eye;value.source_state=o->body.state;value.target_state=body->state;
    memcpy(value.source_eye,o->eye_position,12);memcpy(value.source_basis,o->look.orientation,36);
    memcpy(value.target_eye,v?v->eye_position:player_eye,12);
    memcpy(value.target_basis,v?v->look.orientation:actor_look.eye_orientation,36);
    value.weapon=o->view.weapons[0];value.action=o->ai_mode.action_280;
    if(value.weapon<0 || value.weapon>=64)return 0;
    value.weapon_owned=o->inventory.owned[value.weapon];
    value.order=o->combat_scripted;value.order_target=o->combat_target;value.alert=o->combat_alert;
    value.once=o->script_once.count;value.shoot=o->script_shoot.active;value.animation=o->script_animation.active;
    value.source_link=o->view.linked_handle;value.target_link=v?v->view.linked_handle:campaign_player_view.linked_handle;
    *out=value;return 1;
}
static uint32_t scene_ai_melee_guard_valid(void *context)
{
    const scene_ai_melee_guard *g=context;campaign_npc_body *o,*v;const rf_physics_body *body;
    if(!g || !g->player_eye || !scene_ai_melee_pair_live(&g->pair,&o,&v,&body))return 0;
    return g->weapon>=0 && g->weapon<64 && o->view.weapons[0]==g->weapon &&
        o->inventory.owned[g->weapon]==g->weapon_owned && o->ai_mode.action_280==g->action &&
        o->combat_scripted==g->order && o->combat_target==g->order_target && o->combat_alert==g->alert &&
        o->script_once.count==g->once && o->script_shoot.active==g->shoot && o->script_animation.active==g->animation &&
        o->view.linked_handle==g->source_link && (v?v->view.linked_handle:campaign_player_view.linked_handle)==g->target_link &&
        !memcmp(&o->body.state,&g->source_state,sizeof(g->source_state)) &&
        !memcmp(&body->state,&g->target_state,sizeof(g->target_state)) &&
        !memcmp(o->eye_position,g->source_eye,12) && !memcmp(o->look.orientation,g->source_basis,36) &&
        !memcmp(v?v->eye_position:g->player_eye,g->target_eye,12) &&
        !memcmp(v?v->look.orientation:actor_look.eye_orientation,g->target_basis,36);
}
typedef uint32_t (*scene_ai_melee_valid_fn)(void *context);
static uint32_t scene_ai_melee_contact_valid(const scene_ai_melee_pair *pair,
    scene_ai_melee_valid_fn valid,void *context)
{
    campaign_npc_body *source,*victim;const rf_physics_body *body;
    return valid && scene_ai_melee_pair_live(pair,&source,&victim,&body) && valid(context);
}
/* Terminal cover adapter. A selected stale/protected/breaking prop consumes.
 * The first pass can reach shield commit ONLY when no cover damage occurred.
 * A rejected shield's optional body fallback therefore cannot double-damage
 * rubble or props, although it may need a second, longer cover query. */
static int scene_ai_melee_actor_cover(scene_stream *stream,const scene_ai_melee_pair *pair,
    const float start[3],const float ray[3],float fraction,float damage,int32_t kind,
    scene_ai_melee_valid_fn valid,void *context,uint32_t *consumed)
{
    rf_clutter_base_owner *prop=NULL;float prop_fraction;uint32_t prop_slot,prop_handle=UINT32_MAX,blocked;int status;
    *consumed=1;
    if(!scene_ai_melee_contact_valid(pair,valid,context))return RF_OK;
    status=campaign_clutter_firearm_select(start,ray,fraction,&prop_slot,&prop_fraction);if(status)return status;
    if(prop_slot!=UINT32_MAX) {
        if(!campaign_clutter_bodies || prop_slot>=campaign_clutter_records.count ||
           !(prop=campaign_clutter_bodies[prop_slot]) || !scene_clutter_damage_identity(&campaign_registry,prop))return RF_OK;
        prop_handle=prop->state.handle;fraction=prop_fraction;
    }
    if(!scene_ai_melee_contact_valid(pair,valid,context))return RF_OK;
    status=combat_enemy_fragment_shot(stream,start,ray,fraction,damage,&blocked);if(status)return status;
    if(blocked)return RF_OK; /* Never continue after a damaging cover action. */
    if(!scene_ai_melee_contact_valid(pair,valid,context))return RF_OK;
    if(prop) {
        if(!campaign_clutter_bodies || prop_slot>=campaign_clutter_records.count || campaign_clutter_bodies[prop_slot]!=prop ||
           rf_object_registry_lookup(&campaign_registry,prop_handle)!=(const void *)prop ||
           prop->state.handle!=prop_handle || (prop->state.flags&(2u|0x4000u)))return RF_OK;
        return campaign_clutter_firearm_damage(prop_slot,damage,kind);
    }
    *consumed=0;return RF_OK;
}
static int campaign_enemy_melee_actor_contact(scene_stream *stream,const scene_ai_melee_pair *pair,
    const float start[3],const float target_delta[3],float reach,const float player_eye[3],
    float damage,int32_t kind,scene_ai_melee_valid_fn valid,void *context,uint32_t *contact)
{
    campaign_npc_body *source,*victim;const rf_physics_body *body;
    scene_ai_shield_ray_selection npc_shield={0};scene_player_shield_candidate player_shield={0};
    float squared=0,ray[3],end[3],body_fraction=1,fraction;uint32_t i,body_hit,player_hit=0,shield_hit,consumed;int status;
    if(!stream || !pair || !start || !target_delta || !player_eye || !contact || !valid ||
       !isfinite(reach) || reach<=0 || !isfinite(damage) || damage<0 || kind<0 || kind>8)return RF_RANGE;
    *contact=0;
    if(!scene_ai_melee_pair_live(pair,&source,&victim,&body) || !valid(context))return RF_OK;
    for(i=0;i<3;i++) {
        if(!isfinite(start[i]) || !isfinite(target_delta[i]) || !isfinite(player_eye[i]))return RF_RANGE;
        squared+=target_delta[i]*target_delta[i];
    }
    if(!isfinite(squared))return RF_RANGE;if(squared<.0001f)return RF_OK;
    for(i=0;i<3;i++) {
        ray[i]=target_delta[i]*(reach/sqrtf(squared));end[i]=start[i]+ray[i];
        if(!isfinite(ray[i]) || !isfinite(end[i]))return RF_RANGE;
    }
    body_hit=combat_body(start,ray,body,1,&body_fraction);if(!body_hit)body_fraction=1;
    fraction=body_fraction;
    if(victim) {
        /* Placement reads pose->matrices, whereas exact-body ordering demand-
         * evaluates them. Prepare FIRST so selected shield and body ordering
         * use the same pose. Do not demand models for ordinary unshielded NPCs. */
        if(scene_npc_shields.owners && scene_npc_shields.weapon>=0 && scene_npc_shields.weapon<64 &&
           victim->view.weapons[0]==scene_npc_shields.weapon && victim->inventory.owned[scene_npc_shields.weapon]) {
            rf_entity_pose *pose=NULL;
            status=campaign_model_pose(pair->target_slot,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
            status=campaign_model_pose_ready(pair->target_slot,pose);if(status)return status;
        }
        if(!scene_ai_melee_contact_valid(pair,valid,context))return RF_OK;
        status=scene_ai_shield_ray_select(pair->target_slot,start,end,body_hit,body_fraction,&npc_shield);if(status)return status;
        fraction=npc_shield.fraction;
    } else {
        status=scene_player_shield_query(stream,player_eye,actor_look.eye_orientation,start,end,1,
            &player_shield,&player_hit);if(status)return status;
        if(player_hit)fraction=player_shield.hit.time;
    }
    shield_hit=player_hit || npc_shield.kind==SCENE_AI_SHIELD_RAY_SHIELD;
    status=scene_ai_melee_actor_cover(stream,pair,start,ray,fraction,damage,kind,valid,context,&consumed);
    if(status || consumed)return status;
    if(shield_hit) {
        uint32_t accepted,broken;
        rf_damage_request request={damage,pair->source,kind,0,UINT32_MAX,0};
        if(!scene_ai_melee_contact_valid(pair,valid,context))return RF_OK;
        /* No callback on this clear path between selection and commit. Player
         * prepared skinning matrices remain borrowed at their current frame;
         * NPC matrices were prepared above. The required validity callback is
         * read-only and must not step animations, query new targets or mutate.
         * Keep full endpoints and candidate.limit=1, never hit.time as limit. */
        if(player_hit)status=scene_player_shield_commit(&player_shield,&request,&accepted,&broken);
        else status=scene_ai_shield_ray_commit(&npc_shield,start,end,&request,&accepted,&broken);
        if(status)return status;
        if(accepted)return RF_OK; /* Includes breaking contact: no spillthrough. */
        /* Rejection never turns a shield-only intersection into a body hit.
         * Retain the original body/ray and re-arbitrate props/world/rubble over
         * that full body limit. No per-impact retarget or second shield query. */
        if(!body_hit || !scene_ai_melee_contact_valid(pair,valid,context))return RF_OK;
        status=scene_ai_melee_actor_cover(stream,pair,start,ray,body_fraction,damage,kind,valid,context,&consumed);
        if(status || consumed)return status;
    }
    if(scene_ai_melee_contact_valid(pair,valid,context))*contact=body_hit;
    return RF_OK;
}
#endif
