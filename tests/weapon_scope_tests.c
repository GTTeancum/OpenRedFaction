#include "rf/weapon_scope.h"
#include "rf/weapon.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define STEP(a,s,l) rf_weapon_scope_step(&state,a,s,l,90,25,&out)
int main(void)
{
    rf_weapon_scope state={0},before;rf_weapon_scope_result out,prior;
    CHECK(!STEP(0,1,1));CHECK(!out.active && out.horizontal_fov==90 && out.projection_scale==1);
    CHECK(!STEP(1,1,1));CHECK(out.active && out.changed && out.horizontal_fov==25);
    CHECK(fabsf(out.projection_scale-4.5107085f)<.00001f && fabsf(out.look_scale*out.projection_scale-1)<.00001f);
    CHECK(!STEP(1,1,1));CHECK(out.active && !out.changed);
    CHECK(!STEP(0,1,1));CHECK(out.active && !out.changed);
    CHECK(!STEP(1,1,1));CHECK(!out.active && out.changed);
    CHECK(!STEP(0,1,1));CHECK(!STEP(1,1,1));CHECK(out.active);
    CHECK(!STEP(1,0,1));CHECK(!out.active && out.changed);
    CHECK(!STEP(1,1,1));CHECK(!out.active);
    CHECK(!STEP(0,1,1));CHECK(!STEP(1,1,1));CHECK(out.active);
    CHECK(!STEP(1,1,0));CHECK(!out.active && out.changed);
    CHECK(!STEP(1,1,1));CHECK(!out.active);
    before=state;prior=out;
    CHECK(rf_weapon_scope_step(&state,0,1,1,90,NAN,&out)==RF_RANGE);
    CHECK(!memcmp(&state,&before,sizeof(state)) && !memcmp(&out,&prior,sizeof(out)));
    puts("scope press/hold toggle, switch/death reset and projection/sensitivity passed");return 0;
}
