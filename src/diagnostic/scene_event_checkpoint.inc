#ifndef RF_SCENE_EVENT_CHECKPOINT_INCLUDED
#define RF_SCENE_EVENT_CHECKPOINT_INCLUDED
#include "rf/event_checkpoint.h"
#include <stdlib.h>
#include <string.h>
/* Explicit scene bindings: campaign_events/campaign_registry and the complete
 * UID object map assembled by campaign link resolution. Caller retains the map
 * through capture/prepare only. No synthetic UID, handle or missing-object fixup.
 * Mandatory admission callback is pure and certifies associated external state
 * is captured/settled; this adapter cannot infer NPC, message, audio or alarm safety.
 * Calls and commit occur at a paused scene boundary without concurrent mutation. */
typedef int (*scene_event_checkpoint_admit)(void *,const rf_runtime_event *);
typedef struct scene_event_checkpoint_map {
    const rf_level_uid_object *objects;uint32_t count;rf_object_registry *registry;
} scene_event_checkpoint_map;
typedef struct scene_event_checkpoint_blob {unsigned char *data;uint32_t bytes,count;} scene_event_checkpoint_blob;
typedef struct scene_event_checkpoint_entry {
    rf_runtime_event before,next;rf_switch_state switch_before,switch_next;
    rf_runtime_event *target;void *source_owner,*actor_owner;
} scene_event_checkpoint_entry;
typedef struct scene_event_checkpoint_stage {
    scene_event_checkpoint_entry *entries;uint32_t count,bytes;
    rf_runtime_events *owner;rf_runtime_event *items;rf_object_registry *registry;
    scene_event_checkpoint_admit admit;void *admission_context;
} scene_event_checkpoint_stage;
static void scene_event_checkpoint_blob_close(scene_event_checkpoint_blob *b)
{if(b){free(b->data);memset(b,0,sizeof(*b));}}
static void scene_event_checkpoint_close(scene_event_checkpoint_stage *s)
{if(s){free(s->entries);memset(s,0,sizeof(*s));}}
static uint32_t scene_event_checkpoint_word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int scene_event_checkpoint_map_valid(const scene_event_checkpoint_map *m)
{
    uint32_t i,j;if(!m||!m->registry||m->count>RF_OBJECT_CAPACITY||(m->count&&!m->objects))return RF_RANGE;
    for(i=0;i<m->count;i++){
        if(m->objects[i].uid==UINT32_MAX)return RF_FORMAT;
        for(j=0;j<i;j++)if(m->objects[i].uid==m->objects[j].uid||m->objects[i].handle==m->objects[j].handle)return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_event_checkpoint_export(void *context,uint32_t handle,uint32_t *uid)
{
    const scene_event_checkpoint_map *m=context;uint32_t i;
    if(!rf_object_registry_lookup(m->registry,handle))return RF_NOT_FOUND;
    for(i=0;i<m->count;i++)if(m->objects[i].handle==handle){*uid=m->objects[i].uid;return RF_OK;}
    return RF_NOT_FOUND;
}
static int scene_event_checkpoint_import(void *context,uint32_t uid,uint32_t *handle)
{
    const scene_event_checkpoint_map *m=context;uint32_t i;
    for(i=0;i<m->count;i++)if(m->objects[i].uid==uid){
        if(!rf_object_registry_lookup(m->registry,m->objects[i].handle))return RF_NOT_FOUND;
        *handle=m->objects[i].handle;return RF_OK;}
    return RF_NOT_FOUND;
}
static int scene_event_checkpoint_owners(const rf_runtime_events *events,
    const scene_event_checkpoint_map *map,scene_event_checkpoint_admit admit,void *context)
{
    uint32_t i,j;int status;
    if(!events||events->registry!=map->registry||events->count>RF_OBJECT_CAPACITY||
       (events->count&&(!events->items||!admit)))return RF_RANGE;
    for(i=0;i<events->count;i++){
        const rf_runtime_event *e=events->items+i;void *registered;
        if(!e->authored||e->object_kind!=6||e->retired>1)return RF_FORMAT;
        registered=rf_object_registry_lookup(map->registry,e->handle);
        if((e->retired&&registered)||(!e->retired&&registered!=e))return RF_FORMAT;
        for(j=0;j<i;j++)if(events->items[j].authored->record.uid==e->authored->record.uid||events->items[j].handle==e->handle)return RF_FORMAT;
        status=admit(context,e);if(status)return status;
    }
    return RF_OK;
}
static inline int scene_event_checkpoint_capture(const rf_runtime_events *events,const scene_event_checkpoint_map *map,
    const unsigned char identity[32],int32_t now,scene_event_checkpoint_admit admit,void *context,
    uint32_t budget,scene_event_checkpoint_blob *out)
{
    scene_event_checkpoint_blob b={0};rf_event_checkpoint_refs refs;uint32_t i;int status;
    if(!out||out->data||out->bytes||out->count||!identity||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    status=scene_event_checkpoint_map_valid(map);if(status)return status;
    status=scene_event_checkpoint_owners(events,map,admit,context);if(status)return status;
    b.count=events->count;b.bytes=b.count*RF_EVENT_CHECKPOINT_BYTES;if(b.bytes>budget)return RF_RANGE;
    if(b.bytes){b.data=malloc(b.bytes);if(!b.data)return RF_IO;}
    refs=(rf_event_checkpoint_refs){scene_event_checkpoint_export,scene_event_checkpoint_import,(void *)map};
    for(i=0;i<b.count;i++){
        status=rf_event_checkpoint_encode_mapped(identity,events->items+i,now,&refs,
            b.data+i*RF_EVENT_CHECKPOINT_BYTES,RF_EVENT_CHECKPOINT_BYTES);
        if(status){scene_event_checkpoint_blob_close(&b);return status;}
    }
    *out=b;return RF_OK;
}
static inline int scene_event_checkpoint_prepare(rf_runtime_events *events,const scene_event_checkpoint_map *map,
    const unsigned char identity[32],const void *data,uint32_t bytes,int32_t now,
    scene_event_checkpoint_admit admit,void *context,uint32_t budget,scene_event_checkpoint_stage *out)
{
    scene_event_checkpoint_stage s={0};rf_event_checkpoint_refs refs;const unsigned char *p=data;
    uint32_t i,j,uid;uint64_t cost;int status;
    if(!out||out->entries||out->count||out->bytes||out->owner||out->items||out->registry||out->admit||
       out->admission_context||!identity||(bytes&&!data)||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    status=scene_event_checkpoint_map_valid(map);if(status)return status;
    status=scene_event_checkpoint_owners(events,map,admit,context);if(status)return status;
    if(bytes!=events->count*RF_EVENT_CHECKPOINT_BYTES)return RF_FORMAT;
    cost=(uint64_t)events->count*sizeof(*s.entries);if(cost>budget||cost>UINT32_MAX)return RF_RANGE;
    if(cost){s.entries=calloc(events->count,sizeof(*s.entries));if(!s.entries)return RF_IO;}
    s.count=events->count;s.bytes=(uint32_t)cost;s.owner=events;s.items=events->items;s.registry=events->registry;s.admit=admit;s.admission_context=context;
    refs=(rf_event_checkpoint_refs){scene_event_checkpoint_export,scene_event_checkpoint_import,(void *)map};
    for(i=0;i<s.count;i++){
        scene_event_checkpoint_entry *row=s.entries+i;const unsigned char *packet=p+i*RF_EVENT_CHECKPOINT_BYTES;
        uid=scene_event_checkpoint_word(packet+16);
        for(j=0;j<events->count;j++)if(events->items[j].authored->record.uid==uid)break;
        if(j==events->count){status=RF_FORMAT;goto failed;}
        row->target=events->items+j;
        for(j=0;j<i;j++)if(s.entries[j].target==row->target){status=RF_FORMAT;goto failed;}
        row->before=row->next=*row->target;
        if(row->target->switch_state){row->switch_before=row->switch_next=*row->target->switch_state;row->next.switch_state=&row->switch_next;}
        status=rf_event_checkpoint_restore_mapped(packet,RF_EVENT_CHECKPOINT_BYTES,identity,&row->next,now,&refs);if(status)goto failed;
        if(row->before.retired&&!row->next.retired){status=RF_NOT_FOUND;goto failed;} /* Requires fresh registered owner to resurrect. */
        row->source_owner=rf_object_registry_lookup(s.registry,row->next.state.source);
        row->actor_owner=rf_object_registry_lookup(s.registry,row->next.state.actor);
    }
    /* A real reference cannot target an event removed by this same restore. */
    for(i=0;i<s.count;i++)for(j=0;j<s.count;j++)if(s.entries[j].next.retired&&
        (s.entries[i].source_owner==s.entries[j].target||s.entries[i].actor_owner==s.entries[j].target)){
        status=RF_NOT_FOUND;goto failed;}
    *out=s;return RF_OK;
failed:scene_event_checkpoint_close(&s);return status;
}
static inline int scene_event_checkpoint_validate(scene_event_checkpoint_stage *s)
{
    uint32_t i,retiring=0;int status;
    if(!s||!s->owner||!s->registry||(s->count&&!s->entries)||s->owner->items!=s->items||
       s->owner->count!=s->count||s->owner->registry!=s->registry)return RF_RANGE;
    for(i=0;i<s->count;i++){
        scene_event_checkpoint_entry *r=s->entries+i;void *registered=rf_object_registry_lookup(s->registry,r->before.handle);
        if(memcmp(r->target,&r->before,sizeof(r->before))||
           (r->before.switch_state&&memcmp(r->before.switch_state,&r->switch_before,sizeof(r->switch_before)))||
           (r->before.retired?registered!=NULL:registered!=r->target)||
           rf_object_registry_lookup(s->registry,r->next.state.source)!=r->source_owner||
           rf_object_registry_lookup(s->registry,r->next.state.actor)!=r->actor_owner)return RF_FORMAT;
        status=s->admit(s->admission_context,r->target);if(status)return status;
        if(!r->before.retired&&r->next.retired)++retiring;
    }
    if(s->registry->count>RF_OBJECT_CAPACITY||retiring>RF_OBJECT_CAPACITY-s->registry->count)return RF_FORMAT;
    return RF_OK;
}
/* Assignment only. Caller must validate every composed stage first, then keep
 * scene owners and registry stable until all assignments finish. No callbacks
 * or gameplay dispatch; allocation and stage disposal remain caller-owned. */
static inline void scene_event_checkpoint_assign(scene_event_checkpoint_stage *s)
{
    uint32_t i;
    /* All operations below are non-failing under the stable-scene contract. */
    for(i=0;i<s->count;i++){
        scene_event_checkpoint_entry *r=s->entries+i;rf_switch_state *owned=r->before.switch_state;
        if(!r->before.retired&&r->next.retired)(void)rf_object_registry_remove(s->registry,r->before.handle);
        *r->target=r->next;r->target->switch_state=owned;if(owned)*owned=r->switch_next;
    }
}
static inline int scene_event_checkpoint_commit(scene_event_checkpoint_stage *s)
{
    int status=scene_event_checkpoint_validate(s);if(status)return status;
    scene_event_checkpoint_assign(s);scene_event_checkpoint_close(s);return RF_OK;
}
#endif
