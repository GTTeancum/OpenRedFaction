#include "rf/clutter_damage.h"
#include <assert.h>
#include <string.h>
#include "../src/diagnostic/scene_riot_shield.inc"
int main(void)
{
    rf_clutter_gameplay_definition d={0};
    rf_clutter_damage_state s={100,0x40,-1};scene_riot_shield_result r;
    float f[3]={0,0,1},front[3]={0,0,-1},rear[3]={0,0,1},side[3]={1,0,0};
    d.damage_factors[0]=.5f;
    assert(scene_riot_shield_intercept(&d,&s,1,1,1,1,f,front,20,0,&r)==RF_OK);
    assert(r.intercepted && !r.broken && r.state.health==90);
    assert(r.state.object_flags==(0x40|0x200000));assert(s.health==100);
    assert(scene_riot_shield_intercept(&d,&s,1,1,1,1,f,rear,20,0,&r)==RF_OK && !r.intercepted);
    assert(scene_riot_shield_intercept(&d,&s,1,1,1,1,f,side,20,0,&r)==RF_OK && !r.intercepted);
    assert(scene_riot_shield_intercept(&d,&s,1,1,1,0,f,front,20,0,&r)==RF_OK && !r.intercepted);
    assert(scene_riot_shield_intercept(&d,&s,1,1,0,1,f,front,20,0,&r)==RF_OK && !r.intercepted);
    assert(scene_riot_shield_intercept(&d,&s,0,1,1,1,f,front,20,0,&r)==RF_OK && !r.intercepted);
    assert(scene_riot_shield_intercept(&d,&s,1,0,1,1,f,front,20,0,&r)==RF_OK && !r.intercepted);
    assert(scene_riot_shield_intercept(&d,&s,1,1,1,1,f,front,120,-1,&r)==RF_OK);
    assert(r.intercepted && r.broken && r.state.health==-20 && r.state.killing_type==-1);
    s=r.state;
    assert(scene_riot_shield_intercept(&d,&s,1,1,1,1,f,front,20,0,&r)==RF_OK && !r.intercepted);
    assert(r.state.health==-20);
    return 0;
}
