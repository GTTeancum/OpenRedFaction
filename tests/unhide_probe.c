#include "rf/event.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
static uint32_t actions,allowed;
static int target(void *context,uint32_t handle,int unhide)
{
    (void)context;
    if(handle!=0x12340000)return 1;
    if(unhide && !allowed)return 0;
    actions=actions*4+(unhide?1:2);return 1;
}
int main(void)
{
    uint32_t in[9],out[5];rf_unhide_state s;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(in,sizeof(in),1,stdin)==1) {
        s.deadline=(int32_t)in[0];s.on=(uint8_t)in[1];s.off=(uint8_t)in[2];
        actions=0;allowed=in[4];
        if(in[5]>6)return 2;
        if(in[5]==4)out[3]=(uint32_t)rf_unhide_init(&s,(int32_t)in[3]);
        else if(in[5]>=5)out[3]=(uint32_t)rf_unhide_request(&s,in[5]==5);
        else out[3]=(uint32_t)rf_unhide_tick(&s,(int32_t)in[3],in+6,in[5],target,NULL);
        out[0]=(uint32_t)s.deadline;out[1]=s.on;out[2]=s.off;out[4]=actions;
        if(fwrite(out,sizeof(out),1,stdout)!=1)return 3;
    }
    return ferror(stdin)?3:0;
}
