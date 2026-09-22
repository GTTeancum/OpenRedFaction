#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene_collecting_clutter_lifecycle.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"collecting clutter line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_clutter_state state={0},saved,expected;int expired=0;
    state.handle=24;state.flags=0x4000;state.physics_flags=0x40;
    state.timer_a4=123;state.word_a8=8;state.timer_b0=777;state.health=75;
    saved=state;CHECK(!scene_collecting_clutter_eligible(&state));
    CHECK(scene_collecting_clutter_reserve(&state)==RF_NOT_FOUND&&!memcmp(&state,&saved,sizeof(state)));
    state.flags|=0x1000;expected=state;expected.flags|=0x2000;
    CHECK(scene_collecting_clutter_eligible(&state)&&!scene_collecting_clutter_reserve(&state));
    CHECK(!memcmp(&state,&expected,sizeof(state))&&!scene_collecting_clutter_eligible(&state));
    CHECK(scene_collecting_clutter_reserve(&state)==RF_NOT_FOUND&&!memcmp(&state,&expected,sizeof(state)));
    state.flags|=0x100; /* The real actor attachment path has completed. */
    expected=state;expected.physics_flags|=0x80000001u;expected.timer_a4=1500;expected.word_a8=10;
    CHECK(!scene_collecting_clutter_released(&state,1000)&&!memcmp(&state,&expected,sizeof(state)));
    CHECK(!scene_collecting_clutter_eligible(&state));
    state.handle=25;expected=state;expected.word_a8=11;expected.timer_a4=250;
    CHECK(!scene_collecting_clutter_released(&state,RF_TIMER_PERIOD-250));
    CHECK(!memcmp(&state,&expected,sizeof(state)));
    CHECK(!rf_timer_expired(state.timer_a4,249,&expired)&&!expired);
    CHECK(!rf_timer_expired(state.timer_a4,250,&expired)&&expired);
    saved=state;CHECK(scene_collecting_clutter_released(&state,-1)==RF_RANGE&&!memcmp(&state,&saved,sizeof(state)));
    CHECK(scene_collecting_clutter_released(&state,RF_TIMER_PERIOD+1)==RF_RANGE&&!memcmp(&state,&saved,sizeof(state)));
    state.handle=UINT32_MAX;saved=state;
    CHECK(scene_collecting_clutter_reserve(&state)==RF_RANGE&&!memcmp(&state,&saved,sizeof(state)));
    CHECK(scene_collecting_clutter_released(&state,0)==RF_RANGE&&!memcmp(&state,&saved,sizeof(state)));
    puts("PASS collecting reservation and detached release flags, sound, timer wrap and atomic rejection");return 0;
}
