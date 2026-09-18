#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"precision shield line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    scene_precision_contact hits[RF_WEAPON_PRECISION_HITS]={0},body={0},shield={0},tied={0};uint32_t count=0;
    body.actor=(rf_hitscan_selection){1,3,30,.6f};
    shield.actor=(rf_hitscan_selection){1,2,20,.2f};shield.shield_hit=1;
    shield.shield.slot=2;shield.shield.handle=20;shield.shield.limit=.8f;shield.shield.hit.time=.2f;
    scene_precision_insert(hits,&count,1,&body);scene_precision_insert(hits,&count,1,&shield);
    CHECK(count==1 && hits[0].shield_hit && hits[0].actor.handle==20);
    CHECK(hits[0].shield.limit==.8f && hits[0].shield.hit.time==.2f);
    count=0;scene_precision_insert(hits,&count,RF_WEAPON_PRECISION_HITS,&body);
    scene_precision_insert(hits,&count,RF_WEAPON_PRECISION_HITS,&shield);
    tied=shield;tied.actor.handle=21;tied.shield.handle=21;
    scene_precision_insert(hits,&count,RF_WEAPON_PRECISION_HITS,&tied);
    CHECK(count==3 && hits[0].actor.handle==20 && hits[1].actor.handle==21 && hits[2].actor.handle==30);
    CHECK(!hits[2].shield_hit && hits[1].shield.limit==.8f);
    puts("Precision nearest shield token, stable ties and rail continuation ordering");return 0;
}
