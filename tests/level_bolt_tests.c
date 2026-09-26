#include "rf/level.h"
#include "rf/event.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"bolt line %d: %s\n",__LINE__,#x);return 1;}} while(0)
static uint32_t dispatched[8],dispatch_count,dispatch_on;
static int set_bolts(void *context,const uint32_t *uids,uint32_t count,uint32_t on)
{
    uint32_t i;(void)context;if(count>8)return RF_RANGE;
    dispatch_count=count;dispatch_on=on;
    for(i=0;i<count;i++)dispatched[i]=uids[i];
    return RF_OK;
}
static int scan(const char *path,const char *archive_name,const char *level_name,
    uint32_t expected,uint32_t selected_uid,uint32_t selected_target)
{
    char filename[1024];rf_vpp archive={0};rf_level level;rf_level_bolt_reader reader;
    rf_level_bolt bolt;rf_level_target_reader target_reader;rf_level_target target;
    uint32_t count=0,found=0,sources[256],endpoints[256],resolved[256]={0},target_count=0,i;int status;
    snprintf(filename,sizeof(filename),"%s/%s",path,archive_name);
    CHECK(rf_vpp_open(&archive,filename)==RF_OK);
    CHECK(rf_level_open(&level,&archive,level_name)==RF_OK);
    CHECK(rf_level_bolts_begin(&level,&reader)==RF_OK);
    CHECK(reader.count==expected);
    while((status=rf_level_bolt_next(&reader,&bolt))==RF_OK) {
        CHECK(bolt.bytes>=108 && bolt.uid && bolt.target_uid);
        CHECK(!strcmp(bolt.name,"Bolt Emitter"));
        if(bolt.uid==selected_uid){CHECK(bolt.target_uid==selected_target);found=1;}
        CHECK(count<256);sources[count]=bolt.uid;endpoints[count]=bolt.target_uid;
        ++count;
    }
    CHECK(status==RF_NOT_FOUND && count==expected && found);
    CHECK(rf_level_targets_begin(&level,&target_reader)==RF_OK);
    while((status=rf_level_target_next(&target_reader,&target))==RF_OK){
        CHECK(!strcmp(target.name,"Target") && target.bytes>=57);
        for(i=0;i<count;i++)if(endpoints[i]==target.uid)resolved[i]=1;
        ++target_count;
    }
    CHECK(status==RF_NOT_FOUND && target_count);
    for(i=0;i<count;i++)CHECK(resolved[i]);
    {rf_level_owned_groups groups={0};uint32_t g,l,j,attached=0;
        CHECK(rf_level_owned_groups_open(&level,1024*1024,&groups)==RF_OK);
        for(g=0;g<groups.count;g++)for(l=0;l<2;l++)for(j=0;j<groups.groups[g].record.ids_count[l];j++){
            uint32_t uid=groups.groups[g].ids[l][j],k;
            for(k=0;k<count;k++)if(endpoints[k]==uid || sources[k]==uid)++attached;
        }
        CHECK(attached==0);
        rf_level_owned_groups_close(&groups);
    }
    if(!strcmp(level_name,"L1S2.rfl")) {
        rf_object_registry registry;rf_runtime_events events={0};rf_runtime_triggers triggers={0};
        rf_runtime_event *event=NULL;rf_physics_gravity gravity={0};rf_startup_events_report report;
        uint32_t i;
        rf_object_registry_init(&registry);
        CHECK(rf_runtime_events_open(&level,&registry,1024*1024,&events)==RF_OK);
        for(i=0;i<events.count;i++)if(events.items[i].authored->record.uid==8636)event=events.items+i;
        CHECK(event && event->state.type==43 && event->authored->record.link_count==4);
        triggers.registry=&registry;triggers.bolt_state=set_bolts;
        CHECK(rf_physics_gravity_set(&gravity,9.8f)==RF_OK);
        CHECK(rf_runtime_event_fire(&triggers,event->handle,UINT32_MAX,UINT32_MAX,0,
            &gravity,NULL,NULL,&report)==RF_OK);
        CHECK(dispatch_count==4 && dispatch_on==1);
        CHECK(dispatched[0]==8298 && dispatched[1]==8300 && dispatched[2]==8294 && dispatched[3]==8296);
        rf_runtime_events_close(&events);
    }
    rf_vpp_close(&archive);
    return 0;
}
int main(int argc,char **argv)
{
    CHECK(argc==2);
    CHECK(!scan(argv[1],"levels1.vpp","L1S2.rfl",4,8298,8299));
    CHECK(!scan(argv[1],"levels2.vpp","L7S1.rfl",15,3544,3657));
    CHECK(!scan(argv[1],"levels3.vpp","L17S1.rfl",16,19994,19979));
    puts("PASS authored bolt records in L1S2/L7S1/L17S1");
    return 0;
}
