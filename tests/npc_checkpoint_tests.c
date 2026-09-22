#include "rf/npc_checkpoint.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"NPC checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void reseal(unsigned char *p,uint32_t n)
{uint32_t i,h=2166136261u;for(i=0;i<n;i++){h^=(i>=12&&i<16)?0:p[i];h*=16777619u;}for(i=0;i<4;i++)p[12+i]=(unsigned char)(h>>(i*8));}
int main(void)
{
    rf_npc_checkpoint_catalog c={0};rf_npc_checkpoint_record rows[2]={0},out[2],saved[2];
    unsigned char identity[32]={9},wrong[32]={8},blob[64+2*528],original[sizeof(blob)],sentinel[sizeof(blob)];
    uint32_t bytes=0,count=99,written=123;int status;
    c.hash=0x12345678;c.count=2;c.supported[0]=c.supported[1]=1;
    c.weapons[0]=(rf_weapon_acquire_definition){0,100,12};c.weapons[1]=(rf_weapon_acquire_definition){1,5,0};
    rows[0].uid=7;rows[0].class_id=3;rows[0].health=80;rows[0].armor=15;rows[0].flags=0x4004;
    rows[0].position[0]=4;rows[0].yaw=.5f;rows[0].primary=0;rows[0].secondary=-1;rows[0].ai_mode=2;
    rows[0].inventory.owned[0]=1;rows[0].inventory.loaded[0]=4;rows[0].inventory.reserve[0]=120;
    rows[1]=rows[0];rows[1].uid=10;rows[1].retired=1;rows[1].health=-3;
    rows[1].drop=(rf_campaign_weapon_drop){1,0,0,{4,2,1}};
    CHECK(!rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&bytes)&&bytes==sizeof(blob));
    CHECK(!rf_npc_checkpoint_preflight(blob,bytes,identity,&c,&count)&&count==2);
    CHECK(!rf_npc_checkpoint_decode(blob,bytes,identity,&c,out,2,&count)&&!memcmp(rows,out,sizeof(rows)));
    CHECK(out[0].inventory.reserve[0]==120&&out[1].drop.quantity==0&&out[1].drop.state==1);
    memcpy(original,blob,bytes);memset(out,0x5a,sizeof(out));memcpy(saved,out,sizeof(out));count=99;
    CHECK(rf_npc_checkpoint_decode(blob,bytes,wrong,&c,out,2,&count)==RF_FORMAT&&count==99&&!memcmp(saved,out,sizeof(out)));
    c.hash++;CHECK(rf_npc_checkpoint_preflight(blob,bytes,identity,&c,&count)==RF_FORMAT&&count==99);c.hash--;
    blob[90]^=1;CHECK(rf_npc_checkpoint_decode(blob,bytes,identity,&c,out,2,&count)==RF_FORMAT&&!memcmp(saved,out,sizeof(out)));
    memcpy(blob,original,bytes);blob[64+528+524]=4;blob[64+528+525]=blob[64+528+526]=blob[64+528+527]=0;reseal(blob,bytes);
    CHECK(rf_npc_checkpoint_decode(blob,bytes,identity,&c,out,2,&count)==RF_FORMAT&&count==99&&!memcmp(saved,out,sizeof(out)));
    memcpy(blob,original,bytes);CHECK(rf_npc_checkpoint_decode(blob,bytes,identity,&c,out,1,&count)==RF_RANGE&&!memcmp(saved,out,sizeof(out)));
    memset(blob,0xa5,sizeof(blob));memcpy(sentinel,blob,sizeof(blob));rows[1].uid=rows[0].uid;
    CHECK(rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written)==RF_FORMAT&&written==123&&!memcmp(blob,sentinel,sizeof(blob)));
    rows[1].uid=10;rows[0].health=NAN;
    CHECK(rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written)==RF_FORMAT&&!memcmp(blob,sentinel,sizeof(blob)));
    rows[0].health=80;rows[0].inventory.loaded[0]=13;
    CHECK(rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written)==RF_FORMAT);
    rows[0].inventory.loaded[0]=4;rows[0].inventory.owned[0]=0;rows[0].primary=-1;
    CHECK(!rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written)); /* None retains dormant ammo. */
    rows[0].primary=0;CHECK(rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written)==RF_FORMAT);
    rows[0].primary=-1;rows[1].drop.state=2;
    CHECK(!rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob),&written));
    CHECK(rf_npc_checkpoint_encode(identity,&c,rows,2,blob,sizeof(blob)-1,&written)==RF_RANGE);
    status=rf_npc_checkpoint_encode(identity,&c,NULL,0,blob,sizeof(blob),&written);
    CHECK(!status&&written==64&&!rf_npc_checkpoint_decode(blob,written,identity,&c,NULL,0,&count)&&!count);
    puts("PASS NPC checkpoint pose/vitals/inventory/drop roundtrip, identity, malformed state and output preservation");return 0;
}
