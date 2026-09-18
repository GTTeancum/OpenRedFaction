#include "rf/weapon_scanner.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"scanner line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_object_registry registry;int owners[34];rf_weapon_scanner_candidate c[34]={0};
    rf_weapon_scanner_result out,before;rf_model_projection view={0};uint32_t i;
    rf_object_registry_init(&registry);view.perspective=1;view.rotation[0]=view.rotation[4]=view.rotation[8]=1;
    view.screen[0]=320;view.screen[1]=-320;view.screen[2]=320;view.screen[3]=240;
    for(i=0;i<34;i++){CHECK(!rf_object_registry_insert(&registry,owners+i,&c[i].handle));
        c[i].identity=owners+i;c[i].eligible=1;c[i].health=100;c[i].center[2]=(float)(34-i);}
    CHECK(!rf_weapon_scanner_select(&registry,c,34,&view,100,640,480,&out));
    CHECK(out.count==32 && out.truncated && out.markers[0].index==33 && out.markers[31].index==2);
    CHECK(out.markers[0].screen[0]==320 && out.markers[0].screen[1]==240 && out.markers[0].depth==1);
    c[0].center[0]=1;c[0].center[1]=1;c[0].center[2]=10;
    CHECK(!rf_weapon_scanner_select(&registry,c,1,&view,100,640,480,&out));
    CHECK(out.count==1 && out.markers[0].screen[0]==352 && out.markers[0].screen[1]==208 && out.markers[0].depth==10);
    c[0].center[2]=-1;CHECK(!rf_weapon_scanner_select(&registry,c,1,&view,100,640,480,&out) && !out.count);
    c[0].center[2]=10;c[0].center[0]=100;CHECK(!rf_weapon_scanner_select(&registry,c,1,&view,200,640,480,&out) && !out.count);
    c[0].center[0]=0;CHECK(!rf_weapon_scanner_select(&registry,c,1,&view,5,640,480,&out) && !out.count);
    c[0].health=0;c[0].center[0]=NAN;CHECK(!rf_weapon_scanner_select(&registry,c,1,&view,100,640,480,&out) && !out.count);
    c[0].health=100;before=out;CHECK(rf_weapon_scanner_select(&registry,c,1,&view,100,640,480,&out)==RF_RANGE && !memcmp(&before,&out,sizeof(out)));
    CHECK(!rf_object_registry_remove(&registry,c[0].handle));CHECK(!rf_weapon_scanner_select(&registry,c,1,&view,100,640,480,&out) && !out.count);
    /* Camera translation plus yaw90 uses the same retained projection helper. */
    memset(view.rotation,0,sizeof(view.rotation));view.rotation[2]=-1;view.rotation[4]=1;view.rotation[6]=1;view.camera[0]=5;
    c[1].center[0]=15;c[1].center[1]=0;c[1].center[2]=0;
    CHECK(!rf_weapon_scanner_select(&registry,c+1,1,&view,100,640,480,&out));CHECK(out.count==1 && out.markers[0].depth==10 && out.markers[0].screen[0]==320);
    puts("scanner projection, near/order/cap, view transform, range, stale/dead filters and rollback passed");return 0;
}
