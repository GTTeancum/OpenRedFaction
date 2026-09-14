#include "rf/event.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"transition line %u\n",(unsigned)__LINE__);return 1;}}while(0)
static int enqueue(void *context,const rf_level_event *event,uint32_t source,uint32_t actor)
{return rf_level_transition_enqueue(context,event,source,actor);}
typedef struct resolver_context {const char *directory;uint32_t count;} resolver_context;
static int verify_destination(const rf_vpp_entry *entry,void *context)
{
    resolver_context *c=context;rf_level level={0};rf_vpp archive={0};size_t n=strlen(entry->name);
    if(n<4 || strcmp(entry->name+n-4,".rfl"))return RF_OK;
    CHECK(rf_level_campaign_open(&level,&archive,c->directory,entry->name)==RF_OK);
    CHECK(level.archive==&archive && level.entry.size==entry->size && level.section_count);
    rf_vpp_close(&archive);c->count++;return RF_OK;
}
int main(int argc,char **argv)
{
    rf_level_transition_request request={0},saved;rf_level_owned_event authored={0};
    rf_runtime_event event={0};rf_runtime_events events={0};rf_runtime_triggers triggers={0};
    rf_object_registry registry;rf_physics_gravity gravity={0};rf_startup_events_report report;uint32_t pending,i,j,total=0;
    strcpy(authored.record.texts[0],"L1S2");strcpy(authored.record.texts[1],"entry");authored.record.uid=77;authored.record.words[0]=123;
    CHECK(rf_level_transition_enqueue(&request,&authored.record,7,9)==RF_OK && !strcmp(request.level,"L1S2.rfl") && request.uid==77 && request.source==7 && request.actor==9 && request.words[0]==123);
    saved=request;strcpy(authored.record.texts[0],"L1S3.rfl");CHECK(rf_level_transition_enqueue(&request,&authored.record,8,10)==RF_OK && !memcmp(&request,&saved,sizeof(saved)));
    memset(&request,0,sizeof(request));saved=request;strcpy(authored.record.texts[0],"../L1S2");CHECK(rf_level_transition_enqueue(&request,&authored.record,0,0)==RF_FORMAT && !memcmp(&request,&saved,sizeof(saved)));
    strcpy(authored.record.texts[0],"L1S2.RFL");rf_object_registry_init(&registry);triggers.registry=&registry;
    event.object_kind=6;event.authored=&authored;event.state.type=22;event.state.deadline=-1;
    CHECK(rf_object_registry_insert(&registry,&event,&event.handle)==RF_OK);
    CHECK(rf_runtime_event_fire(&triggers,event.handle,7,9,100,&gravity,0,0,&report)==RF_OK && report.unsupported_actions==1 && !request.pending);
    triggers.load_level=enqueue;triggers.load_level_context=&request;event.state.delay=.25f;event.state.flags=0;
    events.registry=&registry;events.items=&event;events.count=1;
    CHECK(rf_runtime_event_fire(&triggers,event.handle,7,9,100,&gravity,0,0,&report)==RF_OK && !request.pending);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,349,0,0,&report,&pending)==RF_OK && !request.pending && !pending);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,350,0,0,&report,&pending)==RF_OK && request.pending && !pending && !strcmp(request.level,"L1S2.rfl"));
    CHECK(argc==2);
    for(j=1;j<=3;j++) {
        rf_vpp archive={0};rf_level level;rf_runtime_events owned={0};char path[1024],name[32];
        snprintf(path,sizeof(path),"%s/levels1.vpp",argv[1]);snprintf(name,sizeof(name),"L1S%u.rfl",j);
        CHECK(rf_vpp_open(&archive,path)==RF_OK && rf_level_open(&level,&archive,name)==RF_OK);
        rf_object_registry_init(&registry);CHECK(rf_runtime_events_open(&level,&registry,1024*1024,&owned)==RF_OK);
        for(i=0;i<owned.count;i++)if(owned.items[i].state.type==22) {
            rf_vpp_entry target;memset(&request,0,sizeof(request));
            CHECK(rf_runtime_event_fire(&triggers,owned.items[i].handle,7,9,100,&gravity,0,0,&report)==RF_OK);
            if(!request.pending)CHECK(rf_runtime_events_tick(&owned,&triggers,&gravity,owned.items[i].state.deadline,0,0,&report,&pending)==RF_OK);
            CHECK(request.pending && request.uid==owned.items[i].authored->record.uid && !report.unsupported_actions);
            CHECK(rf_vpp_find(&archive,request.level,&target)==RF_OK);
            {rf_vpp destination={0};rf_level next;float offset[3],saved_offset[3];rf_level_transition_request bad;
             CHECK(rf_level_campaign_open(&next,&destination,argv[1],request.level)==RF_OK);
             CHECK(rf_level_transition_offset(&request,&next,offset)==RF_OK);
             printf("ARRIVAL %s %u anchor=%s delta=%.6g,%.6g,%.6g\n",name,request.uid,request.anchor,offset[0],offset[1],offset[2]);
             if(j==1 && request.uid==9019)CHECK(fabsf(offset[0]+144)<.001f && fabsf(offset[1]+32)<.001f && fabsf(offset[2]+48)<.001f);
             if(j==2 && request.uid==9346)CHECK(fabsf(offset[0]-144)<.001f && fabsf(offset[1]-32)<.001f && fabsf(offset[2]-48)<.001f);
             memcpy(saved_offset,offset,sizeof(offset));bad=request;strcpy(bad.anchor,"missing-exit-anchor");
             CHECK(rf_level_transition_offset(&bad,&next,offset)==RF_NOT_FOUND && !memcmp(offset,saved_offset,sizeof(offset)));
             bad=request;bad.anchor_position[0]=NAN;
             CHECK(rf_level_transition_offset(&bad,&next,offset)==RF_FORMAT && !memcmp(offset,saved_offset,sizeof(offset)));
             rf_vpp_close(&destination);}

            printf("EXIT %s uid=%u target=%s entrance=%s\n",name,request.uid,request.level,request.entrance);total++;
            {const rf_level_event *e=&owned.items[i].authored->record;
             printf("EXIT_DATA %s %u words=%u,%u flags=%u,%u values=%.6g,%.6g position=%.6g,%.6g,%.6g orientation=%u links=%u name=%s\n",
                 name,e->uid,e->words[0],e->words[1],e->flags[0],e->flags[1],e->values[0],e->values[1],
                 e->position[0],e->position[1],e->position[2],e->has_orientation,e->link_count,e->name);}

        }
        saved=request;rf_runtime_events_close(&owned);rf_vpp_close(&archive);CHECK(!memcmp(&saved,&request,sizeof(saved)));
    }
    {
        resolver_context context={argv[1],0};rf_vpp archive={0};rf_level level={0},before;char path[1024];
        for(j=1;j<=3;j++) {
            snprintf(path,sizeof(path),"%s/levels%u.vpp",argv[1],j);
            CHECK(rf_vpp_open(&archive,path)==RF_OK);
            CHECK(rf_vpp_visit(&archive,verify_destination,&context)==RF_OK);
            rf_vpp_close(&archive);
        }
        before=level;
        CHECK(rf_level_campaign_open(&level,&archive,argv[1],"absent-transition-target.rfl")==RF_NOT_FOUND);
        CHECK(!archive.stream && !memcmp(&level,&before,sizeof(level)));
        CHECK(rf_level_campaign_open(&level,&archive,"missing-campaign-directory","L1S1.rfl")==RF_IO);
        CHECK(!archive.stream && !memcmp(&level,&before,sizeof(level)));
        CHECK(rf_level_campaign_open(&level,&archive,argv[1],"L1S1.rfl")==RF_OK);
        before=level;
        CHECK(rf_level_campaign_open(&level,&archive,argv[1],"L1S2.rfl")==RF_RANGE);
        CHECK(archive.stream && !memcmp(&level,&before,sizeof(level)));
        rf_vpp_close(&archive);
        snprintf(path,sizeof(path),"%s/",argv[1]);
        CHECK(rf_level_campaign_open(&level,&archive,path,"l1s1.RFL")==RF_OK);
        rf_vpp_close(&archive);CHECK(context.count>3);
        printf("PASS %u campaign destinations across three archives, ownership and failure rollback\n",context.count);
    }
    CHECK(total>0);printf("PASS %u authored exits, delay, first-request ownership and validation\n",total);return 0;
}
