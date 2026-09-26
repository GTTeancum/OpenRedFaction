#ifndef RF_SCENE_WORLD_EVENT_RESTORE_INCLUDED
#define RF_SCENE_WORLD_EVENT_RESTORE_INCLUDED
/* Include after scene_event_checkpoint, scene_world_restore and UID-map helper.
 * Structural/event staging is distinct from external gameplay publication.
 * The snapshot does not contain gravity/force state or transition requests yet.
 * Missing those is never certified by merely restoring an event's scheduler. */
enum {
    SCENE_WORLD_EVENT_PRESENTATION_MESSAGE=1u,
    SCENE_WORLD_EVENT_PRESENTATION_ALARM=2u,
    SCENE_WORLD_EVENT_MISSING_GRAVITY=4u,
    SCENE_WORLD_EVENT_MISSING_FORCES=8u,
    SCENE_WORLD_EVENT_MISSING_TRANSITION=16u,
    SCENE_WORLD_EVENT_PENDING_UNIMPLEMENTED=32u,
    SCENE_WORLD_EVENT_RETIRED_NPC_REFERENCE=64u,
    SCENE_WORLD_EVENT_NEEDS_PLAYER_MISSION=128u,
    SCENE_WORLD_EVENT_PARTICLE_PRESENTATION=256u
};
#define SCENE_WORLD_EVENT_PUBLICATION_BLOCKERS (SCENE_WORLD_EVENT_MISSING_GRAVITY|SCENE_WORLD_EVENT_MISSING_FORCES|SCENE_WORLD_EVENT_MISSING_TRANSITION|SCENE_WORLD_EVENT_PENDING_UNIMPLEMENTED|SCENE_WORLD_EVENT_RETIRED_NPC_REFERENCE|SCENE_WORLD_EVENT_NEEDS_PLAYER_MISSION)
typedef struct scene_world_event_stage {
    scene_event_checkpoint_stage events;
    const scene_world_restore_stage *world;
    uint32_t flags,bytes,failed_uid,failed_type;
} scene_world_event_stage;
static inline void scene_world_event_close(scene_world_event_stage **out)
{
    if(out&&*out){scene_event_checkpoint_close(&(*out)->events);free(*out);*out=NULL;}
}
static int scene_world_event_admit(void *context,const rf_runtime_event *event)
{
    scene_world_event_stage *s=context;uint32_t requirements;
    if(!s||!s->world||!s->world->npcs||!event||!event->authored)return RF_RANGE;
    requirements=rf_event_checkpoint_external_requirements(event->state.type);
    /* Existence of an inert unsupported authored event is not an active effect.
     * An already queued dispatch is a concrete unsupported continuation. */
    if((requirements&RF_EVENT_CHECKPOINT_EXTERNAL_UNIMPLEMENTED)&&!event->retired&&event->state.deadline>=0){
        s->flags|=SCENE_WORLD_EVENT_PENDING_UNIMPLEMENTED;
        s->failed_uid=event->authored->record.uid;s->failed_type=event->state.type;return RF_NOT_FOUND;
    }
    if(requirements&(RF_EVENT_CHECKPOINT_EXTERNAL_INVENTORY|RF_EVENT_CHECKPOINT_EXTERNAL_GOALS|RF_EVENT_CHECKPOINT_EXTERNAL_DAMAGE))
        s->flags|=SCENE_WORLD_EVENT_NEEDS_PLAYER_MISSION;
    /* NPC animation, vitals, mode and pose are validated by the candidate NPC
     * stage; active unsaved routes/combat are rejected there. Never fire an
     * event to reconstruct its effects, or restart the staged animation. */
    return RF_OK;
}
static inline int scene_world_event_prepare(scene_stream *stream,const rf_world_checkpoint *envelope,int32_t now,
    const scene_world_restore_stage *world,uint32_t budget,scene_world_event_stage **out,uint32_t *diagnostics)
{
    scene_world_event_stage *s=NULL;rf_level_uid_object *objects=NULL;scene_event_checkpoint_map map;
    const rf_world_checkpoint_slice *v;uint32_t count=0,i,j;uint64_t used;int status;
    if(!stream||!envelope||!world||!world->npcs||!out||*out||!diagnostics||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(memcmp(envelope->identity,world->envelope.identity,32)||strcmp(envelope->level,world->envelope.level))return RF_FORMAT;
    if(!(world->semantic_mask&RF_WORLD_CHECKPOINT_MASK(RF_WORLD_NPC)))return RF_RANGE;
    if(budget<sizeof(*s))return RF_RANGE;
    s=calloc(1,sizeof(*s));if(!s)return RF_IO;s->world=world;s->failed_uid=UINT32_MAX;
    /* These exact globals need a composed sidecar before full publication:
     * scene_gravity; campaign_forces.items[].active; rf_scene_level_transition.
     * Gravity is flagged even at its default: no historical value is on disk. */
    s->flags=SCENE_WORLD_EVENT_MISSING_GRAVITY;
    if(campaign_forces.count)s->flags|=SCENE_WORLD_EVENT_MISSING_FORCES;
    if(rf_scene_level_transition.pending)s->flags|=SCENE_WORLD_EVENT_MISSING_TRANSITION;
    /* Presentation is explicitly deferred in the first pass, not a save veto.
     * campaign_subtitle + deadline/uid/message_voice and alarm voice/deadline
     * are not restored. NPC alarm consequences are already staged in RFNC. */
    if(campaign_subtitle_deadline>=0||campaign_message_voice>=0)s->flags|=SCENE_WORLD_EVENT_PRESENTATION_MESSAGE;
    if(campaign_alarm_deadline>=0||campaign_alarm_voice>=0||rf_scene_alarm[4])s->flags|=SCENE_WORLD_EVENT_PRESENTATION_ALARM;
    if(stream->particles.state)s->flags|=SCENE_WORLD_EVENT_PARTICLE_PRESENTATION;
    status=scene_world_checkpoint_uid_map_open(stream,&objects,&count,budget-(uint32_t)sizeof(*s));if(status)goto done;
    used=sizeof(*s)+((uint64_t)campaign_events.count+campaign_triggers.count+campaign_group_registration.count+
        campaign_mover_count+campaign_npc_body_count+campaign_clutter_records.count+stream->pickups.count+1)*sizeof(*objects);if(used>budget){status=RF_RANGE;goto done;}
    map=(scene_event_checkpoint_map){objects,count,&campaign_registry};v=envelope->sections+RF_WORLD_EVENT-1;
    status=scene_event_checkpoint_prepare(&campaign_events,&map,envelope->identity,v->data,v->bytes,now,
        scene_world_event_admit,s,budget-(uint32_t)used,&s->events);if(status)goto done;
    for(i=0;i<s->events.count;i++){
        const scene_event_checkpoint_entry *e=s->events.entries+i;
        status=scene_world_event_admit(s,&e->next);if(status)goto done;
        for(j=0;j<world->npcs->count;j++){
            const scene_npc_checkpoint_restore_entry *npc=world->npcs->entries+j;
            if(npc->saved.retired&&npc->registered&&(e->next.state.source==npc->handle||e->next.state.actor==npc->handle)){
                s->flags|=SCENE_WORLD_EVENT_RETIRED_NPC_REFERENCE;s->failed_uid=e->next.authored->record.uid;
                s->failed_type=e->next.state.type;status=RF_NOT_FOUND;goto done;
            }
        }
    }
    s->bytes=(uint32_t)sizeof(*s)+s->events.bytes;status=RF_OK;
done:
    free(objects);*diagnostics=s->flags;
#ifndef RF_IMAGE_XBOX_NATIVE
    {const char *enabled=getenv("RF_REPLAY_WORLD_RESTORE_PROBE");
     if(enabled&&!strcmp(enabled,"1"))printf("WORLD_RESTORE_EVENTS status%d flags%u bytes%u count%u uid%u type%u no_publication1\n",
        status,s->flags,s->bytes,s->events.count,s->failed_uid,s->failed_type);}
#endif
    if(status)scene_world_event_close(&s);else *out=s;return status;
}
static inline int scene_world_event_probe(scene_stream *stream,const scene_world_restore_stage *world,int32_t now)
{
    scene_world_player_stage player;scene_world_event_stage *events=NULL;scene_world_environment_stage *environment=NULL;uint32_t flags=0;int status;
    const rf_world_checkpoint_slice *v=world->envelope.sections+RF_WORLD_ENVIRONMENT-1;
    if(world->allocated_bytes>2u*1024u*1024u)return RF_RANGE;
    status=scene_world_event_prepare(stream,&world->envelope,now,world,2u*1024u*1024u-world->allocated_bytes,&events,&flags);
    if(!status)status=scene_event_checkpoint_validate(&events->events);
    if(!status)status=scene_world_environment_prepare(world->envelope.identity,now,v->data,v->bytes,1024u*1024u,&environment);
    if(!status)status=scene_world_environment_validate(environment);
    if(!status)flags&=~(SCENE_WORLD_EVENT_MISSING_GRAVITY|SCENE_WORLD_EVENT_MISSING_FORCES|SCENE_WORLD_EVENT_MISSING_TRANSITION);
#ifndef RF_IMAGE_XBOX_NATIVE
    printf("WORLD_RESTORE_ENVIRONMENT status%d forces%u remaining_event_flags%u no_publication1\n",status,environment?environment->count:0,flags);
#endif
    scene_world_environment_close(&environment);
    if(!status)status=scene_world_player_prepare(stream,world,&player);
#ifndef RF_IMAGE_XBOX_NATIVE
    printf("WORLD_RESTORE_PLAYER status%d no_publication1\n",status);
#endif
    scene_world_event_close(&events);return status;
}
#endif
