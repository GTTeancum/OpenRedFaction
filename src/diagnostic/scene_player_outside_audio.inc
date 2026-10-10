#ifndef RF_SCENE_PLAYER_OUTSIDE_AUDIO_INCLUDED
#define RF_SCENE_PLAYER_OUTSIDE_AUDIO_INCLUDED
/* Original4a2700/4a2997 ->4a2a60 owns repeated outside gasps separately from
 * damage421170: living actor, room+42 EXACTLY1, current armor<=0. No class,
 * mutant, invulnerability, liquid, suit-bit or applied-damage-result gate.
 * 4a2a9c polls505c00, then505560(50,0,0,1) only after real completion.
 * Gasp_01.wav is NONLOOP in Bluebeard; never invent a timer or force a loop.
 * Include after scene_player_outside_damage.inc. The shared ordinary on-foot
 * exclusions and current body-room phase are explicitly bounded port scope. */
#define SCENE_PLAYER_OUTSIDE_AUDIO_PCM_LIMIT (96u*1024u)
typedef struct scene_player_outside_audio_state {
    uint32_t owner,handle,admitted,failed;
    int32_t sample,voice;
} scene_player_outside_audio_state;
static scene_player_outside_audio_state scene_player_outside_audio={0,0,0,0,-1,-1};
/* ticks,eligible,starts,stops,completions,native polls,software polls,
 * resource failures,start refusals,resource state(0/ready1/failed2),sample,
 * public voice,full voice handle,player handle,room,frame,status,file bytes. */
uint32_t rf_scene_player_outside_audio[18];

static void scene_player_outside_audio_forget(void)
{
    scene_player_outside_audio_state *s=&scene_player_outside_audio;
    uint32_t handle=s->handle,index=handle&0xffffu;
    /* Retire first; the public ID/software slot may have been reused while
     * hardware still plays. Native stop MUST receive our retained full handle.
     * Each software/spatial cleanup independently qualifies its generation. */
    s->handle=s->owner=0;s->voice=-1;
    rf_scene_player_outside_audio[11]=UINT32_MAX;
    rf_scene_player_outside_audio[12]=rf_scene_player_outside_audio[13]=0;
    if(!handle)return;
    if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
    (void)rf_audio_voice_stop(&campaign_audio_mixer,handle);
    if(index<RF_AUDIO_VOICES && campaign_spatial_voices[index].handle==handle)
        memset(campaign_spatial_voices+index,0,sizeof(*campaign_spatial_voices));
    ++rf_scene_player_outside_audio[3];
}

/* Bank/timeline ownership is separate: relocation/load/death forget the
 * voice, while a rebuilt bank also invalidates success/failure admission. */
static void scene_player_outside_audio_close(void)
{
    scene_player_outside_audio_forget();
    scene_player_outside_audio.admitted=scene_player_outside_audio.failed=0;
    scene_player_outside_audio.sample=-1;
    rf_scene_player_outside_audio[9]=rf_scene_player_outside_audio[17]=0;
    rf_scene_player_outside_audio[10]=UINT32_MAX;
}
static void scene_player_outside_audio_reset(void)
{
    scene_player_outside_audio_close();
    memset(rf_scene_player_outside_audio,0,sizeof(rf_scene_player_outside_audio));
    rf_scene_player_outside_audio[10]=rf_scene_player_outside_audio[11]=
        rf_scene_player_outside_audio[14]=rf_scene_player_outside_audio[15]=UINT32_MAX;
}
static uint32_t scene_player_outside_audio_active(void)
{
    uint32_t handle=scene_player_outside_audio.handle,index=handle&0xffffu;
    if(!handle)return 0;
    /* Physical playback has its own clock. Never fall back to the mixer when
     * a native callback exists, including when it reports completion. */
    if(campaign_audio_events.playing) {
        ++rf_scene_player_outside_audio[5];
        return campaign_audio_events.playing(campaign_audio_events_context,handle)!=0;
    }
    ++rf_scene_player_outside_audio[6];
    return index<RF_AUDIO_VOICES && campaign_audio_mixer.voices[index].handle==handle &&
        campaign_audio_mixer.voices[index].active;
}

static uint32_t scene_player_outside_audio_eligible(scene_stream *stream,uint32_t *room_index)
{
    const rf_geometry *geometry;rf_collision_room_location room;
    uint32_t handle=campaign_player_object.handle,offset,i;
    if(!campaign_spawn || !stream || !scene_actor_body.allocated_bytes ||
       !handle || handle==UINT32_MAX ||
       campaign_player_object.object_kind!=0 || campaign_player_object.view!=&campaign_player_view ||
       (uint32_t)campaign_player_view.handle!=handle || campaign_player_view.type!=0 ||
       campaign_player_damage.state.effects.handle!=handle ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&campaign_player_object ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&campaign_player_view ||
       !(campaign_player_view.flags_7c&8u) || (campaign_player_view.flags_7c&(2u|0x4000u)) ||
       (campaign_player_view.flags_810&1u) ||
       !isfinite(campaign_player_damage.state.effects.health) ||
       !(campaign_player_damage.state.effects.health>0) ||
       !isfinite(campaign_player_damage.state.effects.armor) ||
       campaign_player_damage.state.effects.armor>0 ||
       campaign_player_view.linked_handle!=-1 || campaign_cutscene_runtime.active ||
       campaign_defuse.active || scene_driller_active(stream) || scene_turret_player_active())return 0;
    for(i=0;i<3;i++)if(!isfinite(scene_actor_body.state.position[i]))return 0;
    geometry=stream->geometry;
    if(!stream->collision || !geometry || !geometry->data || !geometry->room_offsets)return 0;
    /* Slot0 is the current BODY, never the listener/camera or water owner. */
    if(scene_player_room_locate(stream,0,scene_actor_body.state.position,&room))return 0;
    if(room.room>=stream->collision->room_count || room.room>=geometry->rooms)return 0;
    offset=geometry->room_offsets[room.room];
    if(offset>geometry->bytes || geometry->bytes-offset<40)return 0;
    if(geometry->data[offset+30]!=1)return 0;
    *room_index=room.room;return 1;
}

/* Closure-only ordinary-frame reconciliation also handles boarding,
 * removed/replaced actors, modal/cinematic ownership and late NPC damage.
 * Never call this or forget from shared checkpoint candidate preparation. */
static void scene_player_outside_audio_reconcile(scene_stream *stream)
{
    uint32_t room;
    if(scene_player_outside_audio.handle &&
       (scene_player_outside_audio.owner!=campaign_player_object.handle ||
        !scene_player_outside_audio_eligible(stream,&room)))scene_player_outside_audio_forget();
}

/* Lazy optional admission shares the existing bank and native-safe idle
 * eviction. A resource refusal is cached for this level; a voice refusal is
 * not. No pinned PCM, bank expansion, replacement sample or gameplay error. */
static uint32_t scene_player_outside_audio_prepare(void)
{
    scene_player_outside_audio_state *s=&scene_player_outside_audio;
    const rf_sound_metadata *metadata;const rf_wave_pcm *pcm;rf_vpp_entry entry;
    uint32_t sample;int status=RF_OK;
    if(s->failed)return 0;
    if(!s->admitted) {
        if(!campaign_audio_archive.stream || campaign_audio_bank.archive!=&campaign_audio_archive ||
           !campaign_audio_bank.samples || campaign_audio_bank.count>sizeof(campaign_audio_evictable)) {
            status=RF_RANGE;goto failed;
        }
        /* sounds.tbl50: near5,gain.6,rolloff1. Declare by name, preserving an
         * existing authored row and applying its gain once in flat playback. */
        status=rf_audio_bank_declare(&campaign_audio_bank,"gasp_01.wav",5,.6f,1,&sample);
        if(status)goto failed;
        s->sample=(int32_t)sample;rf_scene_player_outside_audio[10]=sample;
        if(sample>=sizeof(campaign_audio_evictable)){status=RF_RANGE;goto failed;}
        metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
            campaign_audio_bank.samples[sample].name);
        if(!metadata || (metadata->loop_flags&0x47ffffffu) ||
           (metadata->keyoff_flags&0x20000000u)){status=RF_FORMAT;goto failed;}
        status=rf_vpp_find(&campaign_audio_archive,campaign_audio_bank.samples[sample].name,&entry);
        if(status)goto failed;
        if(!entry.size){status=RF_FORMAT;goto failed;}
        if(entry.size>SCENE_PLAYER_OUTSIDE_AUDIO_PCM_LIMIT){status=RF_RANGE;goto failed;}
        rf_scene_player_outside_audio[17]=entry.size;s->admitted=1;
    }
    sample=(uint32_t)s->sample;
    if(sample>=campaign_audio_bank.count){status=RF_RANGE;goto failed;}
    pcm=rf_audio_bank_sample(&campaign_audio_bank,sample);
    if(!pcm) {
        status=campaign_ambient_reload(sample);if(status)goto failed;
        campaign_audio_evictable[sample]=1;
        ++rf_scene_sound_bank[1];rf_scene_sound_bank[2]+=campaign_audio_bank.samples[sample].bytes;
        rf_scene_live_audio[1]=campaign_audio_bank.bytes;
        pcm=rf_audio_bank_sample(&campaign_audio_bank,sample);
    }
    /* Match the shared mixer/native PCM contract before start, so unsupported
     * data cannot masquerade as retryable voice exhaustion every frame. */
    if(!pcm || !pcm->samples || !pcm->frames || !pcm->rate || pcm->rate>192000 ||
       (pcm->channels!=1 && pcm->channels!=2) || (pcm->bits!=8 && pcm->bits!=16) ||
       (uint64_t)pcm->frames*pcm->channels*(pcm->bits/8)!=pcm->bytes) {
        status=RF_FORMAT;goto failed;
    }
    if(campaign_audio_bank.samples[sample].bytes>SCENE_PLAYER_OUTSIDE_AUDIO_PCM_LIMIT) {
        status=RF_RANGE;goto failed;
    }
    rf_scene_player_outside_audio[9]=1;return 1;
failed:
    s->failed=1;++rf_scene_player_outside_audio[7];
    rf_scene_player_outside_audio[9]=2;rf_scene_player_outside_audio[16]=(uint32_t)status;
    printf("PLAYER_OUTSIDE_AUDIO_UNAVAILABLE sample%d status%d bytes%u\n",s->sample,status,
        rf_scene_player_outside_audio[17]);
    return 0;
}

static void scene_player_outside_audio_tick(scene_stream *stream,uint32_t frame)
{
    scene_player_outside_audio_state *s=&scene_player_outside_audio;
    const float centered[3]={0,0,0};uint32_t room,handle;int32_t voice;int status;
    ++rf_scene_player_outside_audio[0];rf_scene_player_outside_audio[15]=frame;
    rf_scene_player_outside_audio[14]=UINT32_MAX;
    if(!s->failed)rf_scene_player_outside_audio[16]=RF_OK;
    if(s->owner!=campaign_player_object.handle)scene_player_outside_audio_forget();
    /* Read life/room/armor independently AFTER liquid and outside damage.
     * Lethal damage closes here; invulnerable/nonhumanoid owners can gasp. */
    if(!scene_player_outside_audio_eligible(stream,&room)){scene_player_outside_audio_forget();return;}
    ++rf_scene_player_outside_audio[1];rf_scene_player_outside_audio[14]=room;
    if(scene_player_outside_audio_active())return;
    if(s->handle){++rf_scene_player_outside_audio[4];scene_player_outside_audio_forget();}
    if(!scene_player_outside_audio_prepare())return;
    voice=campaign_sound_start(s->sample,centered,1,0,0,0);
    if(voice<0){++rf_scene_player_outside_audio[8];rf_scene_player_outside_audio[16]=RF_RANGE;return;}
    /* No intervening callback or mixer operation: capture the complete native
     * identity immediately, then never resolve the public ID for lifetime. */
    status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,voice,&handle);
    if(status){++rf_scene_player_outside_audio[8];rf_scene_player_outside_audio[16]=(uint32_t)status;return;}
    s->voice=voice;s->handle=handle;s->owner=campaign_player_object.handle;
    ++rf_scene_player_outside_audio[2];rf_scene_player_outside_audio[11]=(uint32_t)voice;
    rf_scene_player_outside_audio[12]=handle;rf_scene_player_outside_audio[13]=s->owner;
}
#endif
