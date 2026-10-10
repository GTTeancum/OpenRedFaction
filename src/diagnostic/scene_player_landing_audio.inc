#ifndef RF_SCENE_PLAYER_LANDING_AUDIO_INCLUDED
#define RF_SCENE_PLAYER_LANDING_AUDIO_INCLUDED

/* Player audio boundary of419830. Keep the existing scene support commit,
 * velocity rebasing and stance transition as their single physics owners.
 *419839..4198b3 supplies the speed/material gate;4198b9..4198e2 selects
 *434d00(group,0), then48a9c0. This is FIRST-sample playback, not random Foley.
 * The live scope starts in the real player-stance callback and ends with the
 * scene frame. Queries, actor_routes and clearance probes cannot open it.
 * Ordinary spawn/restore/teleport/vehicle placement cancels it through the
 * existing scene_player_impact_relocated boundary. No save state is added.
 */
typedef struct scene_player_landing_audio_state {
    int32_t groups[3][10];
    uint32_t ready,enabled,skip,frame,owner,captured,reports;
    float velocity[3];
} scene_player_landing_audio_state;
static scene_player_landing_audio_state scene_player_landing_audio;
uint32_t rf_scene_player_landing_audio[16]; /* scopes,eligible,precontacts,commits,requests,starts,quiet,skipped,failures,form,material,group,sample,voice,status,preimpact */
float rf_scene_player_landing_values[8]; /* preimpact XYZ,accepted support XYZ,speed,relative Y */
char rf_scene_player_landing_sample[64];
uint32_t rf_scene_player_landing_events[16][12]; /* frame,before,after,material,group,sample,voice,status,preimpact,speed bits,relative-Y bits,flags */

static void scene_player_landing_audio_reset(void)
{
    memset(&scene_player_landing_audio,0,sizeof(scene_player_landing_audio));
    memset(scene_player_landing_audio.groups,0xff,sizeof(scene_player_landing_audio.groups));
    scene_player_landing_audio.skip=1;
    memset(rf_scene_player_landing_audio,0,sizeof(rf_scene_player_landing_audio));
    memset(rf_scene_player_landing_values,0,sizeof(rf_scene_player_landing_values));
    memset(rf_scene_player_landing_sample,0,sizeof(rf_scene_player_landing_sample));
    memset(rf_scene_player_landing_events,0,sizeof(rf_scene_player_landing_events));
    rf_scene_player_landing_audio[10]=rf_scene_player_landing_audio[11]=
        rf_scene_player_landing_audio[12]=rf_scene_player_landing_audio[13]=UINT32_MAX;
}

static int scene_player_landing_audio_load(const void *text,uint32_t bytes,const char *class_name)
{
    static const char *const forms[3]={"miner1","parker_suit","parker_sci"};
    int32_t groups[3][10];uint32_t form,slot;int status;
    for(form=0;form<3;form++) {
        status=rf_entity_land_sound_groups_read(text,bytes,campaign_spawn?forms[form]:class_name,
            &campaign_foley,groups[form]);if(status)return status;
        for(slot=0;slot<10;slot++)if(groups[form][slot]>=0) {
            const rf_foley_group *group;
            if((uint32_t)groups[form][slot]>=campaign_foley.group_count)return RF_RANGE;
            group=campaign_foley.groups+groups[form][slot];
            if(group->first>campaign_foley.sample_count || group->count>campaign_foley.sample_count-group->first)
                return RF_FORMAT;
        }
    }
    memcpy(scene_player_landing_audio.groups,groups,sizeof(groups));
    scene_player_landing_audio.ready=1;return RF_OK;
}

static void scene_player_landing_audio_end(void)
{scene_player_landing_audio.enabled=scene_player_landing_audio.captured=0;}

static void scene_player_landing_audio_relocated(void)
{scene_player_landing_audio_end();scene_player_landing_audio.skip=1;}

static uint32_t scene_player_landing_audio_owner(void)
{
    return campaign_spawn && scene_actor_body.allocated_bytes &&
        campaign_player_object.view==&campaign_player_view &&
        rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)==&campaign_player_view &&
        !(campaign_player_view.flags_810&1u) && campaign_player_damage.state.effects.health>0 &&
        campaign_player_view.linked_handle==-1 && !campaign_cutscene_runtime.active;
}

static void scene_player_landing_audio_begin(scene_stream *stream,uint32_t frame)
{
    scene_player_landing_audio_state *s=&scene_player_landing_audio;
    scene_player_landing_audio_end();++rf_scene_player_landing_audio[0];
    s->frame=frame;s->owner=campaign_player_object.handle;
    s->enabled=s->ready && frame && !s->skip && scene_player_landing_audio_owner() &&
        !scene_driller_active(stream) && !scene_turret_player_active();
    s->skip=0;
    if(s->enabled)++rf_scene_player_landing_audio[1];
}

/* Observe immediately BEFORE the live actor_contact response, after its
 * successful contact advance. Preserve the first walkable pre-impact velocity:
 * later collision passes may already have clipped its vertical component.
 * Merely seeing a contact never requests playback; the real grounded commit
 * below is mandatory. Wall/ceiling responses cannot supply landing evidence.
 */
static void scene_player_landing_audio_contact(const rf_physics_body_state *body,const float normal[3])
{
    scene_player_landing_audio_state *s=&scene_player_landing_audio;
    if(!s->enabled || s->captured || rf_scene_actor_landing[1]!=3 || !(normal[1]>=.5f))return;
    memcpy(s->velocity,body->velocity,12);s->captured=1;++rf_scene_player_landing_audio[2];
}

/* Call ONLY after a real mode3->mode1 support and body-position publication.
 * incoming is saved before support commit; campaign_support_velocity is now
 * that commit's actual accepted velocity, including passive-body point motion.
 * No physics or stance operation is repeated and audio failure is nonfatal.
 */
static void scene_player_landing_audio_commit(uint32_t frame,const float incoming[3],
    const rf_physics_body_state *body,int32_t material)
{
    scene_player_landing_audio_state *s=&scene_player_landing_audio;
    uint32_t form=rf_scene_player_model[0],slot=0,i,captured=s->captured,requested=0,*event;
    int32_t group=-1,sample=-1,voice=-1;int status=RF_OK;
    float velocity[3];double speed=0,relative=0;
    rf_player_sound_input input={0};rf_player_sound_request request;
    if(!s->enabled || s->frame!=frame || s->owner!=campaign_player_object.handle ||
       !scene_player_landing_audio_owner() || rf_scene_actor_landing[1]!=1) {
        ++rf_scene_player_landing_audio[7];return;
    }
    memcpy(velocity,captured?s->velocity:incoming,12);
    scene_player_landing_audio_end(); /* One committed landing per live frame. */
    ++rf_scene_player_landing_audio[3];
    event=rf_scene_player_landing_events[(rf_scene_player_landing_audio[3]-1)%16];
    rf_scene_player_landing_audio[9]=form;rf_scene_player_landing_audio[10]=(uint32_t)material;
    rf_scene_player_landing_audio[11]=rf_scene_player_landing_audio[12]=rf_scene_player_landing_audio[13]=UINT32_MAX;
    rf_scene_player_landing_audio[14]=RF_OK;rf_scene_player_landing_audio[15]=captured;
    rf_scene_player_landing_sample[0]=0;
    rf_scene_player_landing_values[6]=rf_scene_player_landing_values[7]=0;
    memcpy(rf_scene_player_landing_values,velocity,12);
    memcpy(rf_scene_player_landing_values+3,campaign_support_velocity,12);
    for(i=0;i<3;i++)if(!isfinite(velocity[i]) || !isfinite(campaign_support_velocity[i])) {
        status=RF_FORMAT;goto done;
    }
    relative=(double)campaign_support_velocity[1]-velocity[1];
    speed=sqrt(((double)velocity[0]*velocity[0]+(double)velocity[1]*velocity[1])+
        (double)velocity[2]*velocity[2]);
    rf_scene_player_landing_values[6]=(float)speed;rf_scene_player_landing_values[7]=(float)relative;
    if(!(relative>.25 || speed>.5)){++rf_scene_player_landing_audio[6];goto done;}
    ++rf_scene_player_landing_audio[4];requested=1;
    if(form>=3 || material>=10){status=RF_RANGE;goto done;}
    if(material>=0 && s->groups[form][material]>0)slot=(uint32_t)material;
    if((rf_scene_actor_stance_flags&0x1000u) && s->groups[form][4]>0)slot=4;
    group=s->groups[form][slot];rf_scene_player_landing_audio[11]=(uint32_t)group;
    /*434d00 returns-1 for absent/empty groups; it never consumes random state. */
    if(group<0){++rf_scene_player_landing_audio[6];goto done;}
    if((uint32_t)group>=campaign_foley.group_count){status=RF_RANGE;goto done;}
    {
        const rf_foley_group *g=campaign_foley.groups+group;
        if(!g->count){++rf_scene_player_landing_audio[6];goto done;}
        if(!campaign_foley.samples || g->first>=campaign_foley.sample_count ||
           g->count>campaign_foley.sample_count-g->first){status=RF_FORMAT;goto done;}
        sample=campaign_foley.samples[g->first];
    }
    rf_scene_player_landing_audio[12]=(uint32_t)sample;
    if(sample<0){++rf_scene_player_landing_audio[6];goto done;}
    if((uint32_t)sample>=campaign_audio_bank.count){status=RF_RANGE;goto done;}
    memcpy(rf_scene_player_landing_sample,campaign_audio_bank.samples[sample].name,
        strlen(campaign_audio_bank.samples[sample].name)+1);
    input.owner_present=1;input.sound_id=sample;input.volume=1;
    memcpy(input.position,body->position,12);
    status=rf_player_sound_route(&input,&request);if(status)goto done;
    /* The normal sound owner lazily admits original PCM, applies authored gain
     * once, and performs real native-device playback and lifetime tracking. */
    voice=campaign_sound_start(request.sound_id,body->position,request.volume,
        (uint32_t)request.group,request.spatial,0);
    if(voice<0)status=RF_IO;else ++rf_scene_player_landing_audio[5];
done:
    if(status)++rf_scene_player_landing_audio[8];
    rf_scene_player_landing_audio[13]=(uint32_t)voice;rf_scene_player_landing_audio[14]=(uint32_t)status;
    event[0]=frame;event[1]=3;event[2]=1;event[3]=(uint32_t)material;event[4]=(uint32_t)group;
    event[5]=(uint32_t)sample;event[6]=(uint32_t)voice;event[7]=(uint32_t)status;event[8]=captured;
    memcpy(event+9,rf_scene_player_landing_values+6,8);event[11]=rf_scene_actor_stance_flags;
    if((requested && s->reports<16) || (status && rf_scene_player_landing_audio[8]<=4)) {
        ++s->reports;
        printf("PLAYER_LAND_SOUND frame%u form%u material%d group%d file%s speed%.9g relative%.9g preimpact%u starts%u status%d\n",
            frame,form,material,group,rf_scene_player_landing_sample[0]?rf_scene_player_landing_sample:"<none>",
            speed,relative,captured,rf_scene_player_landing_audio[5],status);
    }
}
#endif
