#include "rf/remote_checkpoint.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"remote checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static rf_remote_checkpoint source,decoded,before;
static unsigned char bytes[RF_REMOTE_CHECKPOINT_MAX],saved[RF_REMOTE_CHECKPOINT_MAX];
static int clear(void *c,const float p[3],const float d[3],float r,rf_weapon_flight_contact *h,uint32_t *yes)
{(void)c;(void)p;(void)d;(void)r;(void)h;*yes=0;return RF_OK;}
int main(void)
{
    float p[3]={1,2,3},d[3]={0,0,1},g[3]={0,-9.8f,0};uint32_t written=99,again,i;
    rf_remote_charge_event a,b;rf_remote_charge_host host={0};
    CHECK(!rf_remote_charge_launch(source.charges+2,7,0,p,d,10,.051f,20));source.owner_keys[2]=42;
    CHECK(!rf_remote_charge_launch(source.charges+31,7,0,p,d,10,.051f,20));source.owner_keys[31]=42;
    source.charges[31].attached=source.charges[31].flight.resting=1;source.charges[31].contact.object=77;
    memcpy(source.charges[31].contact.hit.point,p,12);source.charges[31].contact.hit.normal[2]=1;
    host.found=1;host.handle=77;host.orientation[0]=host.orientation[4]=host.orientation[8]=1;
    CHECK(!rf_remote_charge_bind_host(source.charges+31,&host));source.host_keys[31]=123;source.tags[31]=0x20000003;
    source.held=1;source.pending=1;source.delay=12;source.selected_mode=2;
    CHECK(!rf_remote_checkpoint_encode(&source,10,20,bytes,sizeof(bytes),&written));CHECK(written==48+2*228);
    CHECK(!rf_remote_checkpoint_decode(bytes,written,10,20,&decoded));CHECK(!memcmp(&source,&decoded,sizeof(source)));
    CHECK(!rf_remote_checkpoint_encode(&decoded,10,20,saved,sizeof(saved),&again));CHECK(again==written && !memcmp(bytes,saved,written));
    CHECK(!rf_remote_charge_step(source.charges+2,.1f,g,clear,NULL,&a));CHECK(!rf_remote_charge_step(decoded.charges+2,.1f,g,clear,NULL,&b));
    CHECK(!memcmp(source.charges+2,decoded.charges+2,sizeof(rf_remote_charge)));
    before=decoded;CHECK(rf_remote_checkpoint_decode(bytes,written,11,20,&decoded)==RF_FORMAT && !memcmp(&before,&decoded,sizeof(before)));
    bytes[48+228]=2;CHECK(rf_remote_checkpoint_decode(bytes,written,10,20,&decoded)==RF_FORMAT && !memcmp(&before,&decoded,sizeof(before)));memcpy(bytes,saved,written);
    source.charges[2].flight.position[0]=NAN;memset(bytes,0xa5,sizeof(bytes));memcpy(saved,bytes,sizeof(bytes));again=77;
    CHECK(rf_remote_checkpoint_encode(&source,10,20,bytes,sizeof(bytes),&again)==RF_FORMAT && again==77 && !memcmp(bytes,saved,sizeof(bytes)));
    memset(&source,0,sizeof(source));for(i=0;i<32;i++){CHECK(!rf_remote_charge_launch(source.charges+i,7,0,p,d,10,.051f,20));source.owner_keys[i]=42;}
    CHECK(!rf_remote_checkpoint_encode(&source,10,20,bytes,sizeof(bytes),&written) && written==RF_REMOTE_CHECKPOINT_MAX);
    CHECK(!rf_remote_checkpoint_decode(bytes,written,10,20,&decoded) && !memcmp(&source,&decoded,sizeof(source)));
    puts("remote checkpoint active slots, attachment/scheduler roundtrip, flight continuation, capacity and atomic rejection passed");return 0;
}
