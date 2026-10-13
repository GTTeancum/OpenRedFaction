#ifndef RF_SCENE_NPC_LOOP_AUDIO_INCLUDED
#define RF_SCENE_NPC_LOOP_AUDIO_INCLUDED
/* Include after combat_sound and registered NPC/audio owners. Original
 *4269c1..4269fb enters41a870 for continuous fire;41a9bd guards actor81c and
 *41aa3d..41aa95 retains its positional Launch voice. This adapter observes
 * the port's accepted MP/HMG/Vauss/Flame primary shots and actual cooldown branch. It
 * neither creates a held trigger nor changes AI, cadence, ammo or saves.
 * Begin/end surround the entire enemy pass, including skips and errors. */
typedef struct scene_npc_loop_audio_state {
    uint32_t owner,slot,uid,source,seen;int32_t weapon,voice,sample;
} scene_npc_loop_audio_state;
_Static_assert(sizeof(scene_npc_loop_audio_state)==32,"NPC loop retirement must not grow records");
#define SCENE_NPC_LOOP_SEEN 1u
#define SCENE_NPC_LOOP_PENDING 2u
#define SCENE_NPC_LOOP_ENDED 4u
#define SCENE_NPC_LOOP_ATTEMPTED 0x80000000u
static scene_npc_loop_audio_state scene_npc_loop_audio[RF_AUDIO_VOICES];
/* Cursor high bit consumes this frame's one native retirement attempt. */
static struct {uint32_t frame,cursor;} scene_npc_loop_retirement={UINT32_MAX,0};
_Static_assert(sizeof(scene_npc_loop_retirement)==8,"NPC loop retirement scheduling budget");
#ifdef RF_IMAGE_XBOX_NATIVE
#include "../platform/xbox/audio.h"
#endif
uint32_t rf_scene_npc_loop_audio[16]; /* calls,starts,stops,retained,failures,stale,moves,owners,peak,full,status,bytes,shots,single suppressed,invalid owner,closes */

static uint32_t scene_npc_loop_audio_weapon(int32_t weapon)
{
    /* These IDs were resolved from the immutable weapon catalog before
     * startup events. NPC gameplay currently supports PRIMARY fire only;
     * Machine Pistol Special and HMG alternate are not fabricated here. */
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=campaign_weapon_supply.names.count)return 0;
    return weapon==campaign_extra_ids[0]?1u:weapon==campaign_extra_ids[1]?2u:
        campaign_enemy_vauss_primary_weapon(weapon)?3u:weapon==campaign_flame_id?4u:0u;
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
    rf_scene_npc_loop_audio[11]=sizeof(scene_npc_loop_audio)+sizeof(rf_scene_npc_loop_audio)+sizeof(scene_npc_loop_retirement);
}
/* No NPC/registry dereference or public-ID resolution belongs in retirement.
 * Pending records keep their exact native source through death and slot reuse. */
static int scene_npc_loop_audio_retire(scene_npc_loop_audio_state *s)
{
    uint32_t source=s->source,index=source&0xffffu;int status=RF_OK;
    if(!s->owner)return RF_OK;
    if(source) {
#ifdef RF_IMAGE_XBOX_NATIVE
        status=rf_xbox_audio_release_voice(source);
        if(status!=RF_OK && status!=RF_NOT_FOUND)goto failed;
#else
        if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,source);
#endif
        /* Keep the software loop occupied on failure. Ordinary allocation
         * cannot recycle that slot, even after the generation counter wraps. */
        (void)rf_audio_voice_stop(&campaign_audio_mixer,source);
        if(index<RF_AUDIO_VOICES && campaign_spatial_voices[index].handle==source)
            memset(campaign_spatial_voices+index,0,sizeof(*campaign_spatial_voices));
        ++rf_scene_npc_loop_audio[2];
    } else if(s->voice>=0) {
        /* Serialized post-start capture cannot normally fail. Keep an
         * unresolved identity rather than guessing a later public mapping. */
        status=RF_FORMAT;goto failed;
    }
    if(s->seen&SCENE_NPC_LOOP_ENDED) {
        memset(s,0,sizeof(*s));++rf_scene_npc_loop_audio[15];
    } else {
        /* Stale playback during an admitted episode stays attempted/silent;
         * retirement does not turn cooldown or the next shot into a retry. */
        s->source=0;s->voice=s->sample=-1;s->seen&=~SCENE_NPC_LOOP_PENDING;
    }
    rf_scene_npc_loop_audio[10]=RF_OK;scene_npc_loop_audio_publish();return RF_OK;
failed:
    s->seen|=SCENE_NPC_LOOP_PENDING;
    ++rf_scene_npc_loop_audio[4];rf_scene_npc_loop_audio[10]=(uint32_t)status;
    scene_npc_loop_audio_publish();return status;
}
static int scene_npc_loop_audio_attempt(scene_npc_loop_audio_state *s)
{
    uint32_t index=(uint32_t)(s-scene_npc_loop_audio);
    scene_npc_loop_retirement.cursor=((index+1u)%RF_AUDIO_VOICES)|SCENE_NPC_LOOP_ATTEMPTED;
    return scene_npc_loop_audio_retire(s);
}
static void scene_npc_loop_audio_begin_frame(uint32_t frame)
{
    if(scene_npc_loop_retirement.frame!=frame) {
        scene_npc_loop_retirement.frame=frame;
        scene_npc_loop_retirement.cursor&=~SCENE_NPC_LOOP_ATTEMPTED;
    }
}
static void scene_npc_loop_audio_retirement_tick(uint32_t frame)
{
    uint32_t first;
    scene_npc_loop_audio_begin_frame(frame);
    if(scene_npc_loop_retirement.cursor&SCENE_NPC_LOOP_ATTEMPTED)return;
    first=scene_npc_loop_retirement.cursor;
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++) {
        scene_npc_loop_audio_state *s=scene_npc_loop_audio+(first+i)%RF_AUDIO_VOICES;
        if(s->owner && (s->seen&SCENE_NPC_LOOP_PENDING)) {
            (void)scene_npc_loop_audio_attempt(s);break;
        }
    }
}
static void scene_npc_loop_audio_release(scene_npc_loop_audio_state *s)
{
    if(!s->owner)return;
    s->seen|=SCENE_NPC_LOOP_PENDING|SCENE_NPC_LOOP_ENDED;
    if(!s->source && s->voice<0)(void)scene_npc_loop_audio_retire(s);
    else {
        /* Frame entry sets the budget before input/restore/simulation can
         * remove owners. The later listener hook cannot grant a second
         * attempt in that frame. Select pending work fairly. */
        scene_npc_loop_audio_retirement_tick(scene_npc_loop_retirement.frame);
    }
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
static int scene_npc_loop_audio_drain(void)
{
    int status;
    /* Explicit lifecycle only: mark all before attempting any. Stop at the
     * first failure, so multiple failed voices cannot each add a timeout. */
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)if(scene_npc_loop_audio[i].owner)
        scene_npc_loop_audio[i].seen|=SCENE_NPC_LOOP_PENDING|SCENE_NPC_LOOP_ENDED;
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)if(scene_npc_loop_audio[i].owner) {
        status=scene_npc_loop_audio_attempt(scene_npc_loop_audio+i);if(status)return status;
    }
    return RF_OK;
}
static int scene_npc_loop_audio_idle_status(void)
{
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)if(scene_npc_loop_audio[i].owner ||
        scene_npc_loop_audio[i].source || (scene_npc_loop_audio[i].seen&SCENE_NPC_LOOP_PENDING))return RF_RANGE;
    return RF_OK;
}
static int scene_npc_loop_audio_reset(void)
{
    int status=scene_npc_loop_audio_drain();if(status)return status;
    scene_npc_loop_retirement.frame=UINT32_MAX;scene_npc_loop_retirement.cursor=0;
    memset(rf_scene_npc_loop_audio,0,sizeof(rf_scene_npc_loop_audio));
    scene_npc_loop_audio_publish();
    return RF_OK;
}
static void scene_npc_loop_audio_begin(void)
{
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++)scene_npc_loop_audio[i].seen&=~SCENE_NPC_LOOP_SEEN;
}
static void scene_npc_loop_audio_end(void)
{
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++) {
        scene_npc_loop_audio_state *s=scene_npc_loop_audio+i;
        campaign_npc_body *owner;uint32_t uid=0;
        if(!s->owner)continue;
        if(s->seen&SCENE_NPC_LOOP_ENDED){scene_npc_loop_audio_release(s);continue;}
        owner=(s->seen&SCENE_NPC_LOOP_SEEN)?scene_npc_loop_audio_owner(s->slot,&uid):NULL;
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
    const rf_sound_metadata *metadata;uint32_t uid=0,launch,source,requested=0,blocked=0;
    int32_t weapon,group,sample;int status=RF_OK;
    ++rf_scene_npc_loop_audio[0];rf_scene_npc_loop_audio[12]+=fired!=0;
    owner=scene_npc_loop_audio_owner(slot,&uid);
    for(uint32_t i=0;i<RF_AUDIO_VOICES;i++) {
        scene_npc_loop_audio_state *candidate=scene_npc_loop_audio+i;
        if(candidate->owner && candidate->slot==slot) {
            if(candidate->seen&SCENE_NPC_LOOP_ENDED)blocked=1;
            else if(!owner || candidate->owner!=owner->registration.handle || candidate->uid!=uid) {
                scene_npc_loop_audio_release(candidate);
                if(candidate->owner)blocked=1;
            } else s=candidate;
        }
        if(!candidate->owner && !free_slot)free_slot=candidate;
    }
    if(!owner){++rf_scene_npc_loop_audio[14];status=RF_NOT_FOUND;goto done;}
    weapon=owner->view.weapons[0];launch=scene_npc_loop_audio_weapon(weapon);
    if(s && (once || s->weapon!=weapon || !launch)) {
        scene_npc_loop_audio_release(s);
        if(s->owner)blocked=1;else if(!free_slot)free_slot=s;
        s=NULL;
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
        s->seen|=SCENE_NPC_LOOP_SEEN;++rf_scene_npc_loop_audio[3];
        if(s->voice>=0 && !(s->seen&SCENE_NPC_LOOP_PENDING)) {
            source=s->source;
            if(!source || (source&0xffffu)>=RF_AUDIO_VOICES ||
               campaign_audio_mixer.voices[source&0xffffu].handle!=source) {
                s->seen|=SCENE_NPC_LOOP_PENDING;++rf_scene_npc_loop_audio[5];status=RF_NOT_FOUND;
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
    s->weapon=weapon;s->voice=s->sample=-1;s->source=0;s->seen=SCENE_NPC_LOOP_SEEN;
    scene_npc_loop_audio_publish();requested=1;++rf_scene_weapon_audio[0];
    /* Use an existing free record as this episode's silent attempt while its
     * predecessor is pending. Never overwrite the old identity or start late
     * after it retires. Normal fixed-pool exhaustion still suppresses audio. */
    if(blocked){status=RF_IO;goto requested_done;}
    status=rf_foley_find(&campaign_foley,launch==1?"Machine Pistol Launch":
        launch==2?"HMG Launch 1":launch==3?"Vauss5 Fire":"Flame Thrower Ignite 2",&group);
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
        else s->seen|=SCENE_NPC_LOOP_PENDING;
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
