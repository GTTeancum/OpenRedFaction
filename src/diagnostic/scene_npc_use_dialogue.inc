#ifndef RF_SCENE_NPC_USE_DIALOGUE_INCLUDED
#define RF_SCENE_NPC_USE_DIALOGUE_INCLUDED
/* Ordinary Use9 speech, original4a179b ->409950 ->4305a0 ->4299b0.
 * Include after scene_message_audio.inc / scene_medic_speech.inc. Reuses
 * per-actor speech slots, actual class/persona/Foley metadata and bounded
 * lazy PCM. Ordinary Use has qualified facing/default-patrol pause; optional
 * chatter has transient timers. Speech gestures have an independent explicit
 * residual owner and are enabled only with their complete save joins. */
static uint32_t *scene_npc_use_groups;
static uint32_t scene_npc_use_group_count;
static rf_random_state scene_npc_use_random;
/* classes,bytes,bound,metadata failures,requests,starts,busy,ineligible,
 * silent,playback failures,UID,handle,group,sample,voice,status */
uint32_t rf_scene_npc_use_dialogue[16];
/* Standard audio-helper shape: requests,selections,plays,loads,PCM bytes,
 * sample,RNG,errors,name hash. PCM totals are shared-bank observations. */
uint32_t rf_scene_npc_use_audio[9];

static void scene_npc_use_dialogue_close(void)
{
    free(scene_npc_use_groups);scene_npc_use_groups=NULL;
    scene_npc_use_group_count=0;
    rf_scene_npc_use_dialogue[0]=rf_scene_npc_use_dialogue[1]=0;
}
static void scene_npc_use_dialogue_reset(void)
{
    scene_npc_use_dialogue_close();scene_npc_use_random.value=1;
    memset(rf_scene_npc_use_dialogue,0,sizeof(rf_scene_npc_use_dialogue));
    memset(rf_scene_npc_use_audio,0,sizeof(rf_scene_npc_use_audio));
    rf_scene_npc_use_dialogue[12]=rf_scene_npc_use_dialogue[13]=rf_scene_npc_use_dialogue[14]=UINT32_MAX;
}
/* Optional metadata must never reject an otherwise playable scene. Absent or
 * malformed class/persona bindings remain silent, with bounded diagnostics. */
static void scene_npc_use_dialogue_load(rf_vpp *tables,const void *entity_text,uint32_t entity_bytes)
{
    rf_vpp_entry entry;void *text=NULL;uint32_t count=campaign_seeds.class_count,i;int status;
    scene_npc_use_dialogue_close();
    if(!count)return;
    if(!tables || !entity_text || !entity_bytes || count>640){++rf_scene_npc_use_dialogue[3];return;}
    status=rf_vpp_find(tables,"personas.tbl",&entry);
    if(status || !entry.size || entry.size>64*1024){++rf_scene_npc_use_dialogue[3];return;}
    text=malloc(entry.size);
    if(!text){++rf_scene_npc_use_dialogue[3];return;}
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(status){++rf_scene_npc_use_dialogue[3];free(text);return;}
    scene_npc_use_groups=malloc(count*sizeof(*scene_npc_use_groups));
    if(!scene_npc_use_groups){++rf_scene_npc_use_dialogue[3];free(text);return;}
    for(i=0;i<count;i++) {
        const char *name;int32_t groups[3]={-1,-1,-1};uint32_t packed=0,k;
        uint32_t medic=campaign_seeds.classes[i].physics.use_kind==6;
        scene_npc_use_groups[i]=0;
        if(!medic && !(campaign_seeds.classes[i].physics.flags&0x20000u))continue;
        status=scene_turret_class_name(&campaign_seeds,i,&name);
        if(!status)status=rf_entity_persona_groups_read(entity_text,entity_bytes,name,
            text,entry.size,&campaign_foley,medic,groups);
        if(status){++rf_scene_npc_use_dialogue[3];continue;}
        for(k=0;k<3;k++){
            if(groups[k]<-1 || groups[k]>=640){status=RF_RANGE;break;}
            packed|=(uint32_t)(groups[k]+1)<<(k*10);
        }
        if(status){++rf_scene_npc_use_dialogue[3];continue;}
        scene_npc_use_groups[i]=packed;
        rf_scene_npc_use_dialogue[2]+=(packed&1023u)!=0;
    }
    free(text);scene_npc_use_group_count=count;
    rf_scene_npc_use_dialogue[0]=count;rf_scene_npc_use_dialogue[1]=count*sizeof(*scene_npc_use_groups);
    rf_scene_foley[3]+=rf_scene_npc_use_dialogue[1];
    if(rf_scene_foley[3]+entity_bytes+entry.size>rf_scene_foley[4])
        rf_scene_foley[4]=rf_scene_foley[3]+entity_bytes+entry.size;
}
/* Called only after successful world publication/storage close. A rejected
 * load retains the old speech. No voice or random sequence is saved/replayed. */
static void scene_npc_use_dialogue_stop(void)
{
    for(uint32_t i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++)
        if(campaign_message_voices[i].origin)campaign_message_release(campaign_message_voices+i);
    for(uint32_t i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++)
        campaign_message_voices[i].speech_end=campaign_message_voices[i].speech_next=-1;
    scene_npc_chatter_reset_timers();
}
static int32_t scene_npc_persona_choose(campaign_npc_body *owner,uint32_t lane,float *duration)
{
    campaign_pain_audio_context audio={&scene_npc_use_random,0,rf_scene_npc_use_audio,0};
    uint32_t index=(uint32_t)(owner-campaign_npc_bodies),cls;int32_t group,sample;int status;
    const rf_wave_pcm *pcm;
    if(duration)*duration=0;
    if(lane>=3 || index>=campaign_npc_body_count || index>=campaign_seeds.records.count || !scene_medic_live(owner))return -1;
    cls=campaign_seeds.items[index].class_index;
    if(!scene_npc_use_groups || cls>=scene_npc_use_group_count)return -1;
    group=(int32_t)((scene_npc_use_groups[cls]>>(lane*10))&1023u)-1;
    if(group<0)return -1;
    rf_scene_npc_use_dialogue[12]=(uint32_t)group;++rf_scene_npc_use_audio[0];
    sample=campaign_pain_audio_resolve(&audio,group);rf_scene_npc_use_audio[6]=scene_npc_use_random.value;
    if(audio.status || sample<0 || (uint32_t)sample>=campaign_audio_bank.count)return -1;
    rf_scene_npc_use_dialogue[13]=(uint32_t)sample;
    if(duration && (campaign_audio_sink || campaign_audio_events.play || campaign_audio_events.play_mode)){
        if(!rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)sample)){
            status=campaign_ambient_reload((uint32_t)sample);
            if(!status){campaign_audio_evictable[sample]=1;++rf_scene_sound_bank[1];
                rf_scene_sound_bank[2]+=campaign_audio_bank.samples[sample].bytes;rf_scene_live_audio[1]=campaign_audio_bank.bytes;}
        }
        pcm=rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)sample);
        if(pcm && pcm->rate)*duration=(float)((double)pcm->frames/pcm->rate);
    }
    return sample;
}
static void scene_npc_persona_play(campaign_npc_body *owner,int32_t sample,uint32_t gestures)
{
    scene_message_voice *slot=NULL;const rf_sound_metadata *metadata;const rf_wave_pcm *pcm;
    uint32_t i,index=(uint32_t)(owner-campaign_npc_bodies),uid,handle,voice_handle,before_bytes;
    int32_t voice;int status=RF_OK;
    if(index>=campaign_npc_body_count || index>=campaign_seeds.records.count || !scene_medic_live(owner) ||
       !isfinite(owner->damage.effects.health) || owner->damage.effects.health<=0 ||
       sample<0 || (uint32_t)sample>=campaign_audio_bank.count)return;
    uid=(uint32_t)campaign_seeds.records.items[index].record.uid;handle=owner->registration.handle;
    /* A real scripted actor line and Use response share actor1400 semantics.
     * Unrelated radio/flat lines cannot suppress this NPC's Use response. */
    for(i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++) {
        scene_message_voice *current=campaign_message_voices+i;
        if(current->handle && current->route==SCENE_MESSAGE_ACTOR && current->key==uid) {
            if(current->actor_handle==handle && campaign_message_active(current)){
                ++rf_scene_npc_use_dialogue[6];return;
            }
            campaign_message_release(current);
        }
        if(!slot && !campaign_message_active(current))slot=current;
    }
    if(!slot){status=RF_RANGE;goto failed;}
    for(i=0;i<3;i++)if(!isfinite(owner->eye_position[i])){status=RF_FORMAT;goto failed;}
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    /* Authored Use clips are one-shots. An unexpected looping mod sample
     * must not leave the NPC permanently busy in this speech-only owner. */
    if(metadata && (metadata->loop_flags&0x40000000u)){status=RF_NOT_FOUND;goto failed;}
    campaign_message_release(slot);before_bytes=campaign_audio_bank.bytes;
    voice=campaign_sound_start(sample,owner->eye_position,1,0,1,0);
    if(voice<0){status=RF_IO;goto failed;}
    status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,voice,&voice_handle);
    if(status)goto failed;
    /* Register retained playback before consulting the speaker again. A
     * generation change during an optional audio callback stops this voice. */
    slot->uid=0;slot->key=uid;slot->route=SCENE_MESSAGE_ACTOR;slot->handle=voice_handle;
    slot->sample=(uint32_t)sample;slot->voice=voice;slot->actor_handle=handle;slot->origin=1;
    slot->spatial=campaign_spatial_voices[voice_handle&0xffffu];
    owner=campaign_message_speaker(uid);
    if(!owner || owner->registration.handle!=handle){campaign_message_release(slot);status=RF_NOT_FOUND;goto failed;}
    pcm=rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)sample);
    if(pcm && pcm->rate) {
        uint64_t ms=((uint64_t)pcm->frames*1000+pcm->rate-1)/pcm->rate;
        slot->duration=ms<=RF_TIMER_PERIOD?(uint32_t)ms:0;
    }
    scene_npc_speech_arm(slot,campaign_blackout_now,gestures);
    ++rf_scene_npc_use_dialogue[5];rf_scene_npc_use_dialogue[14]=(uint32_t)voice;
    ++rf_scene_npc_use_audio[2];
    if(campaign_audio_bank.bytes>before_bytes){++rf_scene_npc_use_audio[3];
        rf_scene_npc_use_audio[4]+=campaign_audio_bank.bytes-before_bytes;}
    return;
failed:
    ++rf_scene_npc_use_dialogue[9];++rf_scene_npc_use_audio[7];
    rf_scene_npc_use_dialogue[15]=(uint32_t)status;
}

static uint32_t scene_npc_use_facing_eligible(const campaign_npc_body *o)
{
    if(!scene_medic_live(o) || !isfinite(o->damage.effects.health) || o->damage.effects.health<=0 ||
       (o->object_flags&(2u|0x4000u)) || (o->view.flags_810&1u) ||
       o->view.linked_handle!=-1 || o->attachment_75c!=-1 || o->controller_handle || !o->body.allocated_bytes ||
       o->movement_slot>=16 || (campaign_modes[o->movement_slot].index!=1 && campaign_modes[o->movement_slot].index!=3) ||
       campaign_npc_ai_suppressed(o) || o->scripted_physics || o->scripted_physics_seen ||
       o->combat_alert || o->combat_scripted || o->combat_target || o->medic.pending ||
       o->alarm.present || o->flight.entered || scene_noise_active(o) ||
       o->script_animation.active || o->script_look.active || o->script_shoot.active || o->script_once.count ||
       o->pain.selected_action>=0 || campaign_cutscene_runtime.active || campaign_endgame.phase ||
       (o->ai_mode.action_280!=2 && o->ai_mode.action_280!=4))return 0;
    if(o->script_move.active && (!o->patrol.enabled || o->script_move.origin!=SCENE_MOVE_ORIGIN_ACTOR_DEFAULT || o->script_move.follow))return 0;
    return 1;
}
static void scene_npc_use_facing_begin(campaign_npc_body *o,float duration)
{
    uint32_t index=(uint32_t)(o-campaign_npc_bodies);scene_npc_chatter_state *c;rf_entity_pose *pose;int32_t ms=500;
    if(!scene_npc_chatter || index>=campaign_npc_body_count || !scene_npc_use_facing_eligible(o) ||
       !campaign_player_object.view || campaign_player_object.view!=&campaign_player_view || campaign_player_view.linked_handle!=-1)return;
    if(campaign_actor_pose(index,&pose) || !pose || pose->playback.completion.active.freeze_slot>=0)return;
    c=scene_npc_chatter+index;if(c->handle!=o->registration.handle)return;
    if(isfinite(duration) && duration>0 && duration<(float)((RF_TIMER_PERIOD-500)/1000))ms+=(int32_t)floor(duration*1000.f+.5f);
    /* Existing qualified reset API, never a synthetic script/attack owner. */
    if(rf_scene_npc_reset_ai_animation(o->registration.handle,0) || rf_scene_npc_reset_ai_animation(o->registration.handle,1))return;
    if(rf_timer_set(&c->use_deadline,campaign_blackout_now,ms))return;
    c->use_target=campaign_player_object.handle;
}
static int scene_npc_use_facing_service(campaign_npc_body *o,uint32_t index,float elapsed,uint32_t frame,uint32_t *held)
{
    scene_npc_chatter_state *c;float point[3],turn;int expired,status;
    *held=0;if(!scene_npc_chatter || index>=campaign_npc_body_count)return RF_OK;
    c=scene_npc_chatter+index;if(c->use_deadline==-1)return RF_OK;
    if(c->handle!=o->registration.handle || !scene_npc_use_facing_eligible(o) ||
       c->use_target!=campaign_player_object.handle || campaign_player_object.view!=&campaign_player_view || campaign_player_view.linked_handle!=-1 ||
       !isfinite(campaign_player_damage.state.effects.health) || campaign_player_damage.state.effects.health<=0 ||
       rf_timer_expired(c->use_deadline,campaign_blackout_now,&expired) || expired){c->use_deadline=-1;return RF_OK;}
    /* A scripted replacement line supersedes this optional interaction. */
    for(uint32_t i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++){
        scene_message_voice *v=campaign_message_voices+i;
        if(v->route==SCENE_MESSAGE_ACTOR && v->actor_handle==o->registration.handle && !v->origin && campaign_message_active(v)){
            c->use_deadline=-1;return RF_OK;
        }
    }
    status=scene_medic_player_eye(frame,point);
    if(!status){
        if(o->view.flags_810&0x400u)memcpy(point,scene_actor_body.state.position,12);
        else point[1]-=.1f;
        memcpy(o->body.state.next_position,o->body.state.position,12);
        status=rf_scene_npc_steer(o->registration.handle,point,elapsed,frame,&turn);if(status){c->use_deadline=-1;return RF_OK;}
        status=rf_scene_npc_prepare_angular(o->registration.handle,elapsed);if(status){c->use_deadline=-1;return RF_OK;}
        status=rf_scene_npc_commit_ordinary(o->registration.handle,elapsed);if(status){c->use_deadline=-1;return RF_OK;}
    }else if(status!=RF_NOT_FOUND){c->use_deadline=-1;return RF_OK;}
    status=campaign_script_locomotion(index,0);if(status){c->use_deadline=-1;return RF_OK;}
    *held=1;return RF_OK;
}
static void scene_npc_use_dialogue(uint32_t handle)
{
    campaign_npc_body *owner=NULL;uint32_t i,cls,uid,authored_flags=0;int32_t sample;float duration=0;
    ++rf_scene_npc_use_dialogue[4];rf_scene_npc_use_dialogue[11]=handle;rf_scene_npc_use_dialogue[15]=0;
    for(i=0;i<campaign_npc_body_count;i++)if(scene_medic_live(campaign_npc_bodies+i) &&
        campaign_npc_bodies[i].registration.handle==handle){owner=campaign_npc_bodies+i;break;}
    if(!owner || i>=campaign_seeds.records.count)goto ineligible;
    cls=campaign_seeds.items[i].class_index;uid=(uint32_t)campaign_seeds.records.items[i].record.uid;
    rf_scene_npc_use_dialogue[10]=uid;
    if(!rf_level_entity_chatter_flags_read(campaign_seeds.records.items+i,&authored_flags) && (authored_flags&4u))goto ineligible;
    if(cls>=campaign_seeds.class_count || campaign_seeds.classes[cls].physics.use_kind!=9 ||
       !(campaign_seeds.classes[cls].physics.flags&0x20000u) ||
       !owner->damage.effects.affiliation || !isfinite(owner->damage.effects.health) || owner->damage.effects.health<=0 ||
       (owner->object_flags&(2u|0x4000u)) || (owner->view.flags_810&1u) ||
       ((owner->view.flags_814|owner->damage.effects.flags_814)&0x200u) ||
       (owner->ai_mode.action_280!=2 && owner->ai_mode.action_280!=4) ||
       owner->combat_alert || owner->combat_scripted || owner->script_animation.active ||
       owner->script_look.active || owner->script_shoot.active)goto ineligible;

    if(scene_medic_speech_active(owner)){++rf_scene_npc_use_dialogue[6];return;}
    sample=scene_npc_persona_choose(owner,0,&duration);
    scene_npc_use_facing_begin(owner,duration);
    if(sample<0){++rf_scene_npc_use_dialogue[8];return;}
    scene_npc_persona_play(owner,sample,1);return;
ineligible:
    ++rf_scene_npc_use_dialogue[7];
}

/* Optional automatic persona presentation. Timers reset only on fresh identity
 * or successful world publication; gameplay deadlines/save rows are untouched. */
static void scene_npc_chatter_arm(int32_t *deadline,int32_t now,uint32_t base,uint32_t range,uint32_t duration)
{
    uint32_t random=0;*deadline=-1;
    if(rf_random_next(&scene_npc_use_random,&random))return;
    if(duration>RF_TIMER_PERIOD-base-range)return;
    (void)rf_timer_set(deadline,now,(int32_t)(base+random%range+duration));
}
static void scene_npc_chatter_initialize(uint32_t index,int32_t now)
{
    scene_npc_chatter_state *c;uint32_t flags=0;
    if(!scene_npc_chatter || index>=campaign_npc_body_count || index>=campaign_seeds.records.count)return;
    c=scene_npc_chatter+index;c->humming=c->timeout=c->use_deadline=-1;c->use_target=0;c->flags=0;
    c->handle=campaign_npc_bodies[index].registration.handle;
    if(rf_level_entity_chatter_flags_read(campaign_seeds.records.items+index,&flags))return;
    c->flags=flags|8u;
    scene_npc_chatter_arm(&c->humming,now,60000,30000,0);
}
static void scene_npc_chatter_reset_timers(void)
{
    if(!scene_npc_chatter)return;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        scene_npc_chatter_state *c=scene_npc_chatter+i;
        c->handle=campaign_npc_bodies[i].registration.handle;c->timeout=c->humming=c->use_deadline=-1;c->use_target=0;
        if(c->flags&8u)scene_npc_chatter_arm(&c->humming,campaign_blackout_now,60000,30000,0);
    }
}
static void scene_npc_chatter_tick(uint32_t frame)
{
    int32_t now=campaign_blackout_now;
    if(!frame || !scene_npc_chatter || campaign_cutscene_runtime.active || campaign_endgame.phase)return;
    for(uint32_t i=0;i<campaign_npc_body_count && i<campaign_seeds.records.count;i++){
        campaign_npc_body *o=campaign_npc_bodies+i;scene_npc_chatter_state *c=scene_npc_chatter+i;
        uint32_t cls=campaign_seeds.items[i].class_index;int due=0;int32_t sample;float seconds=0;
        if(c->handle!=o->registration.handle){scene_npc_chatter_initialize(i,now);continue;}
        if(c->use_deadline!=-1 && !scene_npc_use_facing_eligible(o))c->use_deadline=-1;
        if(!(c->flags&8u) || cls>=campaign_seeds.class_count ||
           campaign_seeds.classes[cls].physics.use_kind==6 || !(campaign_seeds.classes[cls].physics.flags&0x20000u) ||
           !scene_medic_live(o) || !isfinite(o->damage.effects.health) || o->damage.effects.health<=0 ||
           (o->object_flags&(2u|0x4000u)) || (o->view.flags_810&1u) || campaign_npc_ai_suppressed(o) ||
           o->view.linked_handle!=-1 || o->combat_alert || o->combat_scripted || o->combat_target ||
           o->script_animation.active || o->script_move.active || o->script_look.active || o->script_shoot.active || o->script_once.count ||
           (o->ai_mode.action_280!=2 && o->ai_mode.action_280!=4) || scene_medic_speech_active(o))continue;
        /*403ea0 Timeout first. Source distance helper returns squared distance. */
        if(((c->flags&1u) || (o->view.flags_810&0x01000000u)) && !(c->flags&4u) &&
           !((o->view.flags_814|o->damage.effects.flags_814)&0x200u) &&
           campaign_player_object.view==&campaign_player_view &&
           isfinite(campaign_player_damage.state.effects.health) && campaign_player_damage.state.effects.health>0 &&
           (campaign_player_view.flags_810&0x800u) && campaign_player_view.linked_handle==-1){
            float d2=0;
            for(uint32_t j=0;j<3;j++){float d=o->body.state.position[j]-scene_actor_body.state.position[j];d2+=d*d;}
            if(c->timeout==-1){if(isfinite(d2)&&d2<4.f)scene_npc_chatter_arm(&c->timeout,now,6000,4000,0);}
            else if(!rf_timer_expired(c->timeout,now,&due)&&due){
                c->timeout=-1;
                if(isfinite(d2)&&d2<4.f){sample=scene_npc_persona_choose(o,2,NULL);scene_npc_persona_play(o,sample,1);}
            }
        }
        if(scene_medic_speech_active(o))continue;
        if((c->flags&2u) || ((o->view.flags_814|o->damage.effects.flags_814)&1u) ||
           (o->view.flags_810&0x02000000u) || c->humming==-1)continue;
        due=0;if(rf_timer_expired(c->humming,now,&due)||!due)continue;
        sample=scene_npc_persona_choose(o,1,&seconds);scene_npc_persona_play(o,sample,0);
        /* Optional failures still rearm, avoiding per-frame PCM retries. */
        scene_npc_chatter_arm(&c->humming,now,60000,30000,
            isfinite(seconds)&&seconds>0&&seconds<(float)(RF_TIMER_PERIOD/1000)?(uint32_t)floor(seconds*1000.f+.5f):0);
    }
}

#endif
