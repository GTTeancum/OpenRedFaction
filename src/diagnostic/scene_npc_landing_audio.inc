#ifndef RF_SCENE_NPC_LANDING_AUDIO_INCLUDED
#define RF_SCENE_NPC_LANDING_AUDIO_INCLUDED
/*419839..4198e2: relative-Y>.25 OR full speed>.5, material/water group,
 *434d00(group,0), then48a9c0's spatial5056a0 branch for an NPC. The scene
 *already owns landing damage/support/stance/publication. Observe that one
 *successful surviving landing; never call the complete419830 path again.
 *The later next-actor-boundary timing and accepted point-support velocity are
 *explicit port adaptations. No random draw, loop, save state or new PCM pool. */
static int32_t (*scene_npc_landing_groups)[10];
static uint32_t scene_npc_landing_group_count,scene_npc_landing_epoch;
/* classes,bytes,binding hash,load failures,captures,accepted,commits,requests,
 *starts,quiet,stale,failures,UID,handle,material,group,sample,voice,frame,status */
uint32_t rf_scene_npc_landing_audio[20];
float rf_scene_npc_landing_values[11]; /* incoming XYZ,accepted support XYZ,speed,relative Y,published XYZ */
typedef struct scene_npc_landing_receipt {
    const campaign_npc_body *owner;
    uint32_t slot,handle,uid,cls,epoch,armed,accepted,committed,support_handle,water;
    int32_t material;
    float incoming[3],support_velocity[3],position[3];
} scene_npc_landing_receipt;

static void scene_npc_landing_audio_close(void)
{
    free(scene_npc_landing_groups);scene_npc_landing_groups=NULL;
    scene_npc_landing_group_count=0;++scene_npc_landing_epoch;
    rf_scene_npc_landing_audio[0]=rf_scene_npc_landing_audio[1]=0;
}
static void scene_npc_landing_audio_reset(void)
{
    scene_npc_landing_audio_close();
    memset(rf_scene_npc_landing_audio,0,sizeof(rf_scene_npc_landing_audio));
    memset(rf_scene_npc_landing_values,0,sizeof(rf_scene_npc_landing_values));
    for(uint32_t i=12;i<19;i++)rf_scene_npc_landing_audio[i]=UINT32_MAX;
}

/* Optional metadata only. The existing Foley bank already declared samples.
 *Keep every class row all-absent on malformed bindings; never reject loading
 *or partially publish a broken material map. Existing class bound:640 rows,
 *at most25,600 additional retained bytes, with no bank budget increase. */
static void scene_npc_landing_audio_load(const void *text,uint32_t bytes)
{
    uint32_t count=campaign_seeds.class_count,i,j;
    scene_npc_landing_audio_close();
    if(!count)return;
    if(count>640 || !text || !bytes){++rf_scene_npc_landing_audio[3];return;}
    scene_npc_landing_groups=malloc(count*sizeof(*scene_npc_landing_groups));
    if(!scene_npc_landing_groups){++rf_scene_npc_landing_audio[3];return;}
    memset(scene_npc_landing_groups,0xff,count*sizeof(*scene_npc_landing_groups));
    for(i=0;i<count;i++) {
        int32_t groups[10];const char *name;int status;
        status=scene_turret_class_name(&campaign_seeds,i,&name);
        if(!status)status=rf_entity_land_sound_groups_read(text,bytes,name,&campaign_foley,groups);
        for(j=0;!status && j<10;j++)if(groups[j]>=0) {
            const rf_foley_group *group;
            if(!campaign_foley.groups || (uint32_t)groups[j]>=campaign_foley.group_count){status=RF_RANGE;break;}
            group=campaign_foley.groups+groups[j];
            if(group->first>campaign_foley.sample_count ||
               group->count>campaign_foley.sample_count-group->first)status=RF_FORMAT;
        }
        if(status){++rf_scene_npc_landing_audio[3];continue;}
        memcpy(scene_npc_landing_groups[i],groups,sizeof(groups));
    }
    scene_npc_landing_group_count=count;
    rf_scene_npc_landing_audio[0]=count;
    rf_scene_npc_landing_audio[1]=count*sizeof(*scene_npc_landing_groups);
    rf_scene_npc_landing_audio[2]=npc_hash_bytes(2166136261u,scene_npc_landing_groups,rf_scene_npc_landing_audio[1]);
    rf_scene_foley[3]+=rf_scene_npc_landing_audio[1];
}

/* Resolve from the captured full generation and original slot, never from a
 *UID alone. Compare the array pointer BEFORE dereferencing a saved owner. */
static campaign_npc_body *scene_npc_landing_audio_owner(const scene_npc_landing_receipt *r)
{
    campaign_npc_body *owner;
    if(!r->armed || r->epoch!=scene_npc_landing_epoch || !scene_npc_landing_groups ||
       !campaign_npc_bodies || !campaign_seeds.items || !campaign_seeds.records.items ||
       r->slot>=campaign_npc_body_count || r->slot>=campaign_seeds.records.count ||
       r->cls>=scene_npc_landing_group_count || r->cls>=campaign_seeds.class_count ||
       campaign_npc_bodies+r->slot!=r->owner)return NULL;
    owner=campaign_npc_bodies+r->slot;
    if(campaign_seeds.items[r->slot].class_index!=r->cls ||
       (uint32_t)campaign_seeds.records.items[r->slot].record.uid!=r->uid ||
       !r->handle || r->handle==UINT32_MAX || owner->registration.handle!=r->handle ||
       owner->registration.view!=&owner->view || owner->registration.object_kind!=0 ||
       (uint32_t)owner->view.handle!=r->handle || owner->view.type!=0 ||
       owner->damage.effects.handle!=r->handle || !owner->body.allocated_bytes ||
       rf_object_registry_lookup(&campaign_registry,r->handle)!=&owner->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)r->handle)!=&owner->view ||
       (owner->object_flags&(2u|8u|0x4000u)) || (owner->view.flags_810&1u) ||
       !isfinite(owner->damage.effects.health) || owner->damage.effects.health<=0)return NULL;
    return owner;
}
static void scene_npc_landing_audio_capture(scene_npc_landing_receipt *r,
    const campaign_npc_body *owner,uint32_t slot)
{
    memset(r,0,sizeof(*r));
    if(!scene_npc_landing_groups || !campaign_npc_bodies || !campaign_seeds.items ||
       !campaign_seeds.records.items || slot>=campaign_npc_body_count ||
       slot>=campaign_seeds.records.count || owner!=campaign_npc_bodies+slot)return;
    r->owner=owner;r->slot=slot;r->handle=owner->registration.handle;
    r->uid=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    r->cls=campaign_seeds.items[slot].class_index;r->epoch=scene_npc_landing_epoch;r->armed=1;
    if(!scene_npc_landing_audio_owner(r)){r->armed=0;return;}
    memcpy(r->incoming,owner->body.state.velocity,12);++rf_scene_npc_landing_audio[4];
}
static void scene_npc_landing_audio_accept(scene_npc_landing_receipt *r)
{
    campaign_npc_body *owner=scene_npc_landing_audio_owner(r);
    if(!owner){r->armed=0;return;}
    memcpy(r->support_velocity,owner->support_velocity,12);
    memcpy(r->position,owner->body.state.position,12);
    r->support_handle=owner->support.handle;r->material=owner->support.material;
    r->water=owner->view.flags_810&0x1000u;r->accepted=1;
    ++rf_scene_npc_landing_audio[5];
}
static campaign_npc_body *scene_npc_landing_audio_matches(const scene_npc_landing_receipt *r)
{
    campaign_npc_body *owner=scene_npc_landing_audio_owner(r);
    if(!owner || !r->accepted || owner->support.handle!=r->support_handle ||
       owner->support.material!=r->material || (owner->view.flags_810&0x1000u)!=r->water ||
       memcmp(owner->body.state.position,r->position,12) ||
       memcmp(owner->support_velocity,r->support_velocity,12))return NULL;
    return owner;
}

/* Record successful surviving publication without invoking any audio backend.
 *The outer script pass owns one stack receipt; it consumes this only after
 *the current actor iteration has finished all gameplay/model owner accesses. */
static void scene_npc_landing_audio_publish(scene_npc_landing_receipt *r)
{
    campaign_npc_body *owner;
    if(!r->armed)return;
    owner=scene_npc_landing_audio_matches(r);
    if(!owner || memcmp(owner->published,r->position,12)) {
        r->armed=0;++rf_scene_npc_landing_audio[10];return;
    }
    r->committed=1;
}

/* Retain the full native handle BEFORE play_mode can call out. Unlike a
 *discarded generic public ID, this lets a stale/replaced actor cancel only
 *the voice just attempted. Ordinary successful one-shots finish naturally. */
static int scene_npc_landing_audio_start(const scene_npc_landing_receipt *r,
    uint32_t sample,const float position[3],uint32_t *started)
{
    const rf_wave_pcm *pcm;campaign_spatial_voice *voice;float gains[2];
    rf_scene_audio_events events;void *context;uint32_t handle,index;int status,attempted=0;
    *started=0;
    if(!rf_audio_bank_sample(&campaign_audio_bank,sample)) {
        /* Direct bounded bank admission has no external idle-release callback.
         *A full bank stays optionally silent; do not enlarge it or evict PCM
         *through a reentrant callback while holding this landing receipt. */
        status=rf_audio_bank_reload(&campaign_audio_bank,&campaign_audio_archive,sample);
        if(status)return status;
        if(sample>=campaign_audio_bank.count || sample>=sizeof(campaign_audio_evictable))return RF_RANGE;
        if(sample!=0x53u)campaign_audio_evictable[sample]=1;
        ++rf_scene_sound_bank[1];rf_scene_sound_bank[2]+=campaign_audio_bank.samples[sample].bytes;
        rf_scene_live_audio[1]=campaign_audio_bank.bytes;
    }
    if(!scene_npc_landing_audio_owner(r))return RF_NOT_FOUND;
    pcm=rf_audio_bank_sample(&campaign_audio_bank,sample);if(!pcm)return RF_NOT_FOUND;
    status=rf_audio_voice_start(&campaign_audio_mixer,pcm,32768,32768,0,&handle);if(status)return status;
    index=handle&0xffffu;voice=campaign_spatial_voices+index;
    memset(voice,0,sizeof(*voice));voice->handle=handle;voice->sample=sample;
    voice->volume=1;voice->positional=1;memcpy(voice->position,position,12);
    voice->last_volume=voice->last_pan=INT32_MIN;
    campaign_spatial_update(voice,1); /* initial update never calls gain */
    status=rf_audio_device_gains(voice->last_volume,voice->last_pan,gains);
    events=campaign_audio_events;context=campaign_audio_events_context;
    if(!status && ((events.play && !events.play_mode) ||
       (events.play_mode && !events.stop)))status=RF_IO;
    if(!status && !scene_npc_landing_audio_owner(r))status=RF_NOT_FOUND;
    if(!status && events.play_mode) {
        attempted=1;status=events.play_mode(context,handle,pcm,gains[0],gains[1],0);
    }
    if(!status && (!scene_npc_landing_audio_owner(r) ||
       campaign_audio_mixer.voices[index].handle!=handle ||
       campaign_spatial_voices[index].handle!=handle ||
       campaign_audio_events_context!=context || campaign_audio_events.play_mode!=events.play_mode ||
       campaign_audio_events.stop!=events.stop))status=RF_NOT_FOUND;
    if(!status){*started=handle;++rf_scene_live_audio[4];return RF_OK;}
    /* Native lifetime can outlast a software slot: stop the retained full
     *handle in the same epoch, independently of current slot ownership.
     *No gameplay state is dereferenced or published after this callback. */
    if(r->epoch==scene_npc_landing_epoch) {
        if(attempted && events.stop && campaign_audio_events_context==context &&
           campaign_audio_events.stop==events.stop)events.stop(context,handle);
        /* A stop callback can itself replace the bank/timeline. */
        if(r->epoch!=scene_npc_landing_epoch)return status;
        (void)rf_audio_voice_stop(&campaign_audio_mixer,handle);
        if(campaign_spatial_voices[index].handle==handle)memset(campaign_spatial_voices+index,0,sizeof(*voice));
    }
    ++rf_scene_live_audio[5];return status;
}

/* Consume at the next per-actor iteration boundary, before acquiring another
 *owner pointer. Use the copied landing position, even if ordinary scripted
 *locomotion advanced the same actor afterward. No actor writes follow audio.
 *Impact/death/stance failures never mark committed. Optional errors stay here. */
static void scene_npc_landing_audio_commit(scene_npc_landing_receipt *r,uint32_t frame)
{
    campaign_npc_body *owner;const rf_foley_group *group;const rf_sound_metadata *metadata;
    int32_t group_id=-1,sample=-1;uint32_t slot=0,voice=0,i;int status=RF_OK;
    double relative,speed;float position[3];
    if(!r->armed || !r->committed){r->armed=0;return;}
    owner=scene_npc_landing_audio_owner(r);
    if(!owner){++rf_scene_npc_landing_audio[10];goto done;}
    ++rf_scene_npc_landing_audio[6];
    rf_scene_npc_landing_audio[12]=r->uid;rf_scene_npc_landing_audio[13]=r->handle;
    rf_scene_npc_landing_audio[14]=(uint32_t)r->material;rf_scene_npc_landing_audio[18]=frame;
    memcpy(position,r->position,12);
    memcpy(rf_scene_npc_landing_values,r->incoming,12);
    memcpy(rf_scene_npc_landing_values+3,r->support_velocity,12);
    memcpy(rf_scene_npc_landing_values+8,position,12);
    for(i=0;i<3;i++)if(!isfinite(r->incoming[i]) || !isfinite(r->support_velocity[i]) ||
       !isfinite(position[i])){status=RF_FORMAT;goto done;}
    relative=(double)r->support_velocity[1]-r->incoming[1];
    speed=sqrt(((double)r->incoming[0]*r->incoming[0]+(double)r->incoming[1]*r->incoming[1])+
        (double)r->incoming[2]*r->incoming[2]);
    rf_scene_npc_landing_values[6]=(float)speed;rf_scene_npc_landing_values[7]=(float)relative;
    if(!(relative>.25 || speed>.5)){++rf_scene_npc_landing_audio[9];goto done;}
    ++rf_scene_npc_landing_audio[7];
    if(r->material>=10){status=RF_RANGE;goto done;}
    if(r->material>=0 && scene_npc_landing_groups[r->cls][r->material]>0)slot=(uint32_t)r->material;
    if(r->water && scene_npc_landing_groups[r->cls][4]>0)slot=4;
    group_id=scene_npc_landing_groups[r->cls][slot];
    if(group_id<0){++rf_scene_npc_landing_audio[9];goto done;}
    if(!campaign_foley.groups || (uint32_t)group_id>=campaign_foley.group_count){status=RF_RANGE;goto done;}
    group=campaign_foley.groups+group_id;
    if(!group->count){++rf_scene_npc_landing_audio[9];goto done;}
    if(!campaign_foley.samples || group->first>=campaign_foley.sample_count ||
       group->count>campaign_foley.sample_count-group->first){status=RF_FORMAT;goto done;}
    sample=campaign_foley.samples[group->first]; /*434d00(group,0), never RNG. */
    if(sample<0){++rf_scene_npc_landing_audio[9];goto done;}
    if(!campaign_audio_bank.samples || (uint32_t)sample>=campaign_audio_bank.count ||
       (uint32_t)sample>=sizeof(campaign_audio_evictable)){status=RF_RANGE;goto done;}
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    if(metadata && (metadata->loop_flags&0x40000000u)){status=RF_FORMAT;goto done;}
    status=scene_npc_landing_audio_start(r,(uint32_t)sample,position,&voice);
    if(!status)++rf_scene_npc_landing_audio[8];
done:
    r->armed=0;
    rf_scene_npc_landing_audio[15]=(uint32_t)group_id;rf_scene_npc_landing_audio[16]=(uint32_t)sample;
    rf_scene_npc_landing_audio[17]=voice;rf_scene_npc_landing_audio[19]=(uint32_t)status;
    if(status)++rf_scene_npc_landing_audio[11];
}
#endif
