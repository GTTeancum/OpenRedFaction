#ifndef RF_SCENE_NPC_FOOTSTEPS_INCLUDED
#define RF_SCENE_NPC_FOOTSTEPS_INCLUDED

/* Ordinary NPC update, after main-model advancement (487cf0/41daf0), never
 * rendering. Original41e6e4 calls42f940: linked-parent rejection, exact named
 * markers, support material/class fallback, then left/right sample halves.
 * The existing scene support owner supplies entity1380; bounds.radius is180.
 * Player view-mode0 instead uses42fb20 and is deliberately outside this pass.
 */
uint32_t rf_scene_npc_footsteps[13]; /* polls,consumed,requests,starts,failures,unsupported surface,UID,material,group,sample,voice,status,RNG */
char rf_scene_npc_footstep_sample[64];
static uint32_t scene_npc_footstep_reports;

static void scene_npc_footsteps_reset(void)
{
    memset(rf_scene_npc_footsteps,0,sizeof(rf_scene_npc_footsteps));
    memset(rf_scene_npc_footstep_sample,0,sizeof(rf_scene_npc_footstep_sample));
    rf_scene_npc_footsteps[8]=rf_scene_npc_footsteps[9]=rf_scene_npc_footsteps[10]=UINT32_MAX;
    scene_npc_footstep_reports=0;
}

static int scene_npc_footsteps_actor(uint32_t actor,rf_entity_pose *pose,uint32_t frame)
{
    campaign_npc_body *owner=campaign_npc_bodies+actor;
    const rf_entity_model_motions *model;
    rf_entity_footstep_input input={0};rf_entity_footstep_plan plan;
    rf_entity_footstep_group groups[10]={{0}};
    uint32_t cls=campaign_seeds.items[actor].class_index,i,before,consumed;
    int status;
    if(cls>=campaign_seeds.class_count || pose->skeleton>=campaign_motion_catalog.model_count ||
       !campaign_motion_catalog.models || !campaign_footstep_groups)return RF_RANGE;
    model=campaign_motion_catalog.models+pose->skeleton;
    ++rf_scene_npc_footsteps[0];
    input.linked_handle=owner->view.linked_handle;
    input.object_flags=owner->object_flags;
    /* Keep42f940's linked gate before touching either pending marker. */
    if(input.linked_handle!=-1)return RF_OK;
    if(owner->support.material<0 || owner->support.material>=10) {
        /* The bounded port cannot index before/after the ten class slots.
         * Discard only actual named footsteps while unsupported, so an old
         * airborne event cannot be replayed when a later landing supplies a
         * material. Other marker names and their pending bits are untouched.
         */
        static const char *const names[2]={"footstep_left","footstep_right"};
        for(i=0;i<2;i++) {
            status=rf_motion_consume_marker(&pose->playback,model->marker_names,model->count,names[i],&consumed);
            if(status)return status;
            rf_scene_npc_footsteps[1]+=consumed;rf_scene_npc_footsteps[5]+=consumed;
        }
        return RF_OK;
    }
    input.surface=(uint32_t)owner->support.material;
    memcpy(input.position,owner->published,sizeof(input.position));
    input.vertical_offset=owner->body.state.bounds.radius;
    /* The NPC spatial branch of48a930 ignores side_value; the flat player
     * branch is excluded. Retain only a ten-slot view, not another Foley bank.
     * Each compact slot maps back to its original class-bound group below.
     */
    for(i=0;i<10;i++) {
        int32_t group=campaign_footstep_groups[cls][i];
        input.groups[i]=-1;
        if(group>=0) {
            const rf_foley_group *source;
            if((uint32_t)group>=campaign_foley.group_count || !campaign_foley.groups)return RF_RANGE;
            source=campaign_foley.groups+group;
            if(source->count>INT32_MAX || source->first>campaign_foley.sample_count ||
               source->count>campaign_foley.sample_count-source->first)return RF_FORMAT;
            input.groups[i]=(int32_t)i;groups[i].count=(int32_t)source->count;groups[i].first=source->first;
        }
    }
    before=pose->playback.event_mask;
    status=rf_entity_plan_footsteps(&input,&pose->playback,model->marker_names,model->count,groups,10,&plan);
    consumed=before&~pose->playback.event_mask;
    rf_scene_npc_footsteps[1]+=(consumed&1u)+((consumed>>1)&1u);
    if(status)return status;
    for(i=0;i<plan.count;i++) {
        const rf_entity_footstep_request *step=plan.requests+i;
        rf_audio_group_input choice={0};rf_audio_group_request request;
        int32_t voice=-1,group=campaign_footstep_groups[cls][step->group];
        const rf_foley_group *source=campaign_foley.groups+group;
        uint32_t available=source->first+source->count-step->first;
        ++rf_scene_npc_footsteps[2];rf_scene_npc_footsteps[8]=(uint32_t)group;
        rf_scene_npc_footsteps[6]=(uint32_t)campaign_seeds.records.items[actor].record.uid;
        rf_scene_npc_footsteps[7]=(uint32_t)owner->support.material;
        rf_scene_npc_footsteps[9]=rf_scene_npc_footsteps[10]=UINT32_MAX;
        rf_scene_npc_footstep_sample[0]=0;
        choice.object_kind=owner->view.type; /* No player record: spatial. */
        memcpy(choice.position,step->position,sizeof(choice.position));
        memcpy(choice.parameters,step->parameters,sizeof(choice.parameters));
        /* Original48a930 selects sample0 for count<=1, including a zero
         * half of an odd singleton. Never borrow beyond this declared group.
         * This uses the existing Foley RNG; complete retail frame RNG order
         * remains separate from this first-playable scene integration.
         */
        if(!campaign_foley.samples || !available)status=RF_NOT_FOUND;
        else status=rf_audio_group_choose(&choice,campaign_foley.samples+step->first,
            available,step->count,&combat_sound_random,&request);
        if(!status) {
            rf_scene_npc_footsteps[9]=(uint32_t)request.sample;
            if(request.sample<0 || (uint32_t)request.sample>=campaign_audio_bank.count)status=RF_NOT_FOUND;
            else {
                const char *name=campaign_audio_bank.samples[request.sample].name;
                memcpy(rf_scene_npc_footstep_sample,name,strlen(name)+1);
                /* Same budgeted lazy PCM/idle eviction and generation-owned
                 * mixer/native voices as other ordinary scene sounds. */
                voice=campaign_sound_start(request.sample,request.position,request.parameters[0],
                    request.group,request.spatial,0);
                if(voice<0)status=RF_IO;
            }
        }
        if(status)++rf_scene_npc_footsteps[4];else ++rf_scene_npc_footsteps[3];
        rf_scene_npc_footsteps[10]=(uint32_t)voice;rf_scene_npc_footsteps[11]=(uint32_t)status;
        rf_scene_npc_footsteps[12]=combat_sound_random.value;
        /* Bounded source-validation log; counters continue for the full run. */
        if(scene_npc_footstep_reports<16 || (status && rf_scene_npc_footsteps[4]<=4)) {
            ++scene_npc_footstep_reports;
            printf("NPC_FOOTSTEP frame%u uid%u material%d group%s file%s markers%u starts%u failures%u status%d\n",
                frame,rf_scene_npc_footsteps[6],owner->support.material,source->name,
                rf_scene_npc_footstep_sample[0]?rf_scene_npc_footstep_sample:"<none>",
                rf_scene_npc_footsteps[1],rf_scene_npc_footsteps[3],rf_scene_npc_footsteps[4],status);
        }
    }
    return RF_OK; /* Playback exhaustion never cancels movement or retries a marker. */
}

static void scene_npc_footsteps_tick(uint32_t frame)
{
    uint32_t actor;
    for(actor=0;actor<campaign_npc_body_count;actor++) {
        campaign_npc_body *owner=campaign_npc_bodies+actor;rf_entity_pose *pose=NULL;int status;
        /* Original41e4b0 rejects object4000 and absent models. Registration
         * qualifies the current owner; flag8 excludes the separate player. */
        if(!owner->registration.view || (owner->object_flags&(2u|8u|0x4000u)))continue;
        if(rf_entity_lookup(&campaign_entities,(int32_t)owner->registration.handle)!=&owner->view)continue;
        status=campaign_actor_pose(actor,&pose);
        if(!status && (!pose || !(pose->playback.event_mask&3u)))continue;
        if(!status)status=scene_npc_footsteps_actor(actor,pose,frame);
        if(status) {
            ++rf_scene_npc_footsteps[4];rf_scene_npc_footsteps[11]=(uint32_t)status;
            if(rf_scene_npc_footsteps[4]<=4)printf("NPC_FOOTSTEP_FAILED frame%u actor%u status%d\n",frame,actor,status);
        }
    }
}
#endif
