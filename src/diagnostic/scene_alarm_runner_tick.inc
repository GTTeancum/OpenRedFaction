#ifndef RF_SCENE_ALARM_RUNNER_TICK_INCLUDED
#define RF_SCENE_ALARM_RUNNER_TICK_INCLUDED
/* Only an actual cursor crossing can authorize the activation. A removed,
 * superseded or zero-weight clip is cancellation, never completion. Value2 is
 * a transient natural-end receipt; RFNC31 only stores an in-progress value1. */
static int scene_alarm_runner_clip_clock(campaign_npc_body *o,rf_entity_pose *pose,
    rf_entity_playback_model *model,float elapsed,uint32_t culled,uint32_t *completed)
{
    rf_motion_playback_state next;rf_motion_playback_resource *r;uint32_t i;
    int32_t delta,refs;int64_t tick;int status;*completed=0;
    if(!o->alarm.present||o->alarm.phase!=3||o->alarm.started!=1)return RF_OK;
    if(!o->script_animation.active||o->script_animation.motion!=o->selection.mapping.actions[44])return RF_OK;
    if(o->script_animation.motion<0||(uint32_t)o->script_animation.motion>=model->count||
       !model->resources||pose->playback.completion.active.count>16)return RF_FORMAT;
    next=pose->playback;r=model->resources+o->script_animation.motion;
    if(next.completion.frozen||r->looping)return RF_FORMAT;
    for(i=0;i<next.completion.active.count;i++)if(next.completion.active.slots[i].motion==o->script_animation.motion)break;
    if(i==next.completion.active.count||next.completion.active.slots[i].weight<=0)return RF_OK;
    status=rf_motion_elapsed_ticks(elapsed,&delta);if(status)return status;
    tick=(int64_t)next.completion.active.slots[i].tick+delta;if(tick>INT32_MAX)return RF_RANGE;
    *completed=tick>=r->comparison.end_tick;
    if(!culled)return RF_OK;
    next.completion.active.slots[i].tick=(int32_t)tick;
    if(*completed){
        if(next.completion.active.primary_slot==(int32_t)i){next.completion.active.primary_slot=-1;
            next.completion.primary_flag=0;memset(next.completion.primary_words,0,sizeof(next.completion.primary_words));
            memset(next.completion.primary_vectors,0,sizeof(next.completion.primary_vectors));}
        refs=r->references;status=rf_motion_remove_slot(&next.completion.active,o->script_animation.motion,&refs);if(status)return status;
        r->references=refs;
    }
    next.generation=(next.generation+1)&65535u;pose->playback=next;return RF_OK;
}
static int scene_alarm_runner_tick(scene_stream *stream,campaign_npc_body *o,uint32_t slot,
    float elapsed,uint32_t frame,scene_npc_landing_receipt *landing,uint32_t *owned)
{
    const rf_level_event *event;rf_runtime_event *runtime;rf_entity_pose *pose;
    rf_entity_playback_model *model;uint32_t uid,handle;uint64_t epoch;int status,pending;int32_t now=scene_medic_now(frame);
    float target[3],x,z,length,forward_length,dot,turn;
    if(!owned||!stream||!o||slot>=campaign_npc_body_count||o!=campaign_npc_bodies+slot)return RF_RANGE;
    *owned=1;if(o->alarm_fault)return RF_FORMAT;if(!o->alarm.present){*owned=0;return RF_OK;}
    if(!scene_medic_live(o)||o->damage.effects.health<=0||(o->view.flags_810&1u)||
       (o->object_flags&(2u|0x4000u))||scene_npc_seat_attached(o->registration.handle)||
       campaign_npc_physics_suspended(o)||campaign_npc_ai_suppressed(o)||o->controller_handle||
       o->ai_mode.action_280!=16||o->view.action_520!=16||campaign_cutscene_runtime.active)
        return scene_alarm_runner_cancel(o,now,o->ai_mode.action_280!=16);
    status=scene_alarm_runner_authored(slot,&uid,&event);
    if(status==RF_NOT_FOUND)return scene_alarm_runner_cancel(o,now,0);if(status)return status;
    if(uid!=o->alarm.event||o->alarm.phase<1||o->alarm.phase>3)return RF_FORMAT;
    if(o->alarm.phase==1){
        /* Source switches to flight if another actor already raised the alarm.
         * This bounded slice returns to ordinary Waiting without a second
         * flight owner or duplicate activation. */
        if(rf_scene_alarm[4]||!o->script_move.active||o->script_move.event!=uid||o->script_move.follow||
           memcmp(o->script_move.target,event->position,12))return scene_alarm_runner_cancel(o,now,0);
        *owned=0;return RF_OK;
    }
    status=campaign_npc_idle_ground_step(stream,o,slot,elapsed,frame,landing);if(status)return status;
    if(o->damage.effects.health<=0||!o->alarm.present)return RF_OK;
    x=event->position[0]-o->body.state.position[0];z=event->position[2]-o->body.state.position[2];
    if(x*x+z*z>.25f)return scene_alarm_runner_cancel(o,now,0);
    if(o->alarm.phase==2){
        status=rf_timer_pending(o->pain.animation_lock,now,&pending);if(status)return status;if(pending)return RF_OK;
        for(uint32_t j=0;j<3;j++)target[j]=event->position[j]+1000.f*event->orientation_disk[j];
        target[1]=o->eye_position[1];
        memcpy(o->body.state.next_position,o->body.state.position,12);
        status=rf_scene_npc_steer(o->registration.handle,target,elapsed,rf_scene_npc_playback[0],&turn);if(status)return status;
        status=rf_scene_npc_prepare_angular(o->registration.handle,elapsed);if(status)return status;
        status=rf_scene_npc_commit_ordinary(o->registration.handle,elapsed);if(status)return status;
        x=target[0]-o->eye_position[0];z=target[2]-o->eye_position[2];length=sqrtf(x*x+z*z);
        forward_length=sqrtf(o->look.orientation[6]*o->look.orientation[6]+o->look.orientation[8]*o->look.orientation[8]);
        if(!isfinite(length)||!isfinite(forward_length)||length<=0||forward_length<=0)return RF_FORMAT;
        dot=(x*o->look.orientation[6]+z*o->look.orientation[8])/(length*forward_length);
        if(dot<.95f)return RF_OK; /* RF.exe589444, checked little-endian3333733f. */
        status=scene_alarm_runner_resources(slot,&o->selection,1);if(status)return status;
        status=campaign_actor_pose(slot,&pose);if(status)return status;if(!pose)return RF_FORMAT;
        model=campaign_playback_resources.models+pose->skeleton;
        status=scene_npc_speech_remove_owned(slot);if(status)return status;
        status=rf_motion_start(&pose->playback,model->resources,model->count,o->selection.mapping.actions[44],1.f,0);if(status)return status;
        o->script_animation.active=1;o->script_animation.loop=o->script_animation.freeze=0;
        o->script_animation.motion=o->selection.mapping.actions[44];
        o->alarm.phase=3;o->alarm.started=1;o->ai_mode.flags_530|=0x200000u;
        memset(o->look.command_708,0,12);memset(o->body.state.vector_c8,0,12);memset(o->body.state.vector_ec,0,12);
        ++scene_alarm_runner_stats[5];return RF_OK;
    }
    if(o->alarm.started!=2){
        uint32_t found=0;status=campaign_actor_pose(slot,&pose);if(status)return status;if(!pose)return RF_FORMAT;
        if(o->alarm.started!=1||!o->script_animation.active||
           o->script_animation.motion!=o->selection.mapping.actions[44])return scene_alarm_runner_cancel(o,now,0);
        for(uint32_t j=0;j<pose->playback.completion.active.count;j++)
            if(pose->playback.completion.active.slots[j].motion==o->script_animation.motion&&
               pose->playback.completion.active.slots[j].weight>0)found=1;
        return found?RF_OK:scene_alarm_runner_cancel(o,now,0);
    }
    runtime=scene_alarm_runner_event(uid);if(!runtime)return scene_alarm_runner_cancel(o,now,0);
    handle=runtime->handle;
    /* Retire our owner before dispatch; links can remove actors, replace their
     * commands or request a section. Never write this actor after callbacks. */
    status=scene_alarm_runner_cancel(o,now,0);if(status)return status;
    epoch=scene_alarm_runner_epoch;++scene_alarm_runner_servicing;
    {rf_startup_events_report report={0};
     status=rf_runtime_event_fire(&campaign_triggers,handle,UINT32_MAX,UINT32_MAX,now,
         &scene_gravity,&stream->particles,&campaign_forces,&report);}
    --scene_alarm_runner_servicing;
    if(status&&epoch==scene_alarm_runner_epoch)scene_alarm_runner_fault=1;
    scene_alarm_runner_stats[10]=(uint32_t)status;if(!status)++scene_alarm_runner_stats[6];
    return status;
}
#endif
