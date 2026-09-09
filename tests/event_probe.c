#include "rf/event.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
static uint32_t actions;
static void callback(void *context,rf_event_state *s,uint32_t action,uint32_t source,uint32_t actor,uint32_t mode)
{(void)context;(void)s;(void)source;(void)actor;(void)mode;actions=actions*4+action+1;}
int main(void)
{
    struct {rf_event_state state;uint32_t tick,now,source,actor,mode;} in;
    struct {rf_event_state state;int32_t status;uint32_t actions;} out;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&in,sizeof(in),1,stdin)==1) {
        actions=0;out.state=in.state;
        out.status=in.tick?rf_event_tick(&out.state,(int32_t)in.now,callback,NULL):
            rf_event_activate(&out.state,(int32_t)in.now,in.source,in.actor,in.mode,callback,NULL);
        out.actions=actions;if(fwrite(&out,sizeof(out),1,stdout)!=1)return 3;
    }
    return ferror(stdin)?3:0;
}
