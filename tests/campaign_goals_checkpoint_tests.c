#include "rf/campaign_goals_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"goals checkpoint line%d\n",__LINE__);return 1;}}while(0)
static rf_campaign_goals goals,out,before;
static rf_campaign_local_goals locals,local_out,local_before;
static unsigned char wire[RF_CAMPAIGN_GOALS_CHECKPOINT_MAX_BYTES],saved[sizeof(wire)];
static const unsigned char identity[32]={1,2,3},other[32]={4};
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void checksum(uint32_t bytes)
{uint32_t i,h=2166136261u;for(i=0;i<bytes;i++){h^=i>=12&&i<16?0:wire[i];h*=16777619u;}put(wire+12,h);}
static int rejected(uint32_t bytes)
{
    int status=rf_campaign_goals_checkpoint_decode(wire,bytes,identity,&out,&local_out);
    return status==RF_FORMAT&&!memcmp(&out,&before,sizeof(out))&&
        !memcmp(&local_out,&local_before,sizeof(local_out));
}
int main(void)
{
    uint32_t bytes=777,passed,written,i,local_offset;
    CHECK(!rf_campaign_goal_declare(&goals,"Mission",1));
    CHECK(!rf_campaign_goal_declare(&goals,"Room",0));
    CHECK(!rf_campaign_goal_adjust(&goals,"Mission",1));
    CHECK(!rf_campaign_goal_adjust(&goals,"Room",1));
    CHECK(!rf_campaign_local_goals_save(&locals,"L1S1.rfl",&goals));
    CHECK(!rf_campaign_goals_next_section(&goals));
    CHECK(!rf_campaign_goal_declare(&goals,"Room",0));
    CHECK(!rf_campaign_goal_adjust(&goals,"Room",0));
    CHECK(!rf_campaign_local_goals_save(&locals,"L1S2.rfl",&goals));
    CHECK(locals.count==2&&goals.count==2&&goals.items[1].value==-1);
    CHECK(!rf_campaign_goals_checkpoint_encode(identity,&goals,&locals,wire,sizeof(wire),&bytes));
    CHECK(bytes==64+2*264+2*324&&!memcmp(wire,"RFGC\1\0\0\0",8));
    CHECK(wire[64+264+256]==255&&wire[64+264+259]==255);
    CHECK(!rf_campaign_goals_checkpoint_preflight(wire,bytes,identity));
    CHECK(!rf_campaign_goals_checkpoint_decode(wire,bytes,identity,&out,&local_out));
    CHECK(!memcmp(&out,&goals,sizeof(goals))&&!memcmp(&local_out,&locals,sizeof(locals)));
    CHECK(!rf_campaign_local_goals_restore(&local_out,"l1s1.RFL",&out));
    CHECK(!rf_campaign_goal_check(&out,"ROOM",1,&passed)&&passed);
    CHECK(!rf_campaign_goal_check(&out,"mission",1,&passed)&&passed);
    CHECK(!rf_campaign_local_goals_restore(&local_out,"L1S2.rfl",&out));
    CHECK(out.items[1].value==-1&&out.items[0].persistent==1);
    memcpy(saved,wire,sizeof(wire));memset(&out,0xa5,sizeof(out));memset(&local_out,0x5a,sizeof(local_out));
    before=out;local_before=local_out;
    CHECK(rf_campaign_goals_checkpoint_decode(wire,bytes,other,&out,&local_out)==RF_FORMAT);
    CHECK(!memcmp(&out,&before,sizeof(out))&&!memcmp(&local_out,&local_before,sizeof(local_out)));
    wire[bytes-1]^=1;CHECK(rejected(bytes));memcpy(wire,saved,sizeof(wire));
    CHECK(rejected(bytes-1));
    /* Valid checksum cannot hide malformed names, flags, counts or duplicate keys. */
    memset(wire+64,'x',256);checksum(bytes);CHECK(rejected(bytes));memcpy(wire,saved,sizeof(wire));
    wire[64]=0;checksum(bytes);CHECK(rejected(bytes));memcpy(wire,saved,sizeof(wire));
    put(wire+64+260,2);checksum(bytes);CHECK(rejected(bytes));memcpy(wire,saved,sizeof(wire));
    memcpy(wire+64+264,wire+64,256);wire[64+264]='m';checksum(bytes);
    CHECK(rejected(bytes));memcpy(wire,saved,sizeof(wire));
    local_offset=64+2*264;
    memcpy(wire+local_offset+324,wire+local_offset,320);wire[local_offset+324]='L';
    checksum(bytes);CHECK(rejected(bytes));memcpy(wire,saved,sizeof(wire));
    memset(wire+local_offset,'x',64);checksum(bytes);CHECK(rejected(bytes));memcpy(wire,saved,sizeof(wire));
    put(wire+16,65);checksum(bytes);CHECK(rejected(bytes));memcpy(wire,saved,sizeof(wire));
    put(wire+20,65);checksum(bytes);CHECK(rejected(bytes));memcpy(wire,saved,sizeof(wire));
    wire[56]=1;checksum(bytes);CHECK(rejected(bytes));memcpy(wire,saved,sizeof(wire));
    written=999;goals.items[1]=goals.items[0];goals.items[1].name[0]='m';
    CHECK(rf_campaign_goals_checkpoint_encode(identity,&goals,&locals,wire,sizeof(wire),&written)==RF_FORMAT);
    CHECK(written==999&&!memcmp(wire,saved,sizeof(wire)));
    strcpy(goals.items[1].name,"Room");locals.items[1]=locals.items[0];locals.items[1].name[0]='r';
    CHECK(rf_campaign_goals_checkpoint_encode(identity,&goals,&locals,wire,sizeof(wire),&written)==RF_FORMAT);
    CHECK(written==999&&!memcmp(wire,saved,sizeof(wire)));
    /* Signed extremes and all capacity slots; fixed owners avoid large stacks. */
    memset(&goals,0,sizeof(goals));memset(&locals,0,sizeof(locals));
    for(i=0;i<64;i++){
        char name[32];snprintf(name,sizeof(name),"g%u",i);
        CHECK(!rf_campaign_goal_declare(&goals,name,0));goals.items[i].value=i&1?INT32_MIN:INT32_MAX;
    }
    CHECK(!rf_campaign_local_goals_save(&locals,"L1S1.rfl",&goals));
    CHECK(!rf_campaign_goals_checkpoint_encode(identity,&goals,&locals,wire,sizeof(wire),&bytes));
    CHECK(bytes==sizeof(wire));
    CHECK(!rf_campaign_goals_checkpoint_decode(wire,bytes,identity,&out,&local_out));
    CHECK(!memcmp(&out,&goals,sizeof(goals))&&!memcmp(&local_out,&locals,sizeof(locals)));
    memset(wire,0x33,sizeof(wire));memcpy(saved,wire,sizeof(wire));written=999;
    CHECK(rf_campaign_goals_checkpoint_encode(identity,&goals,&locals,wire,sizeof(wire)-1,&written)==RF_RANGE);
    CHECK(written==999&&!memcmp(wire,saved,sizeof(wire)));
    goals.count=locals.count=0;
    CHECK(!rf_campaign_goals_checkpoint_encode(identity,&goals,&locals,wire,sizeof(wire),&bytes)&&bytes==64);
    CHECK(!rf_campaign_goals_checkpoint_decode(wire,bytes,identity,&out,&local_out)&&!out.count&&!local_out.count);
    puts("PASS mission goal codec, real section restore, atomic malformed rejection and signed counters");return 0;
}
