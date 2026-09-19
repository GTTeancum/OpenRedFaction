#include "rf/clutter_checkpoint.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"clutter checkpoint line%d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    unsigned char identity[32]={1},other[32]={2},wire[136],saved[136];uint32_t bytes=777,count=999;
    rf_clutter_checkpoint_record rows[3]={{10,1,40,0x200000,-1,25},{20,1,-11,2|0x200000,3,25},{30,2,80,2,-1,-1}},out[3],before[3];
    CHECK(!rf_clutter_checkpoint_encode(identity,rows,3,wire,sizeof(wire),&bytes)&&bytes==136);
    CHECK(!rf_clutter_checkpoint_decode(wire,bytes,identity,out,3,&count)&&count==3&&!memcmp(rows,out,sizeof(rows)));
    memcpy(saved,wire,sizeof(wire));memset(out,0x5a,sizeof(out));memcpy(before,out,sizeof(out));count=999;
    CHECK(rf_clutter_checkpoint_decode(wire,bytes,other,out,3,&count)==RF_FORMAT&&!memcmp(out,before,sizeof(out))&&count==999);
    wire[128]^=1;CHECK(rf_clutter_checkpoint_decode(wire,bytes,identity,out,3,&count)==RF_FORMAT&&!memcmp(out,before,sizeof(out)));
    memcpy(wire,saved,sizeof(wire));CHECK(rf_clutter_checkpoint_decode(wire,bytes-1,identity,out,3,&count)==RF_FORMAT);
    CHECK(rf_clutter_checkpoint_decode(wire,bytes,identity,out,2,&count)==RF_RANGE&&count==999);
    rows[1].uid=10;CHECK(rf_clutter_checkpoint_encode(identity,rows,3,wire,sizeof(wire),&bytes)==RF_FORMAT&&!memcmp(wire,saved,sizeof(wire)));
    rows[1].uid=20;rows[1].cooldown_ms=26;CHECK(rf_clutter_checkpoint_encode(identity,rows,3,wire,sizeof(wire),&bytes)==RF_FORMAT);
    rows[1].cooldown_ms=25;rows[1].flags=0;CHECK(rf_clutter_checkpoint_encode(identity,rows,3,wire,sizeof(wire),&bytes)==RF_FORMAT);
    rows[1].flags=2;rows[1].health=NAN;CHECK(rf_clutter_checkpoint_encode(identity,rows,3,wire,sizeof(wire),&bytes)==RF_FORMAT);
    puts("PASS prop state roundtrip, identity/corruption, atomic errors, UID order, cooldown consistency and GeoMod retirement");return 0;
}
