#ifndef RF_SCENE_VEHICLE_SCRIPT_SLAY_INCLUDED
#define RF_SCENE_VEHICLE_SCRIPT_SLAY_INCLUDED
/* Include after scene_driller_runtime.inc. The early campaign_slay_object
 * caller forward-declares this signature and returns status when handled.
 * This extends the existing first-pass forced direct Slay damage policy to
 * actual vehicle owners. It does not produce a blast or damage occupants. */
static int scene_vehicle_script_slay(uint32_t handle,uint32_t source,int32_t now,
    uint32_t *handled)
{
    scene_driller_damage_runtime *runtime=scene_driller_damage_owner;
    scene_passive_vehicle *passive=NULL;
    scene_driller_damage *damage=NULL;
    uint32_t uid=UINT32_MAX,was_destroyed,i,clock;float amount=0,seconds;
    rf_damage_request request;int status;
    if(!handled)return RF_RANGE;
    *handled=0;
    if(scene_driller_damage_runtime_valid(runtime) &&
       runtime->damage.state.effects.handle==handle && runtime->entry->objects &&
       rf_object_registry_lookup(runtime->entry->objects,handle)==&runtime->entry->registration) {
        damage=&runtime->damage;
        if(campaign_authored_vehicle_uid && campaign_authored_vehicle_handle==handle)
            uid=(uint32_t)campaign_authored_vehicle_uid;
    } else {
        runtime=NULL;
        for(i=0;i<campaign_passive_vehicle_count;i++) {
            scene_passive_vehicle *candidate=campaign_passive_vehicles+i;
            if(candidate->handle!=handle || candidate->damage.state.effects.handle!=handle ||
               rf_object_registry_lookup(&campaign_registry,handle)!=candidate)continue;
            passive=candidate;damage=&passive->damage;uid=passive->uid;break;
        }
    }
    if(!damage)return RF_OK;
    *handled=1;
    /* A completed wreck is idempotent. An interrupted destroy callback still
     * belongs to its original damage owner and must be allowed to retry. */
    if(damage->destroyed && !damage->destruction_pending)return RF_OK;
    was_destroyed=damage->destroyed;
    ++rf_scene_script_slays[0];rf_scene_script_slays[2]=uid;
    rf_scene_script_slays[4]=(uint32_t)now;
    if(now<0 || !isfinite(damage->state.effects.health) || !isfinite(damage->state.effects.armor)) {
        status=RF_FORMAT;
    } else {
        request=(rf_damage_request){damage->state.effects.health+fmaxf(0,damage->state.effects.armor)+1,
            source,-1,0,UINT32_MAX,1};
        if(runtime) {
            /* Reuse the active host's frame-based feedback and deferred exit
             * publication. Event milliseconds quantize down by at most16ms. */
            uint32_t frame=(uint32_t)((uint64_t)(uint32_t)now*60/1000);
            status=scene_driller_damage_runtime_receive(runtime,handle,&request,frame,&amount);
        } else {
            combat_feedback feedback={now,0};
            rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
                combat_random,combat_notify,combat_playing,combat_play,&feedback};
            seconds=(float)now*.001f;memcpy(&clock,&seconds,4);
            status=scene_driller_damage_receive(damage,&request,1,clock,0,&effects,
                scene_passive_vehicle_destroy,passive,&amount);
            if(!status)status=feedback.status;
            /* Retain the same passive damage observability as real hits. */
            rf_scene_passive_damage[6]=(uint32_t)status;
            if(!status) {
                ++rf_scene_passive_damage[2];
                rf_scene_passive_damage[3]+=!was_destroyed&&damage->destroyed;
                rf_scene_passive_damage[4]=uid;
                memcpy(rf_scene_passive_damage+5,&damage->state.effects.health,4);
            }
        }
    }
    /* Damage can commit before a later effect failure. Record the real state,
     * and count its one live-to-destroyed transition even on that error path. */
    rf_scene_script_slays[1]+=!was_destroyed&&damage->destroyed;
    memcpy(rf_scene_script_slays+3,&damage->state.effects.health,4);
    rf_scene_script_slays[5]=(uint32_t)status;
    return status;
}
#endif
