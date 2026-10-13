#ifndef RF_SCENE_CROUCH_RUNTIME_MOTION_INCLUDED
#define RF_SCENE_CROUCH_RUNTIME_MOTION_INCLUDED
/* Bounded ordinary consumer. One deterministic actor slot per simulation
 * frame receives stance visibility/clearance work. No cached Blind age is
 * used as visibility. Other owners keep their existing motion priority. */
typedef struct scene_crouch_physical_context {scene_stream *stream;uint32_t frame;} scene_crouch_physical_context;
static int scene_crouch_refresh_ground(void *context,uint32_t handle)
{scene_crouch_physical_context *c=context;return campaign_scripted_speed_ground(c->stream,handle);}
static uint32_t scene_crouch_clock_bits(void *context)
{
    scene_crouch_physical_context *c=context;
    float seconds=(float)((double)c->frame*scene_step_seconds);uint32_t bits;
    memcpy(&bits,&seconds,sizeof(bits));return bits;
}
static uint32_t scene_crouch_motion_available(const campaign_npc_body *o)
{
    return o&&o->registration.view&&o->damage.effects.health>0&&!(o->view.flags_810&1u)&&
        !(o->object_flags&(2u|8u|0x4000u))&&!campaign_npc_ai_suppressed(o)&&
        !o->scripted_physics&&!o->script_animation.active&&o->ai_mode.action_280!=1&&!o->flight.entered&&
        !scene_guard_retreat_active(o)&&!o->guard_retreat_fault&&!o->alarm.present&&!o->alarm_fault&&
        !o->medic.pending&&!scene_noise_active(o)&&!campaign_cutscene_runtime.active&&
        !campaign_endgame.phase&&!scene_blind_paid(o->registration.handle)&&
        !scene_npc_seat_attached(o->registration.handle);
}
static int scene_crouch_runtime_motion(scene_stream *stream,uint32_t slot,uint32_t frame)
{
    campaign_npc_body *o;rf_entity_pose *pose;scene_crouch_fire_guard guard;
    rf_motion_priority priority={0};rf_motion_crouch_input input={0};
    scene_crouch_motion_candidate candidate;scene_crouch_query_geometry ray;
    rf_entity_motion_catalog catalog;rf_entity_playback_resources playback;
    rf_entity_model_motions motions;rf_entity_playback_model model;uint32_t skeleton;
    scene_crouch_physical_context context={stream,frame};
    rf_scene_npc_crouch_services crouch_services={scene_crouch_refresh_ground,scene_crouch_clock_bits,&context};
    rf_scene_npc_stance_services stand_services={NULL,campaign_scripted_speed_ground,stream};
    uint32_t cls,target=0,blocked=0,physical,handle;float target_eye[3];
    uint64_t epoch;campaign_npc_body *owners;int status,checked,ready,combat_eligible,stood;
    if(!stream||!stream->collision||!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+slot;
    if(!(o->crouch.flags&SCENE_CROUCH_PRESENT)||!scene_crouch_motion_available(o))return RF_OK;
    if(frame%campaign_npc_body_count!=slot)return RF_OK;
    status=scene_crouch_live_shape(slot,o);if(status)return status;
    status=scene_crouch_owner_guard_capture(slot,&guard);if(status)return status;
    pose=campaign_poses.items+slot;cls=campaign_seeds.items[slot].class_index;
    if(!campaign_npc_eyes||cls>=campaign_seeds.class_count)return RF_FORMAT;
    skeleton=pose->skeleton;catalog=campaign_motion_catalog;playback=campaign_playback_resources;
    if(!catalog.models||!playback.models||skeleton>=catalog.model_count||skeleton>=playback.model_count)return RF_FORMAT;
    motions=catalog.models[skeleton];model=playback.models[skeleton];
    handle=o->registration.handle;physical=!!(o->view.flags_810&0x400u);
    owners=campaign_npc_bodies;epoch=scene_robot_death_epoch;
    if(o->movement_slot>=16)return RF_FORMAT;
    if(o->view.action_520==3&&scene_tactical_service_allowed(o)&&
       o->view.weapons[0]!=campaign_grenade_id){
        priority.forced_state=o->ai_override.word_834;priority.flags=o->view.flags_810;
        priority.physics_flags=o->body.state.flags;priority.mode=(int32_t)campaign_modes[o->movement_slot].index;
        priority.class_type=o->view.class_type;priority.action=3;
        priority.entity_handle=(int32_t)handle;priority.linked_occupant_handle=-1;
        memcpy(priority.velocity,o->body.state.velocity,12);
        input.motion_9=o->selection.mapping.states[9];input.weapon=o->view.weapons[0];
        input.excluded_weapons[0]=campaign_grenade_id;input.excluded_weapons[1]=campaign_riot_id;
        input.mode=priority.mode;input.action=3;input.stance_7bc=o->unholster.stance_7bc;
        input.behavior_554=o->ai_mode.state_2b4;input.flags_7d0=o->view.flags_7d0;
        /* This ordinary portable owner has no independent740 override;
         * retain its constructor-zero domain. Scripted action12 is excluded. */
        target=scene_blind_target_handle(o);input.target_present=target&&scene_blind_target_live(target);
        status=rf_entity_combat_predicates(&campaign_entities,&o->view,NULL,0,&ready,&combat_eligible);if(status)return status;
        status=scene_crouch_motion_prepare(&pose->controller,o->selection.mapping.states,&priority,&input,
            !!combat_eligible,0,0,&candidate);if(status)return status;
        if(candidate.query_required){
            if(target==campaign_player_object.handle){
                status=scene_medic_player_eye(frame,target_eye);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
            }else{
                uint32_t target_slot;if(!scene_blind_target_slot(target,&target_slot))return RF_OK;
                if(campaign_npc_bodies[target_slot].object_flags&(2u|0x4000u))return RF_OK;
                memcpy(target_eye,campaign_npc_bodies[target_slot].eye_position,sizeof(target_eye));
            }
            /* The portable class constructor builds this cache and both eye
             * offsets from the same retained class/model, with no alternate
             * render-class pointer. Do not extend that binding implicitly. */
            status=scene_crouch_query_prepare(o->published,campaign_model_owners[slot].basis,
                campaign_npc_eyes[cls].offsets+3,target_eye,&ray);if(status)return status;
            status=rf_geometry_collision_ray(stream->collision,&campaign_movers,ray.start,ray.end,0x27,NULL,&blocked);
            checked=scene_crouch_fire_guard_valid(&guard);if(checked)return checked;
            if(status)return status;
            if(scene_blind_target_handle(o)!=target||!scene_blind_target_live(target)||
               !scene_crouch_motion_available(o)||!scene_tactical_service_allowed(o))return RF_OK;
            status=scene_crouch_motion_prepare(&pose->controller,o->selection.mapping.states,&priority,&input,
                !!combat_eligible,1,!!blocked,&candidate);if(status)return status;
        }
        if(!candidate.priority_handled&&!candidate.stance.handled){
            int32_t desired=o->script_move.active?2:0;
            if(o->selection.mapping.states[desired]<0)desired=0;
            if(!rf_motion_has_state(&candidate.controller,desired)){
                status=rf_motion_request_state(&candidate.controller,o->selection.mapping.states,desired,.25f);
                if(status)return status;
            }
        }
        /* Reserve room for both pending endpoints before either demand.
         * Otherwise loading the second zero-reference clip could evict the
         * first before controller publication. Resource pressure defers this
         * optional stance transition without changing existing playback. */
        {
            uint64_t missing=0;uint32_t caches[2],count=0,slots=pose->playback.completion.active.count;
            int32_t prior_id=-1;
            if(slots>16||!model.cache_ids||!motions.items||!campaign_npc_motion_data||!campaign_npc_motion_sizes)return RF_FORMAT;
            for(uint32_t i=0;i<2;i++){
                int32_t state=i?candidate.controller.next:candidate.controller.current,id;uint32_t cache,j;
                if(state==-1)continue;if(state<0||state>=23)return RF_FORMAT;
                id=o->selection.mapping.states[state];if(id<0)continue;
                if((uint32_t)id>=model.count||(uint32_t)id>=motions.count)return RF_FORMAT;
                cache=model.cache_ids[id];if(cache>=campaign_npc_motion_count)return RF_FORMAT;
                for(j=0;j<count;j++)if(caches[j]==cache)break;
                if(j==count){caches[count++]=cache;if(!campaign_npc_motion_data[cache])missing+=motions.items[id].file.entry.size;}
                for(j=0;j<pose->playback.completion.active.count;j++)
                    if(pose->playback.completion.active.slots[j].motion==id)break;
                if(j==pose->playback.completion.active.count&&id!=prior_id)++slots;
                prior_id=id;
            }
            if(slots>16||(uint64_t)campaign_npc_motion_bytes+missing>SCENE_NPC_MOTION_BUDGET)return RF_OK;
        }
        /* Demand the prepared endpoints before publishing controller bytes.
         * Missing optional mappings retain the core selector's fallback. */
        for(uint32_t i=0;i<2;i++){
            int32_t state=i?candidate.controller.next:candidate.controller.current,id;
            if(state==-1)continue;if(state<0||state>=23)return RF_FORMAT;
            id=o->selection.mapping.states[state];if(id<0)continue;
            status=campaign_npc_motion_require(skeleton,(uint32_t)id);
            checked=scene_crouch_fire_guard_valid(&guard);if(checked)return checked;
            if(memcmp(&campaign_motion_catalog,&catalog,sizeof(catalog))||
               memcmp(&campaign_playback_resources,&playback,sizeof(playback)))return RF_FORMAT;
            if(memcmp(catalog.models+skeleton,&motions,sizeof(motions))||
               memcmp(playback.models+skeleton,&model,sizeof(model)))return RF_FORMAT;
            if(status)return status;
            if(!scene_crouch_motion_available(o)||!scene_tactical_service_allowed(o)||
               scene_blind_target_handle(o)!=target)return RF_OK;
        }
        pose->controller=candidate.controller;
        if(candidate.priority_handled||candidate.stance.effect==RF_MOTION_STANCE_NONE)return RF_OK;
        if(candidate.stance.effect==RF_MOTION_STANCE_CROUCH){
            status=rf_scene_npc_crouch(handle,&crouch_services);
            if(epoch!=scene_robot_death_epoch||owners!=campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_NOT_FOUND;
            o=campaign_npc_bodies+slot;
            if(o->registration.handle!=handle||scene_npc_reload_identity(slot,o))return RF_NOT_FOUND;
            if(status)return status;
            o->crouch.flags|=SCENE_CROUCH_STAMP;o->crouch.stamp_frame=frame;
            return RF_OK;
        }
    }else if(!physical)return RF_OK;
    /* Intent ending does not clear actual crouch when the ceiling blocks.
     * Stronger owners were excluded above; this does not cancel their clips. */
    status=rf_scene_npc_try_stand(stream->collision,handle,&stand_services,&stood);
    if(epoch!=scene_robot_death_epoch||owners!=campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_NOT_FOUND;
    o=campaign_npc_bodies+slot;
    if(o->registration.handle!=handle||scene_npc_reload_identity(slot,o))return RF_NOT_FOUND;
    return status;
}
#endif
