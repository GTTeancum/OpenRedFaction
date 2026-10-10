#ifndef RF_SCENE_NPC_FATAL_AUDIO_INCLUDED
#define RF_SCENE_NPC_FATAL_AUDIO_INCLUDED
/* Ordinary fatal 4196f0 feedback, distinct from selected death-action Foley.
 * Called only by RF_DAMAGE_PAIN_SOUND: core/entity.c retains incoming/class
 * health > .001 and kind != 10. Exact admitted robot profiles route elsewhere.
 * Optional metadata/PCM failures never become a lethal gameplay failure. */
static int32_t *scene_npc_fatal_groups;
static uint32_t scene_npc_fatal_group_count;
/* classes,bytes,binding hash,metadata failures,notifications,descriptor attempts,
 *fallback selections,quiet/repeated,errors,UID,handle,group */
uint32_t rf_scene_npc_fatal_audio[12];

static void scene_npc_fatal_audio_close(void)
{
    free(scene_npc_fatal_groups);scene_npc_fatal_groups=NULL;
    scene_npc_fatal_group_count=0;
    rf_scene_npc_fatal_audio[0]=rf_scene_npc_fatal_audio[1]=0;
}
static void scene_npc_fatal_audio_reset(void)
{
    scene_npc_fatal_audio_close();
    memset(rf_scene_npc_fatal_audio,0,sizeof(rf_scene_npc_fatal_audio));
    rf_scene_npc_fatal_audio[9]=rf_scene_npc_fatal_audio[10]=rf_scene_npc_fatal_audio[11]=UINT32_MAX;
}
static void scene_npc_fatal_audio_load(const void *text,uint32_t bytes)
{
    uint32_t count=campaign_seeds.class_count,i;
    scene_npc_fatal_audio_close();
    if(!count)return;
    if(count>640 || !text || !bytes){++rf_scene_npc_fatal_audio[3];return;}
    scene_npc_fatal_groups=malloc(count*sizeof(*scene_npc_fatal_groups));
    if(!scene_npc_fatal_groups){++rf_scene_npc_fatal_audio[3];return;}
    for(i=0;i<count;i++) {
        int32_t groups[3];const char *name;int status;
        /* -2 is unavailable metadata, not an authored absent descriptor (-1).
         * A failed optional read stays silent rather than inventing fallback. */
        scene_npc_fatal_groups[i]=-2;
        status=scene_turret_class_name(&campaign_seeds,i,&name);
        if(!status)status=rf_entity_damage_sound_groups_read(text,bytes,name,&campaign_foley,groups);
        if(status){++rf_scene_npc_fatal_audio[3];continue;}
        scene_npc_fatal_groups[i]=groups[2];
    }
    scene_npc_fatal_group_count=count;
    rf_scene_npc_fatal_audio[0]=count;
    rf_scene_npc_fatal_audio[1]=count*sizeof(*scene_npc_fatal_groups);
    rf_scene_npc_fatal_audio[2]=npc_hash_bytes(2166136261u,scene_npc_fatal_groups,rf_scene_npc_fatal_audio[1]);
    rf_scene_foley[3]+=rf_scene_npc_fatal_audio[1];
}

typedef struct scene_npc_fatal_audio_context {
    campaign_pain_audio_context audio;
    rf_entity_damage_sound_state *state;
    campaign_npc_body *owner;
    uint32_t slot,handle,uid,cls;
    int32_t deadline;
} scene_npc_fatal_audio_context;

/* Complete registered identity. No lookup through a low slot or UID alone.
 * All gameplay writes occur before the first optional audio callback, so no
 * retained owner is dereferenced after a callback can replace/remove it. */
static campaign_npc_body *scene_npc_fatal_audio_owner(const scene_npc_fatal_audio_context *c)
{
    campaign_npc_body *o;
    if(!campaign_npc_bodies || !campaign_seeds.items || !campaign_seeds.records.items ||
       !campaign_seeds.classes || c->slot>=campaign_npc_body_count ||
       c->slot>=campaign_seeds.records.count || c->cls>=campaign_seeds.class_count ||
       campaign_npc_bodies+c->slot!=c->owner || !c->handle || c->handle==UINT32_MAX)return NULL;
    o=campaign_npc_bodies+c->slot;
    if(campaign_seeds.items[c->slot].class_index!=c->cls ||
       (uint32_t)campaign_seeds.records.items[c->slot].record.uid!=c->uid ||
       o->registration.view!=&o->view || o->registration.handle!=c->handle ||
       o->registration.object_kind!=0 || o->view.type!=0 ||
       (uint32_t)o->view.handle!=c->handle || o->damage.effects.handle!=c->handle ||
       rf_object_registry_lookup(&campaign_registry,c->handle)!=&o->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)c->handle)!=&o->view ||
       !isfinite(o->damage.effects.health) || o->damage.effects.health>0)return NULL;
    return o;
}
static int32_t scene_npc_fatal_audio_resolve(void *context,int32_t group)
{
    scene_npc_fatal_audio_context *c=context;
    if(c->state->death_descriptor==-1) {
        campaign_npc_body *o=scene_npc_fatal_audio_owner(c);
        if(!o || o->pain_sound.deadline!=c->deadline){c->audio.status=RF_NOT_FOUND;return -1;}
        /* The shared core has applied action1/17, timer and 1000ms rules.
         * Publish its exact deadline before resolution/playback, including
         * missing samples and voice-gated attempts. Never copy it back later. */
        o->pain_sound.deadline=c->state->deadline;
        ++rf_scene_npc_fatal_audio[6];
    }
    return campaign_pain_audio_resolve(&c->audio,group);
}
static int32_t scene_npc_fatal_audio_playing(void *context,int32_t voice)
{
    scene_npc_fatal_audio_context *c=context;
    return campaign_pain_audio_playing(&c->audio,voice);
}
static void scene_npc_fatal_audio_play(void *context,const float position[3],int32_t sample)
{
    scene_npc_fatal_audio_context *c=context;
    campaign_pain_audio_play(&c->audio,position,sample);
}
static void scene_npc_fatal_sound(uint32_t slot,uint32_t handle,float fraction,int32_t now,rf_random_state *random)
{
    scene_npc_fatal_audio_context context={0};campaign_npc_body *o;
    rf_entity_damage_sound_state state={0};uint32_t i,predicate,selections;int status;
    rf_entity_damage_sound_backend backend={scene_npc_fatal_audio_resolve,scene_npc_fatal_audio_playing,scene_npc_fatal_audio_play,&context};
    if(!random || !isfinite(fraction) || now<0 || now>RF_TIMER_PERIOD ||
       !campaign_npc_bodies || !campaign_seeds.items || !campaign_seeds.records.items ||
       slot>=campaign_npc_body_count || slot>=campaign_seeds.records.count)return;
    context.owner=campaign_npc_bodies+slot;context.slot=slot;context.handle=handle;
    context.uid=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    context.cls=campaign_seeds.items[slot].class_index;
    o=scene_npc_fatal_audio_owner(&context);if(!o)return;
    ++rf_scene_npc_fatal_audio[4];
    rf_scene_npc_fatal_audio[9]=context.uid;rf_scene_npc_fatal_audio[10]=handle;
    if(!scene_npc_fatal_groups || context.cls>=scene_npc_fatal_group_count ||
       !campaign_pain_groups || scene_npc_fatal_groups[context.cls]<-1) {
        ++rf_scene_npc_fatal_audio[7];return;
    }
    state.health=o->damage.effects.health;
    state.flags=o->view.flags_810|o->damage.effects.flags_810;
    state.death_descriptor=state.death_class=scene_npc_fatal_groups[context.cls];
    state.light_class=campaign_pain_groups[context.cls][0];state.heavy_class=campaign_pain_groups[context.cls][1];
    state.action=o->view.action_520;state.deadline=o->pain_sound.deadline;state.voice=o->pain_sound.voice;
    memcpy(state.position,o->eye_position,12);predicate=o->view.flags_810&1u;
    for(i=0;i<3;i++)if(!isfinite(state.position[i])){++rf_scene_npc_fatal_audio[8];return;}
    rf_scene_npc_fatal_audio[11]=(uint32_t)state.death_class;
    if(state.death_descriptor!=-1) {
        if(state.flags&4u){++rf_scene_npc_fatal_audio[7];return;}
        /* Consume the explicit attempt before callbacks, even if resolution
         * or optional sample admission fails. The core receives the copied
         * pre-latch flags; this helper never overwrites callback flag edits. */
        o->view.flags_810|=4u;o->damage.effects.flags_810|=4u;
        ++rf_scene_npc_fatal_audio[5];
    }
    context.state=&state;context.deadline=state.deadline;
    context.audio=(campaign_pain_audio_context){random,0,rf_scene_npc_pain_audio,0};
    selections=rf_scene_npc_fatal_audio[6];++rf_scene_npc_pain_audio[0];
    status=rf_entity_damage_sound(&state,fraction,predicate,0,now,&backend);
    /* No owner dereference or gameplay publication follows optional audio. */
    rf_scene_npc_pain_audio[6]=random->value;
    if(status || context.audio.status){++rf_scene_npc_fatal_audio[8];++rf_scene_npc_pain_audio[7];}
    else if(state.death_descriptor==-1 && selections==rf_scene_npc_fatal_audio[6])++rf_scene_npc_fatal_audio[7];
}
#endif
