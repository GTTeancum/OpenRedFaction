#ifndef RF_SCENE_WEAPON_IMPACT_AUDIO_INCLUDED
#define RF_SCENE_WEAPON_IMPACT_AUDIO_INCLUDED
#include "rf/weapon_impact_audio.h"

/* Optional dry conventional-hit audio. Include after combat_shot_obstructed,
 * campaign_body_surface and the existing Foley/spatial audio services.
 * Original4c5912..4c5996: select material sample, fall back to Default ONLY
 * when that sample is-1, then one positional5056a0 request at the hit point.
 * It is independent of damage, pain and death effects. Never call this from
 * a generic visibility/pickup query. Underwater groups remain metadata only.
 */
#define SCENE_WEAPON_IMPACT_SAMPLE_LIMIT 64u
#define SCENE_WEAPON_IMPACT_PCM_LIMIT (192u*1024u)
#define SCENE_WEAPON_IMPACT_HEADROOM (256u*1024u)
typedef struct scene_weapon_impact_audio_sample {
    uint32_t sample,newly_loaded;uint8_t previous_evictable;
} scene_weapon_impact_audio_sample;
typedef struct scene_weapon_impact_audio_state {
    rf_weapon_impact_audio_groups catalog;
    scene_weapon_impact_audio_sample samples[SCENE_WEAPON_IMPACT_SAMPLE_LIMIT];
    uint8_t supported[64];uint32_t sample_count,ready;
    rf_random_state random;
} scene_weapon_impact_audio_state;
static scene_weapon_impact_audio_state scene_weapon_impact_audio;
/* contacts,starts,silent/errors,world queries/hits,admitted samples/PCM bytes,
 * latest status/weapon/material/group/sample/voice,RNG,ready */
uint32_t rf_scene_weapon_impact_audio[16];
float rf_scene_weapon_impact_audio_position[3];
static uint32_t scene_weapon_impact_audio_detail[9];

/* Pin ownership ends before the bank is destroyed. PCM stays with that bank;
 * never unload a sample that an existing ordinary/native voice may borrow. */
static void scene_weapon_impact_audio_close(void)
{
    scene_weapon_impact_audio_state *s=&scene_weapon_impact_audio;uint32_t i;
    if(s->ready)for(i=0;i<s->sample_count;i++) {
        uint32_t sample=s->samples[i].sample;
        if(sample<campaign_audio_bank.count && sample<sizeof(campaign_audio_evictable))
            campaign_audio_evictable[sample]=s->samples[i].previous_evictable;
    }
    s->ready=0;s->sample_count=0;memset(s->supported,0,sizeof(s->supported));
    rf_scene_weapon_impact_audio[15]=0;
}
static void scene_weapon_impact_audio_reset(void)
{
    /* Scene initialization follows previous owner teardown, not frame zero of
     * combat: resetting on that frame would discard successful preadmission. */
    memset(&scene_weapon_impact_audio,0,sizeof(scene_weapon_impact_audio));
    memset(scene_weapon_impact_audio.catalog.groups,0xff,sizeof(scene_weapon_impact_audio.catalog.groups));
    scene_weapon_impact_audio.random.value=1;
    memset(rf_scene_weapon_impact_audio,0,sizeof(rf_scene_weapon_impact_audio));
    memset(scene_weapon_impact_audio_detail,0,sizeof(scene_weapon_impact_audio_detail));
    memset(rf_scene_weapon_impact_audio_position,0,sizeof(rf_scene_weapon_impact_audio_position));
    for(uint32_t i=9;i<=13;i++)rf_scene_weapon_impact_audio[i]=UINT32_MAX;
}
static uint32_t scene_weapon_impact_audio_supported(int32_t weapon)
{
    static const char *const names[]={"12mm handgun","Undercover 12mm handgun",
        "Assault Rifle","Shotgun","Sniper Rifle","rail_gun","Machine Pistol",
        "Machine Pistol Special","heavy_machine_gun","scope_assault_rifle"};
    uint32_t i;
    for(i=0;i<sizeof(names)/sizeof(names[0]);i++)
        if(rf_weapon_name_find(&campaign_weapon_reset.names,names[i])==weapon)return 1;
    return 0;
}
static uint32_t scene_weapon_impact_audio_has_sample(uint32_t sample)
{
    uint32_t i;
    for(i=0;i<scene_weapon_impact_audio.sample_count;i++)
        if(scene_weapon_impact_audio.samples[i].sample==sample)return 1;
    return 0;
}

/* Run after mandatory startup PCM admission and before final bank statistics.
 * tables and audio are the open LOCAL archives from campaign_audio_open.
 * All-or-nothing preadmission retains256KiB of bank headroom for later users,
 * never evicts a sample, and never makes the level fail. Installed supported
 * groups deduplicate to21 waveforms/150954 file bytes. New table content still
 * obeys the64-sample/192KiB cap and nonlooping requirement.
 */
static void scene_weapon_impact_audio_load(rf_vpp *tables,rf_vpp *audio)
{
    scene_weapon_impact_audio_state *s=&scene_weapon_impact_audio;
    uint32_t weapon,material,i,total=0,additional=0;int status;
    scene_weapon_impact_audio_close();
    status=rf_weapon_impact_audio_groups_load(tables,128u*1024u,
        &campaign_weapon_reset.names,&campaign_foley,&s->catalog);if(status)goto failed;
    if(!audio || !audio->stream || !campaign_audio_bank.samples ||
       campaign_audio_bank.count>sizeof(campaign_audio_evictable)) {status=RF_RANGE;goto failed;}
    for(weapon=0;weapon<s->catalog.count;weapon++) {
        s->supported[weapon]=(uint8_t)scene_weapon_impact_audio_supported((int32_t)weapon);
        if(!s->supported[weapon])continue;
        for(material=0;material<10;material++) {
            int32_t group=s->catalog.groups[weapon][0][material];const rf_foley_group *g;
            if(group<0)continue;
            if((uint32_t)group>=campaign_foley.group_count){status=RF_RANGE;goto failed;}
            g=campaign_foley.groups+group;
            if(!g->count || !campaign_foley.samples || g->first>=campaign_foley.sample_count ||
               g->count>campaign_foley.sample_count-g->first){status=RF_FORMAT;goto failed;}
            for(i=0;i<g->count;i++) {
                int32_t sample=campaign_foley.samples[g->first+i];rf_vpp_entry entry;
                const rf_sound_metadata *metadata;uint32_t bytes;
                if(sample==-1)continue; /* Retain original absent-sample fallback. */
                if(sample<0 || (uint32_t)sample>=campaign_audio_bank.count){status=RF_RANGE;goto failed;}
                if(scene_weapon_impact_audio_has_sample((uint32_t)sample))continue;
                if(s->sample_count==SCENE_WEAPON_IMPACT_SAMPLE_LIMIT){status=RF_RANGE;goto failed;}
                metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
                    campaign_audio_bank.samples[sample].name);
                if(metadata && (metadata->loop_flags&0x40000000u)){status=RF_FORMAT;goto failed;}
                if(rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)sample))bytes=campaign_audio_bank.samples[sample].bytes;
                else {
                    status=rf_vpp_find(audio,campaign_audio_bank.samples[sample].name,&entry);if(status)goto failed;
                    bytes=entry.size;
                    if(!bytes || bytes>SCENE_WEAPON_IMPACT_PCM_LIMIT-additional){status=RF_RANGE;goto failed;}
                    additional+=bytes;
                }
                if(!bytes || bytes>SCENE_WEAPON_IMPACT_PCM_LIMIT-total){status=RF_RANGE;goto failed;}
                total+=bytes;
                s->samples[s->sample_count++]=(scene_weapon_impact_audio_sample){(uint32_t)sample,0,0};
            }
        }
    }
    if(!s->sample_count){status=RF_NOT_FOUND;goto failed;}
    if((uint64_t)campaign_audio_bank.bytes+additional+SCENE_WEAPON_IMPACT_HEADROOM>
       campaign_audio_bank.budget){status=RF_RANGE;goto failed;}
    for(i=0;i<s->sample_count;i++) {
        uint32_t sample=s->samples[i].sample;
        if(!rf_audio_bank_sample(&campaign_audio_bank,sample)) {
            status=rf_audio_bank_reload(&campaign_audio_bank,audio,sample);if(status)goto failed;
            s->samples[i].newly_loaded=1;
        }
    }
    for(i=0;i<s->sample_count;i++) {
        uint32_t sample=s->samples[i].sample;
        s->samples[i].previous_evictable=campaign_audio_evictable[sample];campaign_audio_evictable[sample]=0;
    }
    s->ready=1;rf_scene_weapon_impact_audio[6]=s->sample_count;
    rf_scene_weapon_impact_audio[7]=total;rf_scene_weapon_impact_audio[8]=RF_OK;
    rf_scene_weapon_impact_audio[15]=1;return;
failed:
    /* Only freshly loaded, not-yet-published PCM is rolled back. Previously
     * resident rows and all existing borrowers/eviction flags stay intact. */
    for(i=0;i<s->sample_count;i++)if(s->samples[i].newly_loaded)
        (void)rf_audio_bank_unload(&campaign_audio_bank,s->samples[i].sample);
    s->sample_count=0;s->ready=0;memset(s->supported,0,sizeof(s->supported));
    rf_scene_weapon_impact_audio[6]=rf_scene_weapon_impact_audio[7]=rf_scene_weapon_impact_audio[15]=0;
    rf_scene_weapon_impact_audio[8]=(uint32_t)status;++rf_scene_weapon_impact_audio[3];
}

static uint32_t scene_weapon_impact_audio_ready(int32_t weapon)
{
    const scene_weapon_impact_audio_state *s=&scene_weapon_impact_audio;
    return s->ready && weapon>=0 && (uint32_t)weapon<s->catalog.count && s->supported[weapon];
}
static void scene_weapon_impact_audio_contact(int32_t weapon,int32_t material,const float position[3])
{
    scene_weapon_impact_audio_state *s=&scene_weapon_impact_audio;
    campaign_pain_audio_context audio={&s->random,0,scene_weapon_impact_audio_detail,0};
    uint32_t slot,i;int status=RF_OK;int32_t group=-1,sample=-1,voice=-1;
    if(!scene_weapon_impact_audio_ready(weapon))return;
    ++rf_scene_weapon_impact_audio[0];rf_scene_weapon_impact_audio[9]=(uint32_t)weapon;
    rf_scene_weapon_impact_audio[10]=(uint32_t)material;
    if(!position || material< -1 || material>=10){status=RF_RANGE;goto done;}
    for(i=0;i<3;i++)if(!isfinite(position[i])){status=RF_FORMAT;goto done;}
    memcpy(rf_scene_weapon_impact_audio_position,position,12);
    slot=material<0?0:(uint32_t)material;
    group=s->catalog.groups[weapon][0][slot];
    sample=campaign_pain_audio_resolve(&audio,group);status=audio.status;if(status)goto done;
    if(sample==-1) {
        /* The original retries Default even when the selected material was
         * already0. A declared group containing-1 may draw a second sample. */
        group=s->catalog.groups[weapon][0][0];
        sample=campaign_pain_audio_resolve(&audio,group);status=audio.status;if(status)goto done;
    }
    if(sample==-1){++rf_scene_weapon_impact_audio[2];goto done;}
    if(sample<0 || (uint32_t)sample>=campaign_audio_bank.count ||
       !scene_weapon_impact_audio_has_sample((uint32_t)sample) ||
       !rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)sample)) {status=RF_NOT_FOUND;goto done;}
    /* Preadmitted/pinned PCM plus the ordinary generation-qualified spatial
     * owner. The helper never allocates, loads or opens an archive per hit.
     * Shared native voice admission/lifetime remains audio-backend owned. */
    voice=campaign_sound_play(NULL,sample,position,1,0);
    if(voice<0)status=RF_IO;else ++rf_scene_weapon_impact_audio[1];
done:
    if(status)++rf_scene_weapon_impact_audio[3];
    rf_scene_weapon_impact_audio[8]=(uint32_t)status;
    rf_scene_weapon_impact_audio[11]=(uint32_t)group;rf_scene_weapon_impact_audio[12]=(uint32_t)sample;
    rf_scene_weapon_impact_audio[13]=(uint32_t)voice;
    rf_scene_weapon_impact_audio[14]=s->random.value;
    if(rf_scene_combat_trace)printf("WEAPON_IMPACT_AUDIO %d %d %d %d %d %d\n",weapon,material,group,sample,voice,status);
}

/* Audio-only original ordered-contact query, not generic nearest geometry.
 * Retained-fraction shortening can prefer a mover over a nearer static face.
 * Caller supplies the accepted fired ray and its current target limit.
 * Existing gameplay visibility queries
 * remain unchanged and must never consume this helper's observations. */
static void scene_weapon_impact_audio_world(scene_stream *stream,int32_t weapon,
    const float start[3],const float delta[3],float limit)
{
    rf_collision_solid_hit hit;rf_geometry_materials mapping={0};
    rf_geometry_body_surfaces surfaces;float end[3];uint32_t matched=0,i,texture,material=0;
    int status,material_status=RF_NOT_FOUND;
    if(!scene_weapon_impact_audio_ready(weapon))return;
    if(!stream || !stream->collision || !start || !delta || !isfinite(limit) || limit<0 || limit>1) {
        status=RF_RANGE;goto failed;
    }
    for(i=0;i<3;i++) {
        if(!isfinite(start[i]) || !isfinite(delta[i])){status=RF_FORMAT;goto failed;}
        end[i]=start[i]+delta[i]*limit;
    }
    ++rf_scene_weapon_impact_audio[4];
    status=rf_geometry_collision_ray(stream->collision,&campaign_movers,start,end,0x26,&hit,&matched);
    if(status)goto failed;if(!matched)return;
    ++rf_scene_weapon_impact_audio[5];
    if(actor_follow_world && campaign_surface_palette && campaign_surface_sources) {
        mapping.offsets=actor_follow_world->offsets;mapping.slots=actor_follow_world->slots;
        mapping.count=actor_follow_world->geometry_count;mapping.textures.count=actor_follow_world->material_count;
        surfaces.geometries=campaign_surface_sources;surfaces.count=mapping.count;
        surfaces.mapping=&mapping;surfaces.palette=campaign_surface_palette;
        material_status=campaign_body_surface(&surfaces,hit.solid_index,hit.face_index,&texture,&material);
    }
    /* Missing metadata uses the original default slot, never an invented rock
     * or metal assignment. A real material index outside0..9 is rejected. */
    scene_weapon_impact_audio_contact(weapon,material_status?-1:(int32_t)material,hit.hit.point);
    if(material_status) {++rf_scene_weapon_impact_audio[3];rf_scene_weapon_impact_audio[8]=(uint32_t)material_status;}
    return;
failed:
    ++rf_scene_weapon_impact_audio[3];rf_scene_weapon_impact_audio[8]=(uint32_t)status;
}
#endif
