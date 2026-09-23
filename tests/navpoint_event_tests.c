#include "rf/event.h"
#include "rf/entity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"navpoint check failed: %s at %d\n",#x,__LINE__);return 1;}} while(0)

typedef struct nav_trace {rf_level_owned_navigation *nav;uint32_t count,indices[6],actions[6];} nav_trace;
static int nav_set(void *context,uint32_t index,uint32_t on)
{
    nav_trace *trace=context;rf_entity_navigation_candidate *candidate;
    if(index>=trace->nav->count)return RF_NOT_FOUND;
    if(trace->count>=6)return RF_RANGE;
    trace->indices[trace->count]=index;trace->actions[trace->count++]=on;
    candidate=&trace->nav->nodes[index].candidate;
    if(on)memcpy(&candidate->radius,&candidate->retained_018,4);
    else candidate->radius=0;
    return RF_OK;
}
static rf_runtime_event *by_uid(rf_runtime_events *events,uint32_t uid)
{
    uint32_t i;for(i=0;i<events->count;i++)if(events->items[i].authored->record.uid==uid)return events->items+i;
    return NULL;
}
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level={0};rf_object_registry registry;
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_level_owned_navigation nav={0};
    rf_level_uid_object *objects;rf_runtime_event *invert,*enable;
    rf_physics_gravity gravity;rf_startup_events_report report;nav_trace trace={0};uint32_t i,indices[3]={21,58,56};
    CHECK(argc==2);CHECK(rf_vpp_open(&archive,argv[1])==RF_OK);
    CHECK(rf_level_open(&level,&archive,"L6S3.rfl")==RF_OK);
    CHECK(rf_level_owned_navigation_open(&level,65536,&nav)==RF_OK);
    rf_object_registry_init(&registry);
    CHECK(rf_runtime_events_open(&level,&registry,1024*1024,&events)==RF_OK);
    objects=calloc(events.count,sizeof(*objects));CHECK(objects);
    for(i=0;i<events.count;i++){
        objects[i].uid=events.items[i].authored->record.uid;
        objects[i].handle=events.items[i].handle;
    }
    CHECK(rf_runtime_events_resolve(&events,objects,events.count,NULL,0)==RF_OK);
    CHECK(rf_runtime_events_bind_navigation(&events,&nav)==RF_OK);
    invert=by_uid(&events,7124);enable=by_uid(&events,7123);CHECK(invert&&enable);
    CHECK(enable->authored->record.link_count==3);
    for(i=0;i<3;i++){
        rf_level_link_target *link=enable->links+i;uint32_t baseline;
        CHECK(link->kind==3&&link->value==indices[i]);
        memcpy(&baseline,&nav.nodes[indices[i]].candidate.radius,4);
        CHECK(baseline==nav.nodes[indices[i]].candidate.retained_018);
    }
    triggers.registry=&registry;triggers.navpoint=nav_set;triggers.navpoint_context=&trace;trace.nav=&nav;
    CHECK(rf_physics_gravity_set(&gravity,9.8f)==RF_OK);
    CHECK(rf_runtime_event_fire(&triggers,invert->handle,UINT32_MAX,UINT32_MAX,0,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(trace.count==3);
    for(i=0;i<3;i++){
        CHECK(trace.indices[i]==indices[i]&&trace.actions[i]==0);
        CHECK(nav.nodes[indices[i]].candidate.radius==0);
        CHECK(!rf_entity_navigation_candidate_allowed(.5f,1,0,nav.nodes[indices[i]].candidate.radius,
            nav.nodes[indices[i]].candidate.height,nav.nodes[indices[i]].candidate.word_040));
    }
    CHECK(rf_runtime_event_fire(&triggers,enable->handle,UINT32_MAX,UINT32_MAX,100,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(trace.count==6);
    for(i=0;i<3;i++){
        uint32_t radius;
        CHECK(trace.indices[i+3]==indices[i]&&trace.actions[i+3]==1);
        memcpy(&radius,&nav.nodes[indices[i]].candidate.radius,4);
        CHECK(radius==nav.nodes[indices[i]].candidate.retained_018);
        CHECK(rf_entity_navigation_candidate_allowed(.5f,1,0,nav.nodes[indices[i]].candidate.radius,
            nav.nodes[indices[i]].candidate.height,nav.nodes[indices[i]].candidate.word_040));
    }
    free(objects);rf_runtime_events_close(&events);rf_level_owned_navigation_close(&nav);
    rf_vpp_close(&archive);puts("navpoint event PASS");return 0;
}
