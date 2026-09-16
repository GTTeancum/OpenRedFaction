#include "rf/debris_audio.h"
#include "rf/timer.h"
#include <math.h>
#include <string.h>
static int valid_state(const rf_debris_audio_state *s)
{
    return s && s->count>=0 && isfinite(s->position_sum[0]) &&
        isfinite(s->position_sum[1]) && isfinite(s->position_sum[2]) &&
        isfinite(s->speed_sum) && s->speed_sum>=0 && s->deadline<=RF_TIMER_PERIOD;
}
void rf_debris_audio_init(rf_debris_audio_state *s)
{if(s){memset(s,0,sizeof(*s));s->deadline=-1;}}
int rf_debris_audio_contact(rf_debris_audio_state *s,const float p[3],
    const float v[3],float normal_y,uint32_t wet,uint32_t *contributed)
{
    rf_debris_audio_state next;double squared;float stored;uint32_t i;
    if(!valid_state(s) || !p || !v || !contributed || wet>1 || !isfinite(normal_y) ||
       normal_y < -1 || normal_y>1)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(p[i]) || !isfinite(v[i]))return RF_RANGE;
    if(normal_y<.7f || wet){*contributed=0;return RF_OK;}
    squared=((double)v[0]*v[0]+(double)v[1]*v[1])+(double)v[2]*v[2];
    if(squared<=4){*contributed=0;return RF_OK;}
    if(s->count==INT32_MAX)return RF_RANGE;
    stored=(float)squared;if(!isfinite(stored))return RF_RANGE;
    next=*s;++next.count;
    for(i=0;i<3;i++){next.position_sum[i]=(float)((double)next.position_sum[i]+p[i]);if(!isfinite(next.position_sum[i]))return RF_RANGE;}
    next.speed_sum=(float)((double)next.speed_sum+(stored>100?10:sqrt((double)stored)));
    if(!isfinite(next.speed_sum))return RF_RANGE;
    *s=next;*contributed=1;return RF_OK;
}
int rf_debris_audio_dispatch(rf_debris_audio_state *s,int32_t now,
    const int32_t *samples,uint32_t count,rf_random_state *random,rf_debris_audio_request *out)
{
    rf_debris_audio_state next;rf_random_state rng;rf_debris_audio_request r={0};
    int expired,status;uint32_t i,draw=0;float inverse,average,factor;
    if(!valid_state(s) || !random || !out || (count && !samples) || count>INT32_MAX)return RF_RANGE;
    status=rf_timer_expired(s->deadline,now,&expired);if(status)return status;
    next=*s;rng=*random;r.sample=-1;
    if(expired)next.deadline=-1;
    if(next.deadline>=0 || next.count<=5){*s=next;*out=r;return RF_OK;}
    inverse=(float)(1.0/next.count);
    for(i=0;i<3;i++){r.position[i]=(float)((double)next.position_sum[i]*inverse);if(!isfinite(r.position[i]))return RF_RANGE;}
    average=(float)((double)next.speed_sum*inverse);
    r.gain=(float)(((double)average*(double).1f+1.0)*.5);
    if(!isfinite(r.gain))return RF_RANGE;
    if(count){if(count>1)rf_random_next(&rng,&draw);r.sample=samples[draw%count];}
    factor=(float)((double)next.count*(double).025f);if(factor>1)factor=1;
    rf_random_next(&rng,&draw);
    r.delay_ms=(int32_t)(((double)draw/32768.0)*(1.0-(double)factor)*200.0);
    status=rf_timer_set(&next.deadline,now,r.delay_ms);if(status)return status;
    next.count=0;next.speed_sum=0;memset(next.position_sum,0,12);r.ready=1;
    *s=next;*random=rng;*out=r;return RF_OK;
}
