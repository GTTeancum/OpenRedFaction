#include "rf/event.h"
#include "rf/level.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
static uint32_t actions,mutation;
static uint32_t auto_trace;
static void auto_callback(void *context,const rf_auto_trigger_state *s,uint32_t actor,uint32_t suppress)
{
    (void)context;
    auto_trace=s->flags^s->count^(uint32_t)s->deadline^(uint32_t)s->cooldown_ms^
        s->activation_time_bits^s->handle^actor^suppress;
    ++actions;
}
static void callback(void *context,rf_event_state *s,uint32_t action,uint32_t source,uint32_t actor,uint32_t mode)
{
    uint32_t i,values[4]={action,source,actor,mode};(void)context;
    if(!mutation) {actions=actions*4+action+1;return;}
    for(i=0;i<4;++i)actions=(actions^values[i])*16777619u;
    if(action!=2) {s->type=s->type==2?30:2;s->source=111;s->actor=222;s->mode=2;s->deadline=999;}
    else s->deadline=888;
}
int main(int argc,char **argv)
{
    struct {rf_event_state state;uint32_t tick,now,source,actor,mode;} in;
    struct {rf_event_state state;int32_t status;uint32_t actions;} out;
    if(argc==2 && !strcmp(argv[1],"--type-id")) {
        char name[256];int32_t type;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(name,sizeof(name),1,stdin)==1) {
            if(!memchr(name,0,sizeof(name)))return 3;
            type=rf_event_type_id(name);
            if(fwrite(&type,sizeof(type),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--auto-init")) {
        struct {float timing;uint32_t shape,flags[5],box,disabled,handle;int32_t now;} input;
        struct {int32_t status;rf_auto_trigger_state state;} result;
        rf_level_trigger record;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&record,0,sizeof(record));memset(&result,0xa5,sizeof(result));
            record.timing=input.timing;record.shape=input.shape;
            memcpy(record.flags,input.flags,sizeof(input.flags));
            record.box_flag=input.box;record.tail_flag=input.disabled;
            result.status=rf_auto_trigger_init(&result.state,&record,input.handle,input.now);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--auto-trigger")) {
        struct {rf_auto_trigger_state state;int32_t now;uint32_t clock_bits,eligible;} input;
        struct {rf_auto_trigger_state state;int32_t status;uint32_t calls,trace;} result;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            actions=auto_trace=0;result.state=input.state;
            result.status=rf_auto_trigger_fire(&result.state,input.now,input.clock_bits,
                (int)input.eligible,auto_callback,NULL);
            result.calls=actions;result.trace=auto_trace;
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--gravity-action")) {
        struct {float value;uint32_t action;} input;
        struct {int32_t status;rf_physics_gravity gravity;} result;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            rf_physics_gravity_set(&result.gravity,9.8f);
            result.status=rf_event_gravity_action(&result.gravity,input.value,input.action);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&in,sizeof(in),1,stdin)==1) {
        mutation=in.tick&2;actions=mutation?2166136261u:0;out.state=in.state;
        out.status=(in.tick&1)?rf_event_tick(&out.state,(int32_t)in.now,callback,NULL):
            rf_event_activate(&out.state,(int32_t)in.now,in.source,in.actor,in.mode,callback,NULL);
        out.actions=actions;if(fwrite(&out,sizeof(out),1,stdout)!=1)return 3;
    }
    return ferror(stdin)?3:0;
}
