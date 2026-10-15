#ifndef RF_SCENE_PURSUIT_RECOVERY_INCLUDED
#define RF_SCENE_PURSUIT_RECOVERY_INCLUDED
/* UNCALLED ordinary standalone recovery adapter proposal. Include only after
 * the existing tactical owner, pursuit return-owner and crouch guard definitions. This cannot replace
 * generic linked-actor recovery. No live return dispatcher calls this file.
 * All direct stop/start operations below are callback-free; resource demand and
 * external audio are guarded before any borrowed owner/pose reload or publish.
 */
/* Diagnostic outcome is deliberately returned even on error. It grants no
 * actor-write permission. In particular RF_NOT_FOUND alone cannot distinguish
 * resource refusal after STOP from stale ownership or untouched admission. */
typedef struct scene_pursuit_recovery_result {
    uint32_t effects_started,owner_valid,completed;
} scene_pursuit_recovery_result;
typedef struct scene_pursuit_recovery_context {
    campaign_unholster_context recovery;
    scene_crouch_fire_guard owner;
    rf_entity_seeds seeds;
    rf_entity_playback_resources resources;
    rf_entity_playback_model model;
    rf_motion_playback_resource clip;uint32_t clip_present;
    float class_delay;
    rf_entity_ai_transition_state ai;
    scene_tactical_receipt tactical;
    uint32_t target,mode,alert,firing_flags,resolved_target;
    int32_t secondary,linked,ai_timer,deadline,death_action;
    float health;uint32_t effects_started;
    /* Storage belongs to the guarded actor's executing return receipt.
     * The caller guarantees that lifetime; never point at a disposable copy. */
    const scene_pursuit_return_receipt *execution;
    scene_pursuit_return_receipt queued;
} scene_pursuit_recovery_context;
_Static_assert(sizeof(void*)!=4||sizeof(scene_pursuit_recovery_context)<=2048,
    "Pursuit recovery context exceeds proposed Xbox32 stack allowance");
static int scene_pursuit_recovery_guard(scene_pursuit_recovery_context *c)
{
    campaign_unholster_context *u=&c->recovery;campaign_npc_body *o;
    uint32_t resolved_target,melee_only;int status;
    /* Owning static headers and epoch are checked before retained dereference. */
    if(c->owner.epoch!=scene_robot_death_epoch||
       memcmp(&c->seeds,&campaign_seeds,sizeof(c->seeds))||
       memcmp(&c->resources,&campaign_playback_resources,sizeof(c->resources)))return RF_NOT_FOUND;
    status=scene_crouch_fire_guard_valid(&c->owner);if(status)return status;
    if(!scene_pursuit_execution_active||scene_pursuit_execution_fault)return RF_NOT_FOUND;
    if(c->execution!=&campaign_npc_bodies[u->slot].pursuit_return)return RF_NOT_FOUND;
    status=scene_pursuit_return_execution_matches(c->execution,&c->queued);
    if(status)return status;
    status=scene_pursuit_return_owner(u->slot,&resolved_target,&melee_only);
    if(status)return status;
    if(resolved_target!=c->resolved_target)return RF_NOT_FOUND;
    o=campaign_npc_bodies+u->slot;
    if(o!=u->owner||u->pose!=campaign_poses.items+u->slot||
       u->cls>=campaign_seeds.class_count||
       campaign_seeds.items[u->slot].class_index!=u->cls||
       u->pose->skeleton>=campaign_playback_resources.model_count||
       memcmp(&c->model,campaign_playback_resources.models+u->pose->skeleton,sizeof(c->model))||
       memcmp(&c->ai,&o->ai_mode,sizeof(c->ai))||
       memcmp(&c->tactical,&o->tactical,sizeof(c->tactical))||
       o->combat_target!=c->target||o->combat_scripted!=c->mode||o->combat_alert!=c->alert||
       o->firing.flags_7d0!=c->firing_flags||
       o->view.weapons[1]!=c->secondary||o->view.linked_handle!=c->linked||
       o->pain.ai_timer!=c->ai_timer||o->unholster.deadline_518!=c->deadline||
       o->death.action_824!=c->death_action||o->damage.effects.health!=c->health||
       memcmp(&c->class_delay,&campaign_seeds.classes[u->cls].unholster_delay,sizeof(c->class_delay)))return RF_NOT_FOUND;
    if(c->clip_present&&(!c->model.resources||
       o->selection.mapping.actions[40]<0||
       (uint32_t)o->selection.mapping.actions[40]>=c->model.count||
       memcmp(&c->clip,c->model.resources+o->selection.mapping.actions[40],sizeof(c->clip))))return RF_NOT_FOUND;
    return RF_OK;
}
/* Call only at entry after fresh resolution, or after our own callback-free
 * mutations. Never refresh this snapshot to accept a callback's replacement. */
static int scene_pursuit_recovery_snapshot(scene_pursuit_recovery_context *c)
{
    campaign_unholster_context *u=&c->recovery;campaign_npc_body *o=u->owner;
    uint32_t resolved_target,melee_only;int status;
    status=scene_crouch_owner_guard_capture(u->slot,&c->owner);if(status)return status;
    if(!scene_pursuit_execution_active||scene_pursuit_execution_fault)return RF_NOT_FOUND;
    if(c->execution!=&campaign_npc_bodies[u->slot].pursuit_return)return RF_NOT_FOUND;
    status=scene_pursuit_return_execution_matches(c->execution,&c->queued);
    if(status)return status;
    status=scene_pursuit_return_owner(u->slot,&resolved_target,&melee_only);
    if(status)return status;
    c->resolved_target=resolved_target;
    if(!campaign_seeds.classes||u->cls>=campaign_seeds.class_count||
       !campaign_playback_resources.models||u->pose->skeleton>=campaign_playback_resources.model_count)return RF_NOT_FOUND;
    /* Notice is one original source bit mirrored in three port owners.
     * Reject divergence before any callback rather than selecting a winner. */
    if(((o->ai_mode.flags_530^o->view.flags_7d0)|
        (o->ai_mode.flags_530^o->firing.flags_7d0))&0x20000u)return RF_FORMAT;
    c->firing_flags=o->firing.flags_7d0;
    c->seeds=campaign_seeds;c->resources=campaign_playback_resources;
    c->model=campaign_playback_resources.models[u->pose->skeleton];
    c->class_delay=campaign_seeds.classes[u->cls].unholster_delay;
    c->clip_present=o->selection.mapping.actions[40]>=0;
    if(c->clip_present){
        if(!c->model.resources||(uint32_t)o->selection.mapping.actions[40]>=c->model.count)return RF_RANGE;
        c->clip=c->model.resources[o->selection.mapping.actions[40]];
    }
    c->ai=o->ai_mode;c->tactical=o->tactical;c->target=o->combat_target;
    c->mode=o->combat_scripted;c->alert=o->combat_alert;
    c->secondary=o->view.weapons[1];c->linked=o->view.linked_handle;
    c->ai_timer=o->pain.ai_timer;c->deadline=o->unholster.deadline_518;
    c->death_action=o->death.action_824;c->health=o->damage.effects.health;
    return RF_OK;
}
static int scene_pursuit_recovery_actor(void *context,uint32_t handle,
    rf_entity_ai_recovery **out)
{
    scene_pursuit_recovery_context *c=context;int status;
    if(!out||handle!=c->recovery.handle)return RF_RANGE;
    status=scene_pursuit_recovery_guard(c);if(status)return status;
    *out=&c->recovery.state;return RF_OK;
}
static int scene_pursuit_recovery_health(void *context,uint32_t handle,
    uint32_t *found,float *health)
{
    (void)context;(void)handle;(void)found;(void)health;
    /* Action13 is excluded at entry; never invent a linked health response. */
    return RF_NOT_FOUND;
}
static int scene_pursuit_recovery_playback(void *context,
    rf_entity_ai_recovery *actor,uint32_t op,double *seconds)
{
    scene_pursuit_recovery_context *c=context;campaign_unholster_context *u=&c->recovery;
    rf_entity_playback_model *model;int status,guard_status;int32_t sounds[45],sound;
    if(actor!=&u->state||!seconds)return RF_RANGE;
    status=scene_pursuit_recovery_guard(c);if(status)return status;
    /* Mark BEFORE the first publication, including later failing starts.
     * Never reset this marker when accepting our own callback-free changes. */
    c->effects_started=1;campaign_unholster_publish(u);
    status=scene_pursuit_recovery_snapshot(c);if(status)return status;
    if(op==RF_AI_RECOVERY_STOP){
        status=rf_scene_model_stop_nonlooping(u->slot);if(status)return status;
    }else if(op==RF_AI_RECOVERY_START){
        if(actor->motion_cd4<0)return RF_NOT_FOUND;
        status=campaign_npc_motion_require(u->pose->skeleton,(uint32_t)actor->motion_cd4);
        guard_status=scene_pursuit_recovery_guard(c);if(guard_status)return guard_status;
        if(status)return status;
        model=campaign_playback_resources.models+u->pose->skeleton;
        for(uint32_t i=0;i<45;i++){
            const char *label=u->owner->selection.action_sounds[i];
            if(!label)return RF_FORMAT;sounds[i]=label[0]?(int32_t)i:-1;
        }
        u->owner->death.action_824=40;
        status=rf_motion_start_action(&u->pose->playback,model->resources,model->count,
            u->owner->selection.mapping.actions,sounds,40,1,0,1,&sound);
        if(status)return status;
        status=scene_pursuit_recovery_snapshot(c);if(status)return status;
        if(sound>=0){
            if(sound>=45)return RF_FORMAT;
            status=u->sound?u->sound(u->sound_context,u->handle,
                u->owner->selection.action_sounds[sound]):RF_NOT_FOUND;
            guard_status=scene_pursuit_recovery_guard(c);if(guard_status)return guard_status;
            if(status)return status;
        }
    }else if(op==RF_AI_RECOVERY_DURATION){
        model=campaign_playback_resources.models+u->pose->skeleton;
        if(!model->resources||actor->motion_cd4<0||
           (uint32_t)actor->motion_cd4>=model->count)return RF_RANGE;
        *seconds=rf_motion_duration(model->resources[actor->motion_cd4].comparison.start_tick,
            model->resources[actor->motion_cd4].comparison.end_tick);
    }else return RF_RANGE;
    /* No external callback can occur between the guarded boundary and reload. */
    campaign_unholster_reload(u);
    return scene_pursuit_recovery_snapshot(c);
}
static int scene_pursuit_recover_unholster(uint32_t handle,int32_t now,
    int (*sound)(void *,uint32_t,const char *),void *sound_context,
    scene_pursuit_recovery_result *result,
    const scene_pursuit_return_receipt *execution,
    const scene_pursuit_return_receipt *queued)
{
    scene_pursuit_recovery_context c={0};campaign_unholster_context *u=&c.recovery;
    rf_entity_ai_recovery_backend b={scene_pursuit_recovery_actor,
        scene_pursuit_recovery_health,scene_pursuit_recovery_playback,&c};
    int status,guard_status;
    if(!result)return RF_RANGE;
    *result=(scene_pursuit_recovery_result){0};
    if(!scene_pursuit_execution_active||scene_pursuit_execution_fault)return RF_NOT_FOUND;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    status=campaign_npc_motion_owner(handle,&u->owner,&u->cls,&u->pose);if(status)return status;
    if(execution!=&u->owner->pursuit_return)return RF_NOT_FOUND;
    status=scene_pursuit_return_execution_matches(execution,queued);if(status)return status;
    c.execution=execution;c.queued=*queued;
    if(u->owner->view.action_520==13)return RF_NOT_FOUND;
    u->handle=handle;u->slot=(uint32_t)(u->owner-campaign_npc_bodies);
    u->sound=sound;u->sound_context=sound_context;
    status=scene_pursuit_recovery_snapshot(&c);if(status)return status;
    campaign_unholster_reload(u);
    status=rf_entity_ai_recover(&u->state,now,&b);
    result->effects_started=c.effects_started;
    guard_status=scene_pursuit_recovery_guard(&c);if(guard_status)return guard_status;
    result->owner_valid=1;
    /* Source recovery retains a completed first timer even if setting the
     * second timer fails. Publish that state only after the guard succeeds;
     * identity/ownership rejection above never writes through stale pointers. */
    result->effects_started=1;
    campaign_unholster_publish(u);result->completed=status==RF_OK;return status;
}
#endif
