#ifndef RF_SCENE_PLAYER_IMPACT_GAMEPLAY_INC
#define RF_SCENE_PLAYER_IMPACT_GAMEPLAY_INC
/* Include after combat_notify/play callbacks (beside liquid damage), with
 * forward declarations before actor_tick. No position-delta damage inference.
 * Begin ONLY around the live local-player actor_tick. Pass relocated1 on
 * spawn/respawn/teleport/checkpoint/snap frames; support and camera updates do
 * not record contacts. End after committing scene_actor_body.state. */
#include "scene_player_impact.inc"
static scene_player_impact campaign_player_impact;
static uint32_t campaign_player_impact_skip;
/* recorded,eligible flushes,suppressed,damage calls,applied bits,health bits,
 * feedback requests,status */
uint32_t rf_scene_player_impact[8];
typedef struct scene_player_impact_context {
    combat_feedback feedback;
    uint32_t clock_bits;
} scene_player_impact_context;

static int scene_player_impact_owner(const rf_physics_body_state *body,
    uint32_t mode,int32_t material,rf_entity_impact_actor *actor)
{
    if(!body || !actor)return RF_RANGE;
    if(!campaign_player_object.view ||
       rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)!=&campaign_player_view)
        return RF_NOT_FOUND;
    memset(actor,0,sizeof(*actor));actor->handle=campaign_player_object.handle;
    actor->object_flags=campaign_player_view.flags_7c;actor->movement_mode=mode;
    actor->use_kind=campaign_force_class_kind;actor->contact_material=material;
    actor->support_material=(int32_t)rf_scene_actor_ground_material;
    actor->health=campaign_player_damage.state.effects.health;
    memcpy(actor->position,body->position,12);memcpy(actor->published,body->position,12);
    return RF_OK;
}
static void scene_player_impact_frame_begin(uint32_t frame,uint32_t relocated)
{
    uint32_t enabled=frame && !relocated && !campaign_player_impact_skip && campaign_spawn && campaign_player_object.view &&
        campaign_player_damage.state.effects.health>0 && campaign_player_view.linked_handle==-1;
    if(!frame)memset(rf_scene_player_impact,0,sizeof(rf_scene_player_impact));
    campaign_player_impact_skip=0;
    scene_player_impact_begin(&campaign_player_impact,frame,enabled);
}
static void scene_player_impact_cancel(void)
{campaign_player_impact.enabled=campaign_player_impact.ready=0;}
static void scene_player_impact_relocated(void)
{scene_player_impact_cancel();campaign_player_impact_skip=1;}
/* Call immediately after successful real actor_contact, passing its returned
 * impact speed and the actual sweep collision material/pre-landing mode. */
static int scene_player_impact_contact(const rf_physics_body_state *body,
    uint32_t mode,int32_t material,float speed)
{
    rf_entity_impact_actor actor;int status;
    if(!campaign_player_impact.enabled)return RF_OK;
    status=scene_player_impact_owner(body,mode,material,&actor);if(status)return status;
    ++rf_scene_player_impact[0];
    return scene_player_impact_record(&campaign_player_impact,campaign_player_impact.frame,1,&actor,speed);
}
static int scene_player_impact_force_gate(void *context,rf_entity_impact_actor *actor,uint32_t *suppressed)
{
    int status;(void)context;
    status=rf_physics_force_suppresses_damage(campaign_forces.items,campaign_forces.count,actor->published,suppressed);
    if(!status && (*suppressed&255u))++rf_scene_player_impact[2];return status;
}
static int scene_player_impact_apply(void *opaque,rf_entity_impact_actor *actor,const rf_damage_request *request)
{
    scene_player_impact_context *context=opaque;float applied=0;int status;
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&context->feedback};
    ++rf_scene_player_impact[3];
    status=rf_scene_player_damage_audio(actor->handle,request,1,context->clock_bits,
        context->feedback.now,&combat_pain_random,&effects,&applied);
    actor->health=campaign_player_damage.state.effects.health;actor->object_flags=campaign_player_view.flags_7c;
    memcpy(rf_scene_player_impact+4,&applied,4);memcpy(rf_scene_player_impact+5,&actor->health,4);
    return status?status:context->feedback.status;
}
static int scene_player_impact_lethal_audio(void *context,rf_entity_impact_actor *actor)
{
    /* The shared damage_audio path already dispatches the authored player
     * pain/death sound. Do not play the same death twice on this callback. */
    (void)context;(void)actor;return RF_OK;
}
static int scene_player_impact_camera(void *opaque,rf_entity_impact_actor *actor,float amount)
{
    scene_player_impact_context *context=opaque;
    if(actor->health<=0)return RF_OK;
    ++rf_scene_player_impact[6];
    /* Bounded first-pass camera kick; exact original scaling remains polish. */
    return rf_scene_player_feedback(actor->handle,fminf(.2f,amount*.001f),.15f,context->feedback.now);
}
static int scene_player_impact_frame_end(uint32_t frame,int32_t now)
{
    rf_entity_impact_actor actor;scene_player_impact_context context={{now,0},0};int status;
    float seconds=(float)((double)(frame+1)*scene_step_seconds);
    rf_entity_impact_backend backend={scene_player_impact_force_gate,scene_player_impact_apply,
        scene_player_impact_lethal_audio,scene_player_impact_camera,&context};
    if(!campaign_player_impact.enabled)return RF_OK;
    status=scene_player_impact_owner(&scene_actor_body.state,rf_scene_actor_landing[1],campaign_player_contact.material,&actor);
    if(!status){
        memcpy(&context.clock_bits,&seconds,4);
        if(campaign_player_impact.ready)++rf_scene_player_impact[1];
        status=scene_player_impact_flush(&campaign_player_impact,frame,&actor,&backend);
    }
    scene_player_impact_cancel();rf_scene_player_impact[7]=(uint32_t)status;return status;
}
#endif
