#ifndef RF_SCENE_WORLD_MISSION_RESTORE_INCLUDED
#define RF_SCENE_WORLD_MISSION_RESTORE_INCLUDED
/* Paused-scene staging for mission stores and live trigger/pickup bindings.
 * Caller validates the entire world before assignment-only publication. */
typedef struct scene_world_mission_trigger {rf_runtime_trigger before,next;} scene_world_mission_trigger;
typedef struct scene_world_mission_pickup {uint32_t before_slot,next_slot,uid;uint8_t before_taken,next_taken;} scene_world_mission_pickup;
typedef struct scene_world_mission_stage {
    rf_campaign_goals goals;rf_campaign_local_goals local;
    rf_campaign_pickups pickups,startup;rf_campaign_triggers triggers;
    scene_stream *stream;rf_runtime_trigger *trigger_owner;
    uint32_t trigger_count,pickup_count,bytes;uint8_t *taken_owner;uint32_t *slot_owner;
    scene_world_mission_trigger *live_triggers;scene_world_mission_pickup *live_pickups;
} scene_world_mission_stage;
static inline void scene_world_mission_close(scene_world_mission_stage **out)
{if(out){free(*out);*out=NULL;}}
static int scene_world_mission_key(const char (*levels)[64],uint32_t nl,const rf_campaign_object_record *items,
    uint32_t count,uint32_t uid,uint32_t *slot)
{
    uint32_t i,found=UINT32_MAX;
    for(i=0;i<count;i++){
        if(items[i].level>=nl)return RF_FORMAT;
        if(items[i].uid==uid&&scene_npc_checkpoint_level_equal(levels[items[i].level],campaign_current_level)){
            if(found!=UINT32_MAX)return RF_FORMAT;found=i;
        }
    }
    if(found==UINT32_MAX)return RF_NOT_FOUND;*slot=found;return RF_OK;
}
static inline int scene_world_mission_prepare(scene_stream *stream,const rf_world_checkpoint *world,int32_t now,
    uint32_t budget,scene_world_mission_stage **out)
{
    scene_world_mission_stage *s=NULL;const rf_world_checkpoint_slice *v;uint32_t i,j,slot;uint64_t cost;int status;
    if(!out||*out||!stream||!world||now<0||now>RF_TIMER_PERIOD||
       campaign_triggers.count>RF_CAMPAIGN_TRIGGER_SLOTS||stream->pickups.count>RF_CAMPAIGN_PICKUP_SLOTS||
       (campaign_triggers.count&&!campaign_triggers.items)||
       (stream->pickups.count&&(!stream->pickups.items||!stream->pickup_slots||!stream->pickup_taken)))return RF_RANGE;
    cost=sizeof(*s)+(uint64_t)campaign_triggers.count*sizeof(*s->live_triggers)+(uint64_t)stream->pickups.count*sizeof(*s->live_pickups);
    if(cost>budget||cost>UINT32_MAX)return RF_RANGE;
    s=calloc(1,(size_t)cost);if(!s)return RF_IO;s->bytes=(uint32_t)cost;
    s->stream=stream;s->trigger_owner=campaign_triggers.items;s->trigger_count=campaign_triggers.count;s->pickup_count=stream->pickups.count;
    s->taken_owner=stream->pickup_taken;s->slot_owner=stream->pickup_slots;
    s->live_triggers=(scene_world_mission_trigger*)(s+1);s->live_pickups=(scene_world_mission_pickup*)(s->live_triggers+s->trigger_count);
    v=world->sections+RF_WORLD_GOALS-1;status=rf_campaign_goals_checkpoint_decode(v->data,v->bytes,world->identity,&s->goals,&s->local);if(status)goto fail;
    v=world->sections+RF_WORLD_PICKUPS-1;status=rf_campaign_pickups_checkpoint_decode(v->data,v->bytes,world->identity,&s->pickups);if(status)goto fail;
    v=world->sections+RF_WORLD_STARTUP-1;status=rf_campaign_pickups_checkpoint_decode(v->data,v->bytes,world->identity,&s->startup);if(status)goto fail;
    v=world->sections+RF_WORLD_TRIGGER-1;status=rf_campaign_triggers_checkpoint_decode(v->data,v->bytes,world->identity,&s->triggers);if(status)goto fail;
    for(i=0;i<s->trigger_count;i++){
        scene_world_mission_trigger *r=s->live_triggers+i;rf_runtime_trigger *t=campaign_triggers.items+i;
        if(!t->authored||rf_object_registry_lookup(&campaign_registry,t->handle)!=t){status=RF_FORMAT;goto fail;}
        for(j=0;j<i;j++)if(campaign_triggers.items[j].authored->record.uid==t->authored->record.uid){status=RF_FORMAT;goto fail;}
        status=scene_world_mission_key(s->triggers.levels,s->triggers.level_count,s->triggers.items,s->triggers.count,(uint32_t)t->authored->record.uid,&slot);if(status)goto fail;
        if(!s->triggers.items[slot].retired){status=RF_FORMAT;goto fail;}
        r->before=r->next=*t;status=rf_runtime_trigger_restore(&r->next,now,s->triggers.states+slot);if(status)goto fail;
    }
    for(i=0;i<s->pickup_count;i++){
        scene_world_mission_pickup *r=s->live_pickups+i;r->uid=(uint32_t)stream->pickups.items[i].uid;
        for(j=0;j<i;j++)if(s->live_pickups[j].uid==r->uid){status=RF_FORMAT;goto fail;}
        status=scene_world_mission_key(s->pickups.levels,s->pickups.level_count,s->pickups.items,s->pickups.count,r->uid,&slot);
        /* Unsupported authored item classes have no runtime pickup owner and
         * are never registered by campaign_pickups_restore. Preserve that
         * source-bound absence; supported missing records still reject. */
        if(status==RF_NOT_FOUND && pickup_class(stream->pickups.items[i].class_name)<0 &&
           !stream->pickup_taken[i] && !stream->pickup_slots[i]){
            r->before_slot=r->next_slot=0;r->before_taken=r->next_taken=0;continue;
        }
        if(status)goto fail;
        r->before_slot=stream->pickup_slots[i];r->before_taken=stream->pickup_taken[i];r->next_slot=slot;r->next_taken=(uint8_t)s->pickups.items[slot].retired;
    }
    *out=s;return RF_OK;
fail:free(s);return status;
}
static inline int scene_world_mission_validate(const scene_world_mission_stage *s)
{
    uint32_t i;if(!s||!s->stream||s->trigger_owner!=campaign_triggers.items||s->trigger_count!=campaign_triggers.count||
        s->stream->pickups.count!=s->pickup_count||s->taken_owner!=s->stream->pickup_taken||s->slot_owner!=s->stream->pickup_slots)return RF_FORMAT;
    for(i=0;i<s->trigger_count;i++)if(memcmp(s->trigger_owner+i,&s->live_triggers[i].before,sizeof(rf_runtime_trigger))||
        rf_object_registry_lookup(&campaign_registry,s->trigger_owner[i].handle)!=s->trigger_owner+i)return RF_FORMAT;
    for(i=0;i<s->pickup_count;i++)if(s->live_pickups[i].uid!=(uint32_t)s->stream->pickups.items[i].uid||
        s->live_pickups[i].before_slot!=s->slot_owner[i]||s->live_pickups[i].before_taken!=s->taken_owner[i])return RF_FORMAT;
    return RF_OK;
}
/* No callbacks, dispatch or fallible operations. Must follow full-world validation. */
static inline void scene_world_mission_assign(const scene_world_mission_stage *s)
{
    uint32_t i;rf_scene_mission_goals=s->goals;campaign_local_goals=s->local;
    rf_scene_campaign_pickups=s->pickups;campaign_startup_inventory=s->startup;campaign_trigger_history=s->triggers;
    for(i=0;i<s->trigger_count;i++)s->trigger_owner[i]=s->live_triggers[i].next;
    for(i=0;i<s->pickup_count;i++){s->slot_owner[i]=s->live_pickups[i].next_slot;s->taken_owner[i]=s->live_pickups[i].next_taken;}
}
static inline int scene_world_mission_probe(scene_stream *stream,const rf_world_checkpoint *world,int32_t now)
{
#ifndef RF_IMAGE_XBOX_NATIVE
    scene_world_mission_stage *stage=NULL;int status;const char *enabled=getenv("RF_REPLAY_WORLD_RESTORE_PROBE");
    if(!enabled||strcmp(enabled,"1"))return RF_OK;
    status=scene_world_mission_prepare(stream,world,now,1024u*1024u,&stage);
    if(!status)status=scene_world_mission_validate(stage);
    printf("WORLD_RESTORE_MISSION status%d bytes%u triggers%u pickups%u no_publication1\n",status,stage?stage->bytes:0,stage?stage->trigger_count:0,stage?stage->pickup_count:0);
    scene_world_mission_close(&stage);return status;
#else
    (void)stream;(void)world;(void)now;return RF_OK;
#endif
}
#endif
