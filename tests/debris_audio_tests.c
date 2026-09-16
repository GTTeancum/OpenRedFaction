#include "rf/debris_audio.h"
#include "rf/timer.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x);return 1;}}while(0)
/* Captured original48f900/48f4e0 outputs, not reconstructed expectations. */
static const struct {float ny,speed;unsigned wet;float y;unsigned accepted;float sum;} contacts[]={
{0.69999998807907104f,0.0f,0,-0.01f,0,0.0f},
{0.69999998807907104f,0.0f,0,0.01f,0,0.0f},
{0.69999998807907104f,0.0f,1,-0.01f,0,0.0f},
{0.69999998807907104f,0.0f,1,0.01f,0,0.0f},
{0.69999998807907104f,2.0f,0,-0.01f,0,0.0f},
{0.69999998807907104f,2.0f,0,0.01f,0,0.0f},
{0.69999998807907104f,2.0f,1,-0.01f,0,0.0f},
{0.69999998807907104f,2.0f,1,0.01f,0,0.0f},
{0.69999998807907104f,2.0000002384185791f,0,-0.01f,1,2.0000002384185791f},
{0.69999998807907104f,2.0000002384185791f,0,0.01f,1,2.0000002384185791f},
{0.69999998807907104f,2.0000002384185791f,1,-0.01f,0,0.0f},
{0.69999998807907104f,2.0000002384185791f,1,0.01f,1,2.0000002384185791f},
{0.69999998807907104f,3.0f,0,-0.01f,1,3.0f},
{0.69999998807907104f,3.0f,0,0.01f,1,3.0f},
{0.69999998807907104f,3.0f,1,-0.01f,0,0.0f},
{0.69999998807907104f,3.0f,1,0.01f,1,3.0f},
{0.69999998807907104f,10.0f,0,-0.01f,1,10.0f},
{0.69999998807907104f,10.0f,0,0.01f,1,10.0f},
{0.69999998807907104f,10.0f,1,-0.01f,0,0.0f},
{0.69999998807907104f,10.0f,1,0.01f,1,10.0f},
{0.69999998807907104f,12.0f,0,-0.01f,1,10.0f},
{0.69999998807907104f,12.0f,0,0.01f,1,10.0f},
{0.69999998807907104f,12.0f,1,-0.01f,0,0.0f},
{0.69999998807907104f,12.0f,1,0.01f,1,10.0f},
{0.70000004768371582f,0.0f,0,-0.01f,0,0.0f},
{0.70000004768371582f,0.0f,0,0.01f,0,0.0f},
{0.70000004768371582f,0.0f,1,-0.01f,0,0.0f},
{0.70000004768371582f,0.0f,1,0.01f,0,0.0f},
{0.70000004768371582f,2.0f,0,-0.01f,0,0.0f},
{0.70000004768371582f,2.0f,0,0.01f,0,0.0f},
{0.70000004768371582f,2.0f,1,-0.01f,0,0.0f},
{0.70000004768371582f,2.0f,1,0.01f,0,0.0f},
{0.70000004768371582f,2.0000002384185791f,0,-0.01f,1,2.0000002384185791f},
{0.70000004768371582f,2.0000002384185791f,0,0.01f,1,2.0000002384185791f},
{0.70000004768371582f,2.0000002384185791f,1,-0.01f,0,0.0f},
{0.70000004768371582f,2.0000002384185791f,1,0.01f,1,2.0000002384185791f},
{0.70000004768371582f,3.0f,0,-0.01f,1,3.0f},
{0.70000004768371582f,3.0f,0,0.01f,1,3.0f},
{0.70000004768371582f,3.0f,1,-0.01f,0,0.0f},
{0.70000004768371582f,3.0f,1,0.01f,1,3.0f},
{0.70000004768371582f,10.0f,0,-0.01f,1,10.0f},
{0.70000004768371582f,10.0f,0,0.01f,1,10.0f},
{0.70000004768371582f,10.0f,1,-0.01f,0,0.0f},
{0.70000004768371582f,10.0f,1,0.01f,1,10.0f},
{0.70000004768371582f,12.0f,0,-0.01f,1,10.0f},
{0.70000004768371582f,12.0f,0,0.01f,1,10.0f},
{0.70000004768371582f,12.0f,1,-0.01f,0,0.0f},
{0.70000004768371582f,12.0f,1,0.01f,1,10.0f},
{1.0f,0.0f,0,-0.01f,0,0.0f},
{1.0f,0.0f,0,0.01f,0,0.0f},
{1.0f,0.0f,1,-0.01f,0,0.0f},
{1.0f,0.0f,1,0.01f,0,0.0f},
{1.0f,2.0f,0,-0.01f,0,0.0f},
{1.0f,2.0f,0,0.01f,0,0.0f},
{1.0f,2.0f,1,-0.01f,0,0.0f},
{1.0f,2.0f,1,0.01f,0,0.0f},
{1.0f,2.0000002384185791f,0,-0.01f,1,2.0000002384185791f},
{1.0f,2.0000002384185791f,0,0.01f,1,2.0000002384185791f},
{1.0f,2.0000002384185791f,1,-0.01f,0,0.0f},
{1.0f,2.0000002384185791f,1,0.01f,1,2.0000002384185791f},
{1.0f,3.0f,0,-0.01f,1,3.0f},
{1.0f,3.0f,0,0.01f,1,3.0f},
{1.0f,3.0f,1,-0.01f,0,0.0f},
{1.0f,3.0f,1,0.01f,1,3.0f},
{1.0f,10.0f,0,-0.01f,1,10.0f},
{1.0f,10.0f,0,0.01f,1,10.0f},
{1.0f,10.0f,1,-0.01f,0,0.0f},
{1.0f,10.0f,1,0.01f,1,10.0f},
{1.0f,12.0f,0,-0.01f,1,10.0f},
{1.0f,12.0f,0,0.01f,1,10.0f},
{1.0f,12.0f,1,-0.01f,0,0.0f},
{1.0f,12.0f,1,0.01f,1,10.0f}};
static const struct {int count,deadline,pool,ready,sample;float gain;int next,count_out;unsigned seed;} dispatches[]={
{0,-1,1,0,-1,0.0f,-1,0,1u},
{0,-1,3,0,-1,0.0f,-1,0,1u},
{0,-1,6,0,-1,0.0f,-1,0,1u},
{0,900,1,0,-1,0.0f,-1,0,1u},
{0,900,3,0,-1,0.0f,-1,0,1u},
{0,900,6,0,-1,0.0f,-1,0,1u},
{0,1100,1,0,-1,0.0f,1100,0,1u},
{0,1100,3,0,-1,0.0f,1100,0,1u},
{0,1100,6,0,-1,0.0f,1100,0,1u},
{5,-1,1,0,-1,0.0f,-1,5,1u},
{5,-1,3,0,-1,0.0f,-1,5,1u},
{5,-1,6,0,-1,0.0f,-1,5,1u},
{5,900,1,0,-1,0.0f,-1,5,1u},
{5,900,3,0,-1,0.0f,-1,5,1u},
{5,900,6,0,-1,0.0f,-1,5,1u},
{5,1100,1,0,-1,0.0f,1100,5,1u},
{5,1100,3,0,-1,0.0f,1100,5,1u},
{5,1100,6,0,-1,0.0f,1100,5,1u},
{6,-1,1,1,101,0.80000001192092896f,1000,0,2745024u},
{6,-1,3,1,103,0.80000001192092896f,1095,0,3357800067u},
{6,-1,6,1,106,0.80000001192092896f,1095,0,3357800067u},
{6,900,1,1,101,0.80000001192092896f,1000,0,2745024u},
{6,900,3,1,103,0.80000001192092896f,1095,0,3357800067u},
{6,900,6,1,106,0.80000001192092896f,1095,0,3357800067u},
{6,1100,1,0,-1,0.0f,1100,6,1u},
{6,1100,3,0,-1,0.0f,1100,6,1u},
{6,1100,6,0,-1,0.0f,1100,6,1u},
{10,-1,1,1,101,0.80000001192092896f,1000,0,2745024u},
{10,-1,3,1,103,0.80000001192092896f,1084,0,3357800067u},
{10,-1,6,1,106,0.80000001192092896f,1084,0,3357800067u},
{10,900,1,1,101,0.80000001192092896f,1000,0,2745024u},
{10,900,3,1,103,0.80000001192092896f,1084,0,3357800067u},
{10,900,6,1,106,0.80000001192092896f,1084,0,3357800067u},
{10,1100,1,0,-1,0.0f,1100,10,1u},
{10,1100,3,0,-1,0.0f,1100,10,1u},
{10,1100,6,0,-1,0.0f,1100,10,1u},
{40,-1,1,1,101,0.80000001192092896f,1000,0,2745024u},
{40,-1,3,1,103,0.80000001192092896f,1000,0,3357800067u},
{40,-1,6,1,106,0.80000001192092896f,1000,0,3357800067u},
{40,900,1,1,101,0.80000001192092896f,1000,0,2745024u},
{40,900,3,1,103,0.80000001192092896f,1000,0,3357800067u},
{40,900,6,1,106,0.80000001192092896f,1000,0,3357800067u},
{40,1100,1,0,-1,0.0f,1100,40,1u},
{40,1100,3,0,-1,0.0f,1100,40,1u},
{40,1100,6,0,-1,0.0f,1100,40,1u},
{80,-1,1,1,101,0.80000001192092896f,1000,0,2745024u},
{80,-1,3,1,103,0.80000001192092896f,1000,0,3357800067u},
{80,-1,6,1,106,0.80000001192092896f,1000,0,3357800067u},
{80,900,1,1,101,0.80000001192092896f,1000,0,2745024u},
{80,900,3,1,103,0.80000001192092896f,1000,0,3357800067u},
{80,900,6,1,106,0.80000001192092896f,1000,0,3357800067u},
{80,1100,1,0,-1,0.0f,1100,80,1u},
{80,1100,3,0,-1,0.0f,1100,80,1u},
{80,1100,6,0,-1,0.0f,1100,80,1u}};
int main(void)
{
 rf_debris_audio_state state,before;rf_random_state rng;rf_debris_audio_request out,sentinel;
 const int32_t samples[]={101,102,103,104,105,106};unsigned i,accepted;
 for(i=0;i<sizeof(contacts)/sizeof(*contacts);i++){
  float p[]={7,8,9},v[]={contacts[i].speed,0,0};rf_debris_audio_init(&state);
  CHECK(!rf_debris_audio_contact(&state,p,v,contacts[i].ny,contacts[i].wet && contacts[i].y<=0,&accepted));
  CHECK(accepted==contacts[i].accepted && state.count==(int)accepted && state.speed_sum==contacts[i].sum);
  CHECK(state.position_sum[0]==(accepted?7:0) && state.position_sum[1]==(accepted?8:0) && state.position_sum[2]==(accepted?9:0));
 }
 for(i=0;i<sizeof(dispatches)/sizeof(*dispatches);i++){
  rf_debris_audio_init(&state);state.count=dispatches[i].count;state.deadline=dispatches[i].deadline;
  state.position_sum[0]=state.count*2.0f;state.position_sum[1]=state.count*3.0f;state.position_sum[2]=state.count*4.0f;state.speed_sum=state.count*6.0f;rng.value=1;
  CHECK(!rf_debris_audio_dispatch(&state,1000,samples,dispatches[i].pool,&rng,&out));
  CHECK(out.ready==(unsigned)dispatches[i].ready && out.sample==dispatches[i].sample && out.gain==dispatches[i].gain);
  CHECK(state.deadline==dispatches[i].next && state.count==dispatches[i].count_out && rng.value==dispatches[i].seed);
  if(out.ready)CHECK(out.position[0]==2 && out.position[1]==3 && out.position[2]==4 && state.speed_sum==0 && state.position_sum[0]==0);
 }
 /* Guards preserve state/RNG/output; count and timer carry when blocked. */
 rf_debris_audio_init(&state);state.count=6;state.speed_sum=36;before=state;rng.value=1;memset(&sentinel,0xa5,sizeof(sentinel));out=sentinel;
 CHECK(rf_debris_audio_dispatch(&state,-1,samples,6,&rng,&out)==RF_RANGE);
 CHECK(!memcmp(&state,&before,sizeof(state)) && rng.value==1 && !memcmp(&out,&sentinel,sizeof(out)));
 {float p[]={INFINITY,0,0},v[]={3,0,0};accepted=55;CHECK(rf_debris_audio_contact(&state,p,v,1,0,&accepted)==RF_RANGE);CHECK(accepted==55 && !memcmp(&state,&before,sizeof(state)));}
 /* Missing group still schedules/reset; only cooldown consumes RNG. */
 CHECK(!rf_debris_audio_dispatch(&state,1000,NULL,0,&rng,&out));CHECK(out.ready && out.sample==-1 && rng.value==2745024u && state.count==0);
 /* Timer wrap uses the shared original-period service. */
 state=before;rng.value=1;CHECK(!rf_debris_audio_dispatch(&state,RF_TIMER_PERIOD-1,samples,6,&rng,&out));
 {int32_t expected;CHECK(!rf_timer_set(&expected,RF_TIMER_PERIOD-1,out.delay_ms));CHECK(state.deadline==expected && state.deadline<200);}
 puts("PASS126 original debris audio vectors plus rollback/missing-group/wrap guards");return 0;
}
