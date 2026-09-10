#include "rf/event.h"
#include "rf/level.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
static uint32_t actions,mutation;
static uint32_t auto_trace;
static rf_event_links propagation_links;
static uint32_t propagation_handles[8],propagation_alternate[8],propagation_calls,propagation_hash;
static void propagation_callback(void *context,uint32_t handle,uint32_t source,
    uint32_t actor,uint32_t on,uint32_t suppress)
{
    uint32_t i,values[5]={handle,source,actor,on,suppress};(void)context;
    for(i=0;i<5;++i)propagation_hash=(propagation_hash^values[i])*16777619u;
    if(!propagation_calls++) {
        if(mutation==1)propagation_links.count=0;
        if(mutation==2)propagation_links.count=8;
        if(mutation==3)propagation_links.handles=propagation_alternate;
        if(mutation==4)propagation_handles[1]=0xffffffff;
    }
}
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
    if(argc==2 && !strcmp(argv[1],"--propagation")) {
        uint32_t input[5],output[4],i;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            if(input[0]>8)return 2;
            for(i=0;i<8;++i) {propagation_handles[i]=100+i;propagation_alternate[i]=200+i;}
            propagation_links.count=input[0];propagation_links.handles=propagation_handles;
            mutation=input[4];propagation_calls=0;propagation_hash=2166136261u;
            output[0]=(uint32_t)rf_event_links_propagate(&propagation_links,input[1],input[2],input[3],propagation_callback,NULL);
            output[1]=propagation_calls;output[2]=propagation_hash;output[3]=propagation_links.count;
            if(fwrite(output,sizeof(output),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==4 && (!strcmp(argv[1],"--owned-triggers") || !strcmp(argv[1],"--trigger-links") || !strcmp(argv[1],"--event-links") || !strcmp(argv[1],"--startup-events"))) {
        rf_vpp archive;rf_level level;rf_runtime_events events={0};rf_runtime_triggers triggers={0};
        rf_object_registry registry;uint32_t i,j,bytes,handle,event_count,n;
        rf_level_uid_object objects[RF_OBJECT_CAPACITY];int emit=!strcmp(argv[1],"--trigger-links"),emit_events=!strcmp(argv[1],"--event-links");
        if(rf_vpp_open(&archive,argv[2]))return 3;
        if(rf_level_open(&level,&archive,argv[3])) {rf_vpp_close(&archive);return 3;}
        rf_object_registry_init(&registry);
        if(rf_runtime_events_open(&level,&registry,1024*1024,&events))return 4;
        event_count=events.count;
        if(rf_runtime_triggers_open(&level,&registry,1024*1024,12345,&triggers))return 5;
        n=events.count+triggers.count;
        for(i=0;i<n;++i) {
            objects[i].uid=i<events.count?events.items[i].authored->record.uid:triggers.items[i-events.count].authored->record.uid;
            objects[i].handle=i<events.count?events.items[i].handle:triggers.items[i-events.count].handle;
            objects[i].flags=0;
        }
        if(rf_runtime_triggers_resolve(&triggers,objects,n,NULL,0))return 13;
        if(rf_runtime_events_resolve(&events,objects,n,NULL,0))return 13;
        for(i=0;i<events.count;++i)for(j=0;j<events.items[i].authored->record.link_count;++j) {
            rf_level_link_target *target=events.items[i].links+j;
            if(target->kind && !rf_object_registry_lookup(&registry,target->value))return 14;
            if(emit_events)printf("%u %u %u %u %u\n",events.items[i].authored->record.uid,
                events.items[i].authored->links[j],target->value,target->kind,target->index);
        }
        for(i=0;i<triggers.count;++i)for(j=0;j<triggers.items[i].authored->record.link_count;++j) {
            rf_level_link_target *target=triggers.items[i].links+j;
            if(target->kind && !rf_object_registry_lookup(&registry,target->value))return 14;
            if(emit)printf("%u %u %u %u %u\n",triggers.items[i].authored->record.uid,
                triggers.items[i].authored->links[j],target->value,target->kind,target->index);
        }
        bytes=triggers.allocated_bytes;
        for(i=0;i<triggers.count;++i) {
            rf_runtime_trigger *t=triggers.items+i;rf_auto_trigger_state initial;
            if(rf_auto_trigger_init(&initial,&t->authored->record,t->handle,12345) ||
               memcmp(&initial,&t->state,sizeof(initial)) || t->object_kind!=5 ||
               rf_object_registry_lookup(&registry,t->handle)!=t)return 6;
        }
        if(!emit && !emit_events)printf("%u %u\n",triggers.count,bytes);
        if(!strcmp(argv[1],"--startup-events")) {
            rf_physics_gravity gravity;rf_startup_events_report report;uint32_t words[13];
            rf_physics_gravity_set(&gravity,9.8f);
            if(rf_runtime_startup_events(&triggers,&gravity,12345,0x41400000,&report))return 15;
            memcpy(words,&report,sizeof(report));memcpy(words+9,&gravity,sizeof(gravity));
            printf("STARTUP");for(i=0;i<13;++i)printf(" %u",words[i]);puts("");
        }
        handle=triggers.count?triggers.items[0].handle:UINT32_MAX;
        rf_runtime_triggers_close(&triggers);rf_runtime_triggers_close(&triggers);
        if(registry.count!=RF_OBJECT_CAPACITY-event_count || rf_object_registry_lookup(&registry,handle))return 7;
        for(i=0;i<events.count;++i)if(rf_object_registry_lookup(&registry,events.items[i].handle)!=events.items+i)return 8;
        if(rf_runtime_triggers_open(&level,&registry,bytes-1,12345,&triggers)!=RF_RANGE ||
           registry.count!=RF_OBJECT_CAPACITY-event_count || triggers.items)return 9;
        if(rf_runtime_triggers_open(&level,&registry,bytes,12345,&triggers))return 10;
        rf_vpp_close(&archive);
        for(i=0;i<triggers.count;++i)if(triggers.items[i].authored->record.shape>1)return 11;
        rf_runtime_triggers_close(&triggers);rf_runtime_events_close(&events);
        return registry.count==RF_OBJECT_CAPACITY?0:12;
    }
    if(argc==4 && !strcmp(argv[1],"--owned")) {
        rf_vpp archive;rf_level level;rf_runtime_events events={0};
        rf_object_registry registry;uint32_t i,bytes,handle;int status;
        if(rf_vpp_open(&archive,argv[2]))return 3;
        if(rf_level_open(&level,&archive,argv[3])) {rf_vpp_close(&archive);return 3;}
        rf_object_registry_init(&registry);
        status=rf_runtime_events_open(&level,&registry,1024*1024,&events);if(status)return 4;
        bytes=events.allocated_bytes;
        for(i=0;i<events.count;++i) {
            rf_runtime_event *e=events.items+i;
            if(rf_object_registry_lookup(&registry,e->handle)!=e || e->object_kind!=6 ||
               e->state.type!=(uint32_t)rf_event_type_id(e->authored->record.type) ||
               e->state.deadline!=-1 || e->state.flags)return 5;
        }
        printf("%u %u\n",events.count,bytes);
        handle=events.count?events.items[0].handle:UINT32_MAX;
        rf_runtime_events_close(&events);rf_runtime_events_close(&events);
        if(registry.count!=RF_OBJECT_CAPACITY || rf_object_registry_lookup(&registry,handle))return 6;
        if(rf_runtime_events_open(&level,&registry,bytes-1,&events)!=RF_RANGE ||
           registry.count!=RF_OBJECT_CAPACITY || events.items)return 7;
        if(rf_runtime_events_open(&level,&registry,bytes,&events))return 8;
        rf_vpp_close(&archive);
        /* Both raw and unresolved runtime links survive source archive close. */
        for(i=0;i<events.count;++i) {
            uint32_t j;rf_runtime_event *e=events.items+i;
            if(rf_event_type_id(e->authored->record.type)<0)return 9;
            for(j=0;j<e->authored->record.link_count;++j)
                if(e->links[j].value!=e->authored->links[j] || e->links[j].kind || e->links[j].index!=UINT32_MAX)return 9;
        }
        rf_runtime_events_close(&events);return registry.count==RF_OBJECT_CAPACITY?0:10;
    }
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
