#ifndef RF_SCENE_NPC_LOOP_AUDIO_INCLUDED
#define RF_SCENE_NPC_LOOP_AUDIO_INCLUDED
/* Include after combat_sound and registered NPC/audio owners. Original
 *4269c1..4269fb enters41a870 for continuous fire;41a9bd guards actor81c and
 *41aa3d..41aa95 retains its positional Launch voice. This adapter observes
 * the port's accepted primary shots and actual cooldown branch only. It
 * neither creates a held trigger nor changes AI, cadence, ammo or saves.
 * Begin/end surround the entire enemy pass, including skips and errors. */
typedef struct scene_npc_loop_audio_state {
    uint32_t owner,slot,uid,source,seen;int32_t weapon,voice,sample;
} scene_npc_loop_audio_state;
static scene_npc_loop_audio_state scene_npc_loop_audio[RF_AUDIO_VOICES];
uint32_t rf_scene_npc_loop_audio[16]; /* calls,starts,stops,retained,failures,stale,moves,owners,peak,full,status,bytes,shots,single suppressed,invalid owner,closes */

static uint32_t scene_npc_loop_audio_weapon(int32_t weapon)
{
    /* These IDs were resolved from the immutable weapon catalog before
     * startup events. NPC gameplay currently supports PRIMARY fire only;
     * Machine Pistol Special and HMG alternate are not fabricated here. */
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=campaign_weapon_supply.names.count)return 0;
    return weapon==campaign_extra_ids[0]?1u:weapon==campaign_extra_ids[1]?2u:0u;
}
static campaign_npc_body *scene_npc_loop_audio_owner(uint32_t slot,uint32_t *uid)
{
    campaign_npc_body *owner;uint32_t handle;
    if(!campaign_npc_bodies || !campaign_seeds.records.items ||
       slot>=campaign_npc_body_count || slot>=campaign_seeds.records.count)return NULL;
    owner=campaign_npc_bodies+slot;handle=owner->registration.handle;
    if(!handle || handle==UINT32_MAX || owner->registration.view!=&owner->view ||
       (uint32_t)owner->view.handle!=handle || owner->damage.effects.handle!=handle ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view ||
       !isfinite(owner->damage.effects.health) || owner->damage.effects.health<=0 ||
       (owner->object_flags&(2u|0x4000u)) || (owner->view.flags_810&1u))return NULL;
    *uid=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    if(*uid>INT32_MAX)return NULL;
    return owner;
}
static void scene_npc_loop_audio_publish(void)
{
    uint32_t count=0;
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)count+=scene_npc_loop_audio[i].owner!=0;
    rf_scene_npc_loop_audio[7]=count;
    if(count>rf_scene_npc_loop_audio[8])rf_scene_npc_loop_audio[8]=count;
    rf_scene_npc_loop_audio[11]=sizeof(scene_npc_loop_audio)+sizeof(rf_scene_npc_loop_audio);
}
static void scene_npc_loop_audio_release(scene_npc_loop_audio_state *s)
{
    uint32_t source;
    if(!s->owner)return;
    if(s->voice>=0) {
        /* Retain both identities. An ID-map reset or recycled mixer slot
         * cannot authorize stopping a different full mixer generation. */
        if(!rf_audio_voice_ids_resolve(&campaign_device_voice_ids,
            &campaign_audio_mixer,s->voice,&source) && source==s->source) {
            rf_audio_voice_stop(&campaign_audio_mixer,source);
            if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,source);
            if(campaign_spatial_voices[source&0xffffu].handle==source)
                memset(campaign_spatial_voices+(source&0xffffu),0,sizeof(*campaign_spatial_voices));
            ++rf_scene_npc_loop_audio[2];
        } else ++rf_scene_npc_loop_audio[5];
    }
    memset(s,0,sizeof(*s));++rf_scene_npc_loop_audio[15];scene_npc_loop_audio_publish();
}
static void scene_npc_loop_audio_remove(uint32_t handle)
{
    /* Caller passes the retiring/relocating full NPC generation, never its
     * low registry index or authored UID. This does not acquire a new owner. */
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)
        if(scene_npc_loop_audio[i].owner==handle)scene_npc_loop_audio_release(scene_npc_loop_audio+i);
}
static void scene_npc_loop_audio_close(void)
{
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)scene_npc_loop_audio_release(scene_npc_loop_audio+i);
}
static void scene_npc_loop_audio_reset(void)
{
    scene_npc_loop_audio_close();memset(rf_scene_npc_loop_audio,0,sizeof(rf_scene_npc_loop_audio));
    scene_npc_loop_audio_publish();
}
static void scene_npc_loop_audio_begin(void)
{
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)scene_npc_loop_audio[i].seen=0;
}
static void scene_npc_loop_audio_end(void)
{
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++) {
        scene_npc_loop_audio_state *s=scene_npc_loop_audio+i;
        campaign_npc_body *owner;uint32_t uid=0;
        if(!s->owner)continue;
        owner=s->seen?scene_npc_loop_audio_owner(s->slot,&uid):NULL;
        /* A shooter can die after its own iteration, including from another
         * NPC. Requalify the real source instead of trusting its seen mark. */
        if(!owner || owner->registration.handle!=s->owner || uid!=s->uid ||
           owner->view.weapons[0]!=s->weapon)scene_npc_loop_audio_release(s);
    }
}
static uint32_t scene_npc_loop_audio_observe(uint32_t slot,uint32_t fired,uint32_t once)
{
    scene_npc_loop_audio_state *s=NULL,*free_slot=NULL;campaign_npc_body *owner;
    campaign_pain_audio_context audio={&combat_sound_random,0,rf_scene_weapon_audio,0};
    const rf_sound_metadata *metadata;uint32_t uid=0,launch,source,requested=0;
    int32_t weapon,group,sample;int status=RF_OK;
    ++rf_scene_npc_loop_audio[0];rf_scene_npc_loop_audio[12]+=fired!=0;
    owner=scene_npc_loop_audio_owner(slot,&uid);
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++) {
        scene_npc_loop_audio_state *candidate=scene_npc_loop_audio+i;
        if(candidate->owner && candidate->slot==slot) {
            if(!owner || candidate->owner!=owner->registration.handle || candidate->uid!=uid)
                scene_npc_loop_audio_release(candidate);
            else s=candidate;
        }
        if(!candidate->owner && !free_slot)free_slot=candidate;
    }
    if(!owner){++rf_scene_npc_loop_audio[14];status=RF_NOT_FOUND;goto done;}
    weapon=owner->view.weapons[0];launch=scene_npc_loop_audio_weapon(weapon);
    if(s && (once || s->weapon!=weapon || !launch)) {
        scene_npc_loop_audio_release(s);free_slot=s;s=NULL;
    }
    /* A queued single shot bypasses cadence and has no continuous held
     * lifetime. End its former episode but do not invent a sound deadline
     * or turn the installed loop into an unauthored one-shot. */
    if(once){++rf_scene_npc_loop_audio[13];goto done;}
    if(!launch)goto done;
    for(uint32_t i=0;i<3;i++)if(!isfinite(owner->eye_position[i])) {
        if(s)scene_npc_loop_audio_release(s);status=RF_FORMAT;goto done;
    }
    if(s) {
        s->seen=1;++rf_scene_npc_loop_audio[3];
        if(s->voice>=0) {
            if(rf_audio_voice_ids_resolve(&campaign_device_voice_ids,
                &campaign_audio_mixer,s->voice,&source) || source!=s->source) {
                s->voice=-1;++rf_scene_npc_loop_audio[5];
            } else if(campaign_spatial_voices[source&0xffffu].handle==source) {
                campaign_spatial_voice *spatial=campaign_spatial_voices+(source&0xffffu);
                memcpy(spatial->position,owner->eye_position,12);
                campaign_spatial_update(spatial,0);++rf_scene_npc_loop_audio[6];
            }
        }
        goto done; /* One attempt per episode, including unavailable/stolen voices. */
    }
    if(!fired)goto done; /* Cooldown never starts a voice or retries failed admission. */
    if(!free_slot){++rf_scene_npc_loop_audio[9];status=RF_RANGE;goto done;}
    s=free_slot;s->owner=owner->registration.handle;s->slot=slot;s->uid=uid;
    s->weapon=weapon;s->voice=s->sample=-1;s->source=0;s->seen=1;
    scene_npc_loop_audio_publish();requested=1;++rf_scene_weapon_audio[0];
    status=rf_foley_find(&campaign_foley,launch==1?"Machine Pistol Launch":"HMG Launch 1",&group);
    if(status)goto requested_done;
    sample=campaign_pain_audio_resolve(&audio,group);status=audio.status;if(status)goto requested_done;
    if(sample<0 || (uint32_t)sample>=campaign_audio_bank.count){status=RF_NOT_FOUND;goto requested_done;}
    s->sample=sample;
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    if(!metadata || !(metadata->loop_flags&0x40000000u) ||
       (metadata->loop_flags&0x07ffffffu)){status=RF_FORMAT;goto requested_done;}
    s->voice=campaign_pain_audio_play_voice(&audio,owner->eye_position,sample);status=audio.status;
    if(!status && s->voice<0)status=RF_IO;
    if(!status) {
        status=rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,s->voice,&source);
        if(!status){s->source=source;++rf_scene_npc_loop_audio[1];}
        else s->voice=-1;
    }
requested_done:
    rf_scene_weapon_audio[6]=combat_sound_random.value;
    if(status)++rf_scene_weapon_audio[7];
done:
    if(status)++rf_scene_npc_loop_audio[4];
    rf_scene_npc_loop_audio[10]=(uint32_t)status;
    return requested; /* Optional audio failures never reject gameplay. */
}
static void scene_npc_loop_audio_tick(uint32_t slot)
{
    /* Called only by the actual post-ammo/post-holster cooldown continuation.
     * Remaining aim/range/LOS/pain checks still execute at their old deadline;
     * a denied due shot leaves this owner unseen and closes at pass end. */
    (void)scene_npc_loop_audio_observe(slot,0,0);
}
static uint32_t scene_npc_loop_audio_shot(uint32_t slot,uint32_t once)
{return scene_npc_loop_audio_observe(slot,1,once);}
#endif
