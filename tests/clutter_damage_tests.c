#include "rf/clutter_damage.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_clutter_gameplay_definition d={0};rf_clutter_damage_state s;
    rf_clutter_damage_result r,before;unsigned h,a,k,i;unsigned cases=0;
    const float healths[]={0,10,20},damages[]={0,5,10,20},factors[]={2,0,.5f,2};
    const int32_t types[]={-1,0,2,10};
    for(i=0;i<11;++i)d.damage_factors[i]=1;
    /* Exact48 ordinary410270 cases retained by geomod_clutter_damage.py. */
    for(h=0;h<3;++h)for(a=0;a<4;++a)for(k=0;k<4;++k){
        float expected=healths[h]-damages[a]*(types[k]<0?1:factors[k]);
        s=(rf_clutter_damage_state){healths[h],0x400000u,99};
        if(types[k]>=0)d.damage_factors[types[k]]=factors[k];
        CHECK(rf_clutter_damage_apply(&d,&s,damages[a],types[k],&r)==RF_OK);
        CHECK(r.state.health==expected && r.state.killing_type==(expected<=0?types[k]:99));
        CHECK(r.state.object_flags==s.object_flags && r.admitted && !r.retirement_marked);
        CHECK(r.health_break_due==(uint32_t)(expected<=0));++cases;
    }
    for(i=0;i<11;++i)d.damage_factors[i]=1;
    s=(rf_clutter_damage_state){80,0x400000,99};
    CHECK(!rf_clutter_damage_receive(&d,&s,79,3,0,&r));
    CHECK(r.state.health==1 && r.state.killing_type==99 && !r.health_break_due);
    CHECK(!rf_clutter_damage_receive(&d,&r.state,1,3,0,&r));
    CHECK(r.state.health==0 && r.state.killing_type==3 && r.health_break_due && !r.retirement_marked);
    s=(rf_clutter_damage_state){80,0x400004,99};
    CHECK(!rf_clutter_damage_receive(&d,&s,100,3,0,&r));
    CHECK(!r.admitted && r.state.health==80 && r.state.killing_type==99 && r.state.object_flags==0x600004);
    CHECK(!rf_clutter_damage_receive(&d,&s,100,3,1,&r));
    CHECK(r.admitted && r.state.health== -20 && r.health_break_due && !r.retirement_marked);
    CHECK(!rf_clutter_damage_receive(&d,&s,nextafterf(.001f,0),3,1,&r));
    CHECK(!r.admitted && r.state.object_flags==s.object_flags);
    CHECK(!rf_clutter_damage_receive(&d,&s,.001f,3,1,&r));CHECK(r.admitted);
    /* Marked living prop: no forged lethal event, kill type or break effects. */
    s=(rf_clutter_damage_state){80,0x400002,99};
    CHECK(!rf_clutter_damage_receive(&d,&s,0,3,0,&r));
    CHECK(r.retirement_marked && !r.health_break_due && r.state.health==80 && r.state.killing_type==99);
    s.health= -1;CHECK(!rf_clutter_damage_receive(&d,&s,0,3,0,&r));
    CHECK(r.retirement_marked && r.health_break_due && !r.admitted);
    s=(rf_clutter_damage_state){10,0,99};d.damage_factors[3]= -2;
    CHECK(!rf_clutter_damage_receive(&d,&s,5,3,0,&r));CHECK(r.state.health==20);
    d.damage_factors[3]=0;CHECK(!rf_clutter_damage_receive(&d,&s,100,3,0,&r));CHECK(r.state.health==10 && r.admitted);
    /* Failure must not publish even the outer damaged flag. */
    memset(&r,0xa5,sizeof(r));before=r;d.damage_factors[3]=FLT_MAX;
    CHECK(rf_clutter_damage_receive(&d,&s,FLT_MAX,3,0,&r)==RF_RANGE);CHECK(!memcmp(&r,&before,sizeof(r)));
    CHECK(rf_clutter_damage_apply(&d,&s,1,11,&r)==RF_RANGE);CHECK(!memcmp(&r,&before,sizeof(r)));
    CHECK(rf_clutter_damage_apply(&d,&s,1,-2,&r)==RF_RANGE);CHECK(!memcmp(&r,&before,sizeof(r)));
    CHECK(rf_clutter_damage_apply(&d,&s,NAN,3,&r)==RF_RANGE);CHECK(!memcmp(&r,&before,sizeof(r)));
    d.damage_factors[3]=NAN;
    CHECK(rf_clutter_damage_apply(&d,&s,1,3,&r)==RF_RANGE);CHECK(!memcmp(&r,&before,sizeof(r)));
    CHECK(!rf_clutter_damage_apply(&d,&s,1,-1,&r));CHECK(r.state.health==9);
    printf("%u retained arithmetic cases plus admission/retirement/rollback cases passed\n",cases);return 0;
}
