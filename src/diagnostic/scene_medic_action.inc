#ifndef RF_SCENE_MEDIC_ACTION_INCLUDED
#define RF_SCENE_MEDIC_ACTION_INCLUDED
/* RF.exe40a670/40a540,407ee0. Source references and bounded audio/input
 * qualifications are in docs/MEDIC-USE-ACTION.md. */
static int scene_medic_syringe_present(const campaign_npc_body *owner);
static uint32_t scene_medic_speech_active(const campaign_npc_body *owner);
static void combat_sound(const char *name,const float position[3]);
/* accepted,completed,range abort,missing/dead,full,refused,superseded,
 * reserve debit bits,last actor UID,last target handle,resource failures */
uint32_t rf_scene_medic_action[11];
/* Actual player entity eye, captured before cinematic/aim-camera overrides.
 * Retain its body-relative world offset through the subsequent physics step. */
static struct {uint32_t handle,frame;float eye[3],body[3];} scene_medic_player_aim;
static void scene_medic_player_eye_publish(uint32_t frame,const float eye[3])
{
    scene_medic_player_aim.handle=campaign_player_object.handle;
    scene_medic_player_aim.frame=frame;
    memcpy(scene_medic_player_aim.eye,eye,12);
    memcpy(scene_medic_player_aim.body,scene_actor_body.state.position,12);
}
static int scene_medic_player_eye(uint32_t frame,float aim[3])
{
    if(scene_medic_player_aim.handle!=campaign_player_object.handle||
       scene_medic_player_aim.frame!=frame)return RF_NOT_FOUND;
    for(uint32_t j=0;j<3;j++){
        aim[j]=scene_medic_player_aim.eye[j]+(scene_actor_body.state.position[j]-scene_medic_player_aim.body[j]);
        if(!isfinite(aim[j]))return RF_FORMAT;
    }
    return RF_OK;
}
static int32_t scene_medic_now(uint32_t frame)
{
    uint64_t elapsed=(uint64_t)frame*1000/60;
    return (int32_t)(elapsed?((elapsed-1)%RF_TIMER_PERIOD)+1:0);
}
static uint32_t scene_medic_qualified(const campaign_npc_body *owner)
{
    uint32_t i;const char *name;
    if(!owner||!campaign_npc_bodies||owner<campaign_npc_bodies||
       owner>=campaign_npc_bodies+campaign_npc_body_count)return 0;
    i=(uint32_t)(owner-campaign_npc_bodies);
    if(i>=campaign_seeds.records.count)return 0;
    name=campaign_seeds.records.items[i].record.class_name;
    return rf_emitter_name_lookup(&name,1,"medic1")==0;
}
static uint32_t scene_medic_live(const campaign_npc_body *owner)
{
    return owner&&owner->registration.view==&owner->view&&
        rf_object_registry_lookup(&campaign_registry,owner->registration.handle)==&owner->registration&&
        rf_entity_lookup(&campaign_entities,(int32_t)owner->registration.handle)==&owner->view;
}
static int scene_medic_initialize(campaign_npc_body *owner,uint32_t index,int32_t now)
{
    static const int32_t defaults[6]={1,2,4,5,11,-1};rf_level_entity_ai authored;int status;
    memset(&owner->medic,0,sizeof(owner->medic));owner->medic.motion=-1;
    owner->medic.deadline=-1;owner->medic.default_action=-1;
    owner->medic_reserve_seen=0;owner->medic_reserve=0;
    if(!scene_medic_qualified(owner))return RF_OK;
    status=rf_level_entity_ai_read(campaign_seeds.records.items+index,&authored);if(status)return status;
    if(authored.mode>=6)return RF_FORMAT;
    owner->medic.default_action=defaults[authored.mode];owner->medic_reserve=200.f;
    /*4649c1/59c014; medic1 is not the separate camera2 default2 exception. */
    status=rf_entity_ai_set_action(&owner->ai_mode,owner->medic.default_action,
        UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
    owner->view.action_520=owner->ai_mode.action_280;return RF_OK;
}
static void scene_medic_default(campaign_npc_body *owner,int32_t now)
{
    rf_entity_ai_transition_state next=owner->ai_mode;
    /* A newer action must never be replaced by a stale healing default. */
    if(owner->ai_mode.action_280!=14||owner->view.action_520!=14)return;
    if(!rf_entity_ai_set_action(&next,owner->medic.default_action,UINT32_MAX,UINT32_MAX,(float)now,0,0)){
        owner->ai_mode=next;owner->view.action_520=next.action_280;
    }
}
static void scene_medic_supersede(campaign_npc_body *owner)
{
    if(!owner->medic.pending)return;
    scene_medic_default(owner,scene_medic_now(rf_scene_profile_stage[0]));
    owner->medic.pending=0;++rf_scene_medic_action[6];
    /* AI default does not stop model43. A new animation naturally replaces
     * it; otherwise its specific residual owner remains until playback ends. */
}
static float scene_use_approximate_length(const float delta[3])
{
    float a=fabsf(delta[0]),b=fabsf(delta[1]),c=fabsf(delta[2]),swap;
    if(a<b){swap=a;a=b;b=swap;}if(b<c){swap=b;b=c;c=swap;}
    if(a<b){swap=a;a=b;b=swap;}return a+.375f*b+.1875f*c;
}
static int scene_medic_clip_reconcile(campaign_npc_body *owner,uint32_t index)
{
    rf_entity_pose *pose;int status;uint32_t found=0;
    if(!owner->medic.clip_owned)return RF_OK;
    status=campaign_actor_pose(index,&pose);if(status)return status;
    if(pose)for(uint32_t j=0;j<pose->playback.completion.active.count;j++){
        const rf_motion_active_slot *slot=pose->playback.completion.active.slots+j;
        if(slot->motion==owner->medic.motion){found=1;break;}
    }
    if(!found)owner->medic.clip_owned=0;
    return RF_OK;
}
/* Port presentation policy: an owned nonfreezing43 continues its normal
 * cursor clock when ordinary visibility culls this model. No other slot or
 * loop phase advances, and normally visible models never take this branch.
 * This keeps a finite transient save/handoff guard from becoming permanent. */
static int scene_medic_clip_culled(campaign_npc_body *owner,rf_entity_pose *pose,
    rf_entity_playback_model *model,float elapsed)
{
    rf_motion_playback_state next;rf_motion_playback_resource *resource;
    int32_t delta,references;uint32_t slot;int status;int64_t tick;
    if(!owner->medic.clip_owned)return RF_OK;
    if(!isfinite(elapsed)||elapsed<0)return RF_RANGE;
    if(owner->medic.motion<0||(uint32_t)owner->medic.motion>=model->count)return RF_FORMAT;
    resource=model->resources+owner->medic.motion;
    if(resource->looping||resource->comparison.end_tick<=resource->comparison.start_tick)return RF_FORMAT;
    next=pose->playback;
    for(slot=0;slot<next.completion.active.count;slot++)
        if(next.completion.active.slots[slot].motion==owner->medic.motion)break;
    if(slot==next.completion.active.count){owner->medic.clip_owned=0;return RF_OK;}
    if(next.completion.active.freeze_slot==(int32_t)slot)return RF_FORMAT;
    status=rf_motion_elapsed_ticks(elapsed,&delta);if(status)return status;
    tick=(int64_t)next.completion.active.slots[slot].tick+delta;
    if(tick>INT32_MAX)return RF_RANGE;
    next.completion.active.slots[slot].tick=(int32_t)tick;
    if(tick>=resource->comparison.end_tick||next.completion.active.slots[slot].weight==0){
        if(next.completion.active.primary_slot==(int32_t)slot){
            next.completion.active.primary_slot=-1;next.completion.primary_flag=0;
            memset(next.completion.primary_words,0,sizeof(next.completion.primary_words));
            memset(next.completion.primary_vectors,0,sizeof(next.completion.primary_vectors));
        }
        references=resource->references;
        status=rf_motion_remove_slot(&next.completion.active,owner->medic.motion,&references);if(status)return status;
        resource->references=references;owner->medic.clip_owned=0;
    }
    next.generation=(next.generation+1)&65535u;pose->playback=next;
    return RF_OK;
}
static int scene_medic_accept(campaign_npc_body *owner,uint32_t frame)
{
    uint32_t index=(uint32_t)(owner-campaign_npc_bodies);rf_entity_pose *pose;
    rf_entity_playback_model *model;rf_motion_playback_resource *resources=NULL;
    rf_motion_playback_state playback;rf_entity_ai_transition_state ai;
    int32_t motion,deadline,sound,sounds[45];int status;double duration,delay;
    const float health=campaign_player_damage.state.effects.health;
    const float maximum=campaign_player_damage.state.effects.class_health;
    if(!frame||!scene_medic_qualified(owner)||!scene_medic_live(owner)||
       owner->ai_mode.action_280!=2||owner->view.action_520!=2||
       !isfinite(owner->damage.effects.health)||owner->damage.effects.health<=0||
       !isfinite(owner->medic_reserve)||owner->medic_reserve<=0||owner->medic_reserve>200||
       !scene_medic_syringe_present(owner)||scene_medic_speech_active(owner)||
       (campaign_player_view.flags_810&1u)||!isfinite(health)||health<=0||!isfinite(maximum)||maximum<=0||health>=maximum){
        ++rf_scene_medic_action[5];return RF_OK;
    }
    status=campaign_actor_pose(index,&pose);if(status||!pose)goto unavailable;
    if(pose->skeleton>=campaign_playback_resources.model_count){status=RF_RANGE;goto unavailable;}
    model=campaign_playback_resources.models+pose->skeleton;motion=owner->selection.mapping.actions[43];
    if(motion<0||(uint32_t)motion>=model->count||owner->selection.mapping.skeleton!=pose->skeleton){status=RF_NOT_FOUND;goto unavailable;}
    status=campaign_npc_motion_require(pose->skeleton,(uint32_t)motion);if(status)goto unavailable;
    if(model->resources[motion].looping){status=RF_FORMAT;goto unavailable;}
    /* Real clip start/end; success persona selection is not implemented yet.
     * Optional Foley/device success cannot alter this clip-only deadline. */
    duration=rf_motion_duration(model->resources[motion].comparison.start_tick,
        model->resources[motion].comparison.end_tick);
    delay=750.0+floor(duration*1000.0+.5);
    if(!isfinite(duration)||duration<=0||delay>RF_TIMER_PERIOD){status=RF_RANGE;goto unavailable;}
    status=rf_timer_set(&deadline,scene_medic_now(frame),(int32_t)delay);if(status)goto unavailable;
    if(model->count>1024){status=RF_RANGE;goto unavailable;}
    resources=malloc(model->count*sizeof(*resources));if(!resources){status=RF_IO;goto unavailable;}
    memcpy(resources,model->resources,model->count*sizeof(*resources));playback=pose->playback;ai=owner->ai_mode;
    /* Stage fallible model changes so rejected Use leaves startup kneeling
     * and its script owner intact. Nothing borrows pain/death fields. */
    if(owner->script_animation.active){
        status=rf_motion_stop_slot(&playback,owner->script_animation.motion);if(status)goto unavailable;
        status=rf_motion_stop_looping(&playback,resources,model->count);if(status)goto unavailable;
    }
    for(uint32_t j=0;j<45;j++)sounds[j]=-1;
    status=rf_motion_start_action(&playback,resources,model->count,owner->selection.mapping.actions,
        sounds,43,1,0,0,&sound);if(status)goto unavailable;
    status=rf_entity_ai_set_action(&ai,14,UINT32_MAX,UINT32_MAX,(float)scene_medic_now(frame),0,0);if(status)goto unavailable;
    memcpy(model->resources,resources,model->count*sizeof(*resources));free(resources);resources=NULL;
    pose->playback=playback;pose->controller.override_enabled=0;
    if(owner->script_animation.active)++rf_scene_script_animation[9];
    memset(&owner->script_animation,0,sizeof(owner->script_animation));
    owner->ai_override.action_1364=owner->ai_override.motion_1368=owner->ai_override.word_834=-1;
    owner->ai_mode=ai;owner->view.action_520=14;
    /* AI14 owns dispatch, not independent route/look/fire/reload records.
     * Preserve those owners; explicit source-qualified action replacement
     * supersedes this transaction through its own command path. */
    owner->medic.pending=owner->medic.clip_owned=1;owner->medic.motion=motion;owner->medic.deadline=deadline;
    owner->medic.source=owner->registration.handle;owner->medic.source_view=&owner->view;
    owner->medic.target=campaign_player_object.handle;owner->medic.target_view=&campaign_player_view;
    ++rf_scene_medic_action[0];rf_scene_medic_action[8]=(uint32_t)campaign_seeds.records.items[index].record.uid;
    rf_scene_medic_action[9]=owner->medic.target;
    if(owner->selection.action_sounds[43][0])combat_sound(owner->selection.action_sounds[43],owner->eye_position);
    return RF_OK;
unavailable:
    free(resources);++rf_scene_medic_action[10];
    /* Selected owner still consumes the press; resource failure is refusal. */
    return RF_OK;
}
static int scene_medic_tick(campaign_npc_body *owner,uint32_t index,uint32_t frame,float elapsed,uint32_t steer)
{
    float delta[3],aim[3],range,health,maximum,deficit,transfer,turn;int expired,status;
    if(!owner->medic.pending)return RF_OK;
    if(owner->ai_mode.action_280!=14||owner->view.action_520!=14){
        owner->medic.pending=0;++rf_scene_medic_action[6];return RF_OK;
    }
    if(!frame)return RF_OK;
    /* Port lifecycle policy: no resurrecting dead/replaced player or healer.
     * Original426fc0 checks type/full handle, not positive health. */
    if(!scene_medic_live(owner)||owner->medic.source!=owner->registration.handle||
       owner->medic.source_view!=&owner->view||!isfinite(owner->damage.effects.health)||owner->damage.effects.health<=0||
       (owner->view.flags_810&1u)||owner->medic.target!=campaign_player_object.handle||
       owner->medic.target_view!=&campaign_player_view||campaign_player_object.view!=&campaign_player_view||
       rf_object_registry_lookup(&campaign_registry,owner->medic.target)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)owner->medic.target)!=&campaign_player_view||
       (campaign_player_view.flags_810&1u)||campaign_player_damage.state.effects.health<=0){
        scene_medic_default(owner,scene_medic_now(frame));owner->medic.pending=0;
        ++rf_scene_medic_action[3];return RF_OK;
    }
    for(uint32_t j=0;j<3;j++)delta[j]=scene_actor_body.state.position[j]-owner->body.state.position[j];
    range=owner->model_radius_78+campaign_player_geometry.model_radius+3.5f;
    if(!isfinite(range)||range<0||!isfinite(delta[0])||!isfinite(delta[1])||!isfinite(delta[2]))return RF_FORMAT;
    if(scene_use_approximate_length(delta)>range){
        rf_entity_pose *pose;status=campaign_actor_pose(index,&pose);if(status)return status;
        if(pose){rf_entity_playback_model *model=campaign_playback_resources.models+pose->skeleton;
            status=rf_motion_stop_nonlooping(&pose->playback,model->resources,model->count);if(status)return status;}
        /* Stop only zeroes weights; retain ownership until the actual slot
         * is removed, including when ordinary playback is visibility-culled. */
        scene_medic_default(owner,scene_medic_now(frame));owner->medic.pending=0;
        ++rf_scene_medic_action[2];return RF_OK;
    }
    if(steer){
    status=scene_medic_player_eye(frame,aim);if(status)return status;
    memcpy(owner->body.state.next_position,owner->body.state.position,12);
    /*48a8d0 resolved type0 returns target7d4, never the listener camera. */
    status=rf_scene_npc_steer(owner->registration.handle,aim,elapsed,frame,&turn);if(status)return status;
    status=rf_scene_npc_prepare_angular(owner->registration.handle,elapsed);if(status)return status;
    status=rf_scene_npc_commit_ordinary(owner->registration.handle,elapsed);if(status)return status;}
    health=campaign_player_damage.state.effects.health;maximum=campaign_player_damage.state.effects.class_health;
    if(!isfinite(health)||health<=0||!isfinite(maximum)||maximum<=0||
       !isfinite(owner->medic_reserve)||owner->medic_reserve<0||owner->medic_reserve>200)return RF_FORMAT;
    deficit=maximum-health;
    if(deficit<=0){scene_medic_default(owner,scene_medic_now(frame));owner->medic.pending=0;
        ++rf_scene_medic_action[4];return RF_OK;}
    status=rf_timer_expired(owner->medic.deadline,scene_medic_now(frame),&expired);if(status)return status;
    if(!expired)return RF_OK;
    transfer=fminf(deficit,owner->medic_reserve);
    /* One exact finite current-deficit transfer, without difficulty/armor or
     * pickup semantics. No callback can observe a half-committed transfer. */
    if(transfer>0){
        campaign_player_damage.state.effects.health=fmaxf(0,fminf(maximum,health+transfer));
        owner->medic_reserve-=transfer;owner->medic_reserve_seen=1;
        memcpy(rf_scene_medic_action+7,&transfer,4);
    }
    scene_medic_default(owner,scene_medic_now(frame));owner->medic.pending=0;
    ++rf_scene_medic_action[1];return RF_OK;
}
static uint32_t scene_medic_capture_transient(const campaign_npc_body *owner,int32_t motion)
{
    return scene_medic_live(owner)&&owner->medic.clip_owned&&
        owner->medic.source==owner->registration.handle&&owner->medic.source_view==&owner->view&&
        motion>=0&&motion==owner->medic.motion&&motion==owner->selection.mapping.actions[43];
}
static int scene_medic_save_guard(const char **reason)
{
    if(reason)*reason=NULL;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->medic.pending||o->medic.clip_owned){if(reason)*reason=o->medic.pending?"medic_action_pending":"medic_clip_pending";return RF_NOT_FOUND;}
    }
    return RF_OK;
}
static void scene_medic_load_committed(void)
{
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        campaign_npc_bodies[i].medic.pending=campaign_npc_bodies[i].medic.clip_owned=0;
        campaign_npc_bodies[i].medic.deadline=campaign_npc_bodies[i].medic.motion=-1;
    }
    scene_use_edge.held=scene_use_edge.raw;scene_use_edge.edge=0;
    scene_use_edge.selected=UINT32_MAX;scene_use_edge.claimed=scene_use_edge.raw;scene_use_edge.consumed=1;
    scene_use_edge.player=campaign_player_object.handle;
}
#endif
