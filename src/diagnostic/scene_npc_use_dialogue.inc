#ifndef RF_SCENE_NPC_USE_DIALOGUE_INCLUDED
#define RF_SCENE_NPC_USE_DIALOGUE_INCLUDED
/* Ordinary Use9 speech, original4a179b ->409950 ->4305a0 ->4299b0.
 * Include after scene_message_audio.inc / scene_medic_speech.inc. Reuses
 * per-actor speech slots, actual class/persona/Foley metadata and bounded
 * lazy PCM. Speech-only first pass: no facing, animation reset, wandering
 * pause, target change or gameplay state to invent/persist. */
static int32_t *scene_npc_use_groups;
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
        const char *name;
        scene_npc_use_groups[i]=-1;
        if(campaign_seeds.classes[i].physics.use_kind!=9)continue;
        status=scene_turret_class_name(&campaign_seeds,i,&name);
        if(!status)status=rf_entity_use_sound_group_read(entity_text,entity_bytes,name,
            text,entry.size,&campaign_foley,scene_npc_use_groups+i);
        if(status){++rf_scene_npc_use_dialogue[3];continue;}
        rf_scene_npc_use_dialogue[2]+=scene_npc_use_groups[i]>=0;
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
}
static void scene_npc_use_dialogue(uint32_t handle)
{
    campaign_npc_body *owner=NULL;scene_message_voice *slot=NULL;
    campaign_pain_audio_context audio={&scene_npc_use_random,0,rf_scene_npc_use_audio,0};
    const rf_sound_metadata *metadata;const rf_wave_pcm *pcm;
    uint32_t i,cls,uid,voice_handle,before_bytes;int32_t group,sample,voice;int status=RF_OK;
    ++rf_scene_npc_use_dialogue[4];rf_scene_npc_use_dialogue[11]=handle;
    rf_scene_npc_use_dialogue[15]=0;
    for(i=0;i<campaign_npc_body_count;i++)if(scene_medic_live(campaign_npc_bodies+i) &&
        campaign_npc_bodies[i].registration.handle==handle){owner=campaign_npc_bodies+i;break;}
    if(!owner || i>=campaign_seeds.records.count)goto ineligible;
    cls=campaign_seeds.items[i].class_index;uid=(uint32_t)campaign_seeds.records.items[i].record.uid;
    rf_scene_npc_use_dialogue[10]=uid;
    if(cls>=campaign_seeds.class_count || campaign_seeds.classes[cls].physics.use_kind!=9 ||
       !(campaign_seeds.classes[cls].physics.flags&0x20000u) ||
       !owner->damage.effects.affiliation || !isfinite(owner->damage.effects.health) || owner->damage.effects.health<=0 ||
       (owner->object_flags&(2u|0x4000u)) || (owner->view.flags_810&1u) ||
       ((owner->view.flags_814|owner->damage.effects.flags_814)&0x200u) ||
       (owner->ai_mode.action_280!=2 && owner->ai_mode.action_280!=4) ||
       owner->combat_alert || owner->combat_scripted || owner->script_animation.active ||
       owner->script_look.active || owner->script_shoot.active)goto ineligible;
    if(!scene_npc_use_groups || cls>=scene_npc_use_group_count ||
       (group=scene_npc_use_groups[cls])<0){++rf_scene_npc_use_dialogue[8];return;}
    rf_scene_npc_use_dialogue[12]=(uint32_t)group;
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
    ++rf_scene_npc_use_audio[0];sample=campaign_pain_audio_resolve(&audio,group);
    rf_scene_npc_use_audio[6]=scene_npc_use_random.value;
    if(audio.status){status=audio.status;goto failed;}
    rf_scene_npc_use_dialogue[13]=(uint32_t)sample;
    if(sample==-1){++rf_scene_npc_use_dialogue[8];return;}
    if(sample<0 || (uint32_t)sample>=campaign_audio_bank.count){status=RF_FORMAT;goto failed;}
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
    ++rf_scene_npc_use_dialogue[5];rf_scene_npc_use_dialogue[14]=(uint32_t)voice;
    ++rf_scene_npc_use_audio[2];
    if(campaign_audio_bank.bytes>before_bytes){++rf_scene_npc_use_audio[3];
        rf_scene_npc_use_audio[4]+=campaign_audio_bank.bytes-before_bytes;}
    return;
ineligible:
    ++rf_scene_npc_use_dialogue[7];return;
failed:
    ++rf_scene_npc_use_dialogue[9];++rf_scene_npc_use_audio[7];
    rf_scene_npc_use_dialogue[15]=(uint32_t)status;
    /* Optional speech failure leaves Use consumed and gameplay untouched. */
}
#endif
