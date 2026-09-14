#include "rf/level.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"waypoints line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static rf_vpp archive;static uint32_t levels,peak;
static int visit(const rf_vpp_entry *entry,void *context)
{
    rf_level level;rf_level_owned_navigation nav={0};const rf_level_section *s;
    unsigned char *data;rf_level_waypoint_path path={0};size_t len=strlen(entry->name);int status;(void)context;
    if(len<4 || strcmp(entry->name+len-4,".rfl"))return RF_OK;
    CHECK(rf_level_open(&level,&archive,entry->name)==RF_OK);
    s=rf_level_find(&level,0x10000);if(!s)return RF_OK;
    status=rf_level_owned_navigation_open(&level,1024*1024,&nav);CHECK(status==RF_OK || status==RF_NOT_FOUND);
    data=malloc(s->size?s->size:1);CHECK(data);
    CHECK(rf_level_read(&level,s,0,data,s->size)==RF_OK);
    status=rf_level_waypoint_find(data,s->size,nav.count,NULL,NULL);
    if(status)fprintf(stderr,"level %s status%d\n",entry->name,status);CHECK(status==RF_OK);
    if(!strcmp(entry->name,"L1S1.rfl")) {
        CHECK(rf_level_waypoint_find(data,s->size,nav.count,"L1S1MinerBuddy",&path)==RF_OK);
        CHECK(path.count==2 && rf_level_waypoint_node(&path,0)==18 && rf_level_waypoint_node(&path,1)==330);
        CHECK(nav.nodes[330].uid==9673);
    }
    ++levels;if(s->size>peak)peak=s->size;free(data);rf_level_owned_navigation_close(&nav);return RF_OK;
}
int main(int argc,char **argv)
{
    unsigned char data[]={1,0,0,0,1,0,'A',2,0,0,0,0,0,0,0,1,0,0,0};
    rf_level_waypoint_path path={0},saved;uint32_t i;char file[1024];CHECK(argc==2);
    CHECK(rf_level_waypoint_find(data,sizeof(data),2,"A",&path)==RF_OK);
    CHECK(path.count==2 && rf_level_waypoint_node(&path,1)==1 && rf_level_waypoint_node(&path,2)==UINT32_MAX);saved=path;
    for(i=0;i<sizeof(data);i++)CHECK(rf_level_waypoint_find(data,i,2,"A",&path)==RF_FORMAT && !memcmp(&saved,&path,sizeof(path)));
    CHECK(rf_level_waypoint_find(data,sizeof(data),2,"missing",&path)==RF_NOT_FOUND && !memcmp(&saved,&path,sizeof(path)));
    CHECK(rf_level_waypoint_find(data,sizeof(data),1,"A",&path)==RF_FORMAT);
    CHECK(rf_level_waypoint_find(data,sizeof(data),2,"",&path)==RF_NOT_FOUND);
    data[7]=255;CHECK(rf_level_waypoint_find(data,sizeof(data),2,"A",&path)==RF_FORMAT);data[7]=2;
    memset(data+11,255,4);CHECK(rf_level_waypoint_find(data,sizeof(data),2,NULL,NULL)==RF_OK);
    CHECK(rf_level_waypoint_find(data,sizeof(data),2,"A",&path)==RF_NOT_FOUND && !memcmp(&saved,&path,sizeof(path)));
    memset(data+11,0,4);
    data[6]=0;CHECK(rf_level_waypoint_find(data,sizeof(data),2,"A",&path)==RF_FORMAT);data[6]='A';
    for(i=1;i<=3;i++) {snprintf(file,sizeof(file),"%s/levels%u.vpp",argv[1],i);
        CHECK(rf_vpp_open(&archive,file)==RF_OK);CHECK(rf_vpp_visit(&archive,visit,NULL)==RF_OK);rf_vpp_close(&archive);}
    CHECK(levels);printf("PASS waypoint sections%u peak payload%u bytes\n",levels,peak);return 0;
}
