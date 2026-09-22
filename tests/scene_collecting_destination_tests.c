#include <stdio.h>
#include "../src/diagnostic/scene_collecting_destination.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"collecting destination line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_level_owned_event items[4]={0};rf_level_owned_events events={0};
    scene_collecting_destination result,saved;float origin[3]={0,0,0};uint32_t i;
    memset(&result,0x5a,sizeof(result));saved=result;
    CHECK(scene_collecting_destination_find(&events,origin,&result)==RF_NOT_FOUND);
    CHECK(!memcmp(&result,&saved,sizeof(result)));
    events.items=items;events.count=4;
    for(i=0;i<4;i++){strcpy(items[i].record.type,"Drop_Point_Marker");items[i].record.uid=100+i;}
    strcpy(items[0].record.type,"Trigger_Event"); /* A closer unrelated event cannot win. */
    items[1].record.position[1]=9;items[2].record.position[0]=3;items[2].record.position[2]=4;
    items[3].record.position[0]=-3;items[3].record.position[2]=-4;
    CHECK(scene_collecting_destination_find(&events,origin,&result)==RF_OK);
    CHECK(result.index==2&&result.uid==102&&result.distance==5);
    CHECK(!memcmp(result.position,items[2].record.position,12)); /* 3D height and stable tie. */
    items[1].record.position[1]=0;
    CHECK(!scene_collecting_destination_find(&events,origin,&result)&&result.index==1&&result.distance==0);
    saved=result;items[3].record.position[2]=NAN;
    CHECK(scene_collecting_destination_find(&events,origin,&result)==RF_FORMAT&&!memcmp(&saved,&result,sizeof(result)));
    items[3].record.position[2]=0;origin[0]=INFINITY;
    CHECK(scene_collecting_destination_find(&events,origin,&result)==RF_FORMAT&&!memcmp(&saved,&result,sizeof(result)));
    origin[0]=0;events.items=NULL;
    CHECK(scene_collecting_destination_find(&events,origin,&result)==RF_RANGE&&!memcmp(&saved,&result,sizeof(result)));
    puts("PASS collecting drop-marker nearest 3D selection, ties, empty and invalid inputs");return 0;
}
