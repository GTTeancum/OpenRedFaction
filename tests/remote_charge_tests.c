#include "rf/remote_charge.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"remote line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int contact(void *ctx,const float p[3],const float d[3],float r,rf_weapon_flight_contact *h,uint32_t *yes)
{(void)p;(void)d;(void)r;*yes=ctx!=NULL;if(*yes){memset(h,0,sizeof(*h));h->object=UINT32_MAX;h->hit.fraction=.5f;h->hit.point[1]=-1;h->hit.normal[1]=1;}return RF_OK;}
int main(void)
{
    rf_remote_charge items[3]={0},before;rf_remote_charge_event event,prior;float p[3]={0},d[3]={0,0,1},g[3]={0,-9.8f,0};uint32_t any,i;
    for(i=0;i<3;i++)CHECK(!rf_remote_charge_launch(items+i,i==2?22:11,8,p,d,10,.051f,20));
    CHECK(!rf_remote_charge_step(items,.1f,g,contact,NULL,&event));CHECK(!event.attached && items[0].flight.position[2]==1 && items[0].flight.lifecycle.fuse==20);
    CHECK(!rf_remote_charge_step(items,.1f,g,contact,items,&event));CHECK(event.attached && fabsf(items[0].flight.position[1]+.95f)<.00001f);
    before=items[0];for(i=0;i<100;i++)CHECK(!rf_remote_charge_step(items,.25f,g,contact,items,&event));
    CHECK(!event.detonate && !memcmp(&before,&items[0],sizeof(before)));
    CHECK(!rf_remote_charge_available(items,3,11,8,&any) && any);
    CHECK(!rf_remote_charge_request(items,3,11,&any) && any && items[0].flight.lifecycle.fuse==-1 && items[1].flight.lifecycle.fuse==-1 && items[2].flight.lifecycle.fuse==20);
    CHECK(!rf_remote_charge_available(items,3,11,8,&any) && !any);
    for(i=0;i<2;i++){CHECK(!rf_remote_charge_step(items+i,.1f,g,contact,NULL,&event) && event.detonate);
        CHECK(!rf_remote_charge_step(items+i,.1f,g,contact,NULL,&event) && !event.detonate);}
    before=items[2];prior=event;CHECK(rf_remote_charge_step(items+2,NAN,g,contact,NULL,&event)==RF_RANGE);
    CHECK(!memcmp(&before,items+2,sizeof(before)) && !memcmp(&prior,&event,sizeof(event)));
    CHECK(!rf_remote_charge_request(items,3,99,&any) && !any);
    {
        rf_remote_charge charge={0},saved;rf_remote_charge_host host={0};uint32_t action;
        CHECK(!rf_remote_charge_launch(&charge,321,8,p,d,10,.051f,20));
        charge.attached=charge.flight.resting=1;charge.contact.object=456;
        charge.contact.hit.point[0]=12;charge.contact.hit.point[1]=23;charge.contact.hit.point[2]=34;charge.contact.hit.normal[2]=1;
        host.handle=456;host.found=1;host.position[0]=10;host.position[1]=20;host.position[2]=30;
        host.orientation[2]=-1;host.orientation[4]=1;host.orientation[6]=1;
        charge.flight.lifecycle.fuse=-1;
        CHECK(!rf_remote_charge_bind_host(&charge,&host));
        CHECK(charge.owner==321 && charge.host==456 && charge.local_offset[0]==-4 && charge.local_offset[1]==3 && charge.local_offset[2]==2);
        CHECK(charge.flight.lifecycle.fuse==20 && charge.orientation[8]==-1 && (charge.flight.lifecycle.instance_flags&0x40));
        charge.flight.lifecycle.fuse=-1;saved=charge;CHECK(!rf_remote_charge_bind_host(&charge,&host));CHECK(!memcmp(&charge,&saved,sizeof(saved)));
        charge.flight.lifecycle.fuse=20;
        memset(host.orientation,0,sizeof(host.orientation));host.orientation[0]=host.orientation[8]=-1;host.orientation[4]=1;
        host.position[0]=20;host.position[1]=30;host.position[2]=40;
        CHECK(!rf_remote_charge_update_host(&charge,&host,&action) && action==1);
        CHECK(charge.flight.position[0]==24 && charge.flight.position[1]==33 && charge.flight.position[2]==38);
        CHECK(charge.orientation[0]==0 && charge.orientation[2]==1 && charge.orientation[4]==1 && charge.orientation[6]==-1 && charge.orientation[8]==0);
        saved=charge;host.orientation[0]=NAN;action=99;
        CHECK(rf_remote_charge_update_host(&charge,&host,&action)==RF_RANGE && action==99 && !memcmp(&charge,&saved,sizeof(saved)));
        host.orientation[0]=-1;host.entity=1;host.entity_flags=1;
        CHECK(!rf_remote_charge_update_host(&charge,&host,&action) && action==2);
        CHECK(!charge.host_bound && !charge.attached && charge.attachment_initialized && (charge.flight.lifecycle.instance_flags&0x40) && charge.flight.lifecycle.active);
        charge=saved;host.entity=0;host.found=0;
        CHECK(!rf_remote_charge_update_host(&charge,&host,&action) && action==3 && (charge.object_flags&2) && !charge.flight.lifecycle.active);
        CHECK(!rf_remote_charge_step(&charge,.1f,g,contact,NULL,&event) && !event.detonate);
    }
    puts("remote gravity, persistent world stick, owned request, airborne/attached once detonation and rollback passed");return 0;
}
