#include "rf/grenade_throw.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_grenade_throw s={0},before;rf_grenade_throw_event e,saved;int32_t reserve=2;
    uint32_t i,alt;
    for(alt=0;alt<2;++alt){
        memset(&s,0,sizeof(s));reserve=2;
        CHECK(!rf_grenade_throw_step(&s,alt?2:1,1,reserve,&e));CHECK(e.kind==1 && e.alternate==alt);
        for(i=1;i<(alt?96u:102u);++i){CHECK(!rf_grenade_throw_step(&s,0,1,reserve,&e));CHECK(!e.kind);}
        CHECK(!rf_grenade_throw_step(&s,0,1,reserve,&e));CHECK(e.kind==2 && e.alternate==alt && reserve==2);
        CHECK(!rf_grenade_throw_step(&s,0,1,reserve,&e));CHECK(!e.kind); /* no duplicate release */
        CHECK(!rf_grenade_throw_resolve(&s,1,&reserve));CHECK(reserve==1 && s.cooldown==180);
        CHECK(rf_grenade_throw_resolve(&s,1,&reserve)==RF_RANGE && reserve==1);
        for(i=0;i<179;++i)CHECK(!rf_grenade_throw_step(&s,0,0,reserve,&e));
        CHECK(s.cooldown==1);
        CHECK(!rf_grenade_throw_step(&s,1,1,reserve,&e));CHECK(e.kind==1 && !s.cooldown);
    }
    /* Selection change cancels windup, holding on reselection is not a new edge. */
    CHECK(!rf_grenade_throw_step(&s,1,0,reserve,&e));CHECK(!s.phase);
    CHECK(!rf_grenade_throw_step(&s,1,1,reserve,&e));CHECK(!e.kind);
    CHECK(!rf_grenade_throw_step(&s,0,1,reserve,&e));
    CHECK(!rf_grenade_throw_step(&s,3,1,reserve,&e));CHECK(e.kind==1 && e.alternate);
    for(i=0;i<96;++i)CHECK(!rf_grenade_throw_step(&s,3,1,reserve,&e));
    CHECK(e.kind==2);CHECK(!rf_grenade_throw_resolve(&s,0,&reserve));CHECK(reserve==1 && !s.cooldown);
    CHECK(!rf_grenade_throw_step(&s,3,1,reserve,&e));CHECK(!e.kind);
    /* Last reserve removed during windup cancels release, never underflows. */
    memset(&s,0,sizeof(s));CHECK(!rf_grenade_throw_step(&s,1,1,1,&e));
    for(i=0;i<102;++i)CHECK(!rf_grenade_throw_step(&s,0,1,0,&e));CHECK(!e.kind && !s.phase);
    before=s;saved=e;CHECK(rf_grenade_throw_step(&s,4,1,1,&e)==RF_RANGE);
    CHECK(!memcmp(&before,&s,sizeof(s)) && !memcmp(&saved,&e,sizeof(e)));
    puts("grenade primary/alternate release, acknowledgement/ammo, cooldown/cancel passed");return 0;
}
