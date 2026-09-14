#include "rf/entity_assets.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_entity_movement_values value,saved;char path[1024];
    CHECK(argc==2);CHECK(snprintf(path,sizeof(path),"%s/tables.vpp",argv[1])<(int)sizeof(path));
    CHECK(rf_vpp_open(&tables,path)==0);
    CHECK(rf_entity_movement_load(&tables,"miner1",512*1024,&value)==0);
    CHECK(fabsf(value.radius-.5f)<.000001f && value.speed>0 && value.acceleration>0);
    saved=value;
    CHECK(rf_entity_movement_load(&tables,"__missing_class__",512*1024,&value)==RF_NOT_FOUND);
    CHECK(!memcmp(&saved,&value,sizeof(value)));
    rf_vpp_close(&tables);puts("authored movement radius and failed-load preservation PASS");return 0;
}
