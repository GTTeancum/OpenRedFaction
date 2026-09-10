#include "rf/event.h"
#include "rf/level.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
static uint32_t actions,mutation;
static uint32_t auto_trace;
static uint32_t explode_count,explode_hash;
static void explode_callback(void *context,const rf_event_explode_request *request)
{
    uint32_t words[11],i;(void)context;memcpy(words,request,sizeof(words));
    for(i=0;i<11;++i)explode_hash=(explode_hash^words[i])*16777619u;
    ++explode_count;
}
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
static uint32_t activation_trace,activation_mutation;
static void activation_callback(void *context,rf_trigger_activation *trigger,uint32_t actor,uint32_t suppress)
{
    uint32_t words[8],i;(void)context;memcpy(words,trigger,sizeof(words));
    activation_trace=2166136261u;
    for(i=0;i<8;i++)activation_trace=(activation_trace^words[i])*16777619u;
    activation_trace=(activation_trace^actor)*16777619u;activation_trace=(activation_trace^suppress)*16777619u;
    if(activation_mutation) {
        trigger->state.flags^=8;trigger->state.count=UINT32_MAX;
        trigger->limit=0;trigger->object_flags|=0x80;
    }
}
int main(int argc,char **argv)
{
    struct {rf_event_state state;uint32_t tick,now,source,actor,mode;} in;
    struct {rf_event_state state;int32_t status;uint32_t actions;} out;
    if(argc==2 && !strcmp(argv[1],"--trigger-actor")) {
        int32_t input[26];rf_entity_view nodes[4];rf_entity_registry registry;rf_trigger_actor_facts facts;uint32_t output[10],i;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            memset(nodes,0,sizeof(nodes));memset(&registry,0,sizeof(registry));
            for(i=0;i<4;i++) {
                nodes[i].handle=input[i*5];nodes[i].type=input[i*5+1];nodes[i].class_type=input[i*5+2];
                nodes[i].flags_7c=(uint32_t)input[i*5+3];nodes[i].linked_handle=input[i*5+4];
                registry.slots[i]=nodes+i;
            }
            memset(&facts,0xa5,sizeof(facts));output[0]=(uint32_t)rf_trigger_actor_resolve(&registry,nodes,input[20],input[21],input+22,4,&facts);
            memcpy(output+1,&facts,sizeof(facts));fwrite(output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--runtime-trigger-fire")) {
        static rf_object_registry registry;rf_runtime_triggers owner={0};rf_runtime_trigger trigger={0};
        rf_runtime_event events[2]={{0}};rf_level_owned_trigger authored={0};rf_level_owned_event records[2]={{0}};
        rf_level_link_target targets[2]={{0}},self={0};rf_startup_events_report report;
        rf_physics_gravity gravity={0};uint32_t fired,i;
        rf_object_registry_init(&registry);owner.registry=&registry;owner.items=&trigger;owner.count=1;
        trigger.object_kind=5;trigger.authored=&authored;trigger.links=targets;authored.record.link_count=2;
        trigger.activation.limit=1;trigger.state.deadline=100;trigger.state.cooldown_ms=50;
        if(rf_object_registry_insert(&registry,&trigger,&trigger.handle))return 70;
        trigger.state.handle=trigger.handle;
        for(i=0;i<2;i++) {
            events[i].object_kind=6;events[i].authored=records+i;events[i].state.type=i?44:3;events[i].state.deadline=-1;
            if(rf_object_registry_insert(&registry,events+i,&events[i].handle))return 71;
            targets[i].kind=1;targets[i].value=events[i].handle;
        }
        self.kind=1;self.value=trigger.handle;events[0].links=&self;records[0].record.link_count=1;
        records[1].record.values[0]=12.5f;
        if(rf_runtime_trigger_fire(&owner,trigger.handle,123,100,0x42c80000,1,0,&gravity,NULL,&report,&fired) ||
            fired || trigger.state.count || report.triggers)return 72;
        if(rf_runtime_trigger_fire(&owner,trigger.handle,123,100,0x42c80000,0,0,&gravity,NULL,&report,&fired) ||
            !fired || trigger.state.count!=1 || trigger.state.flags!=80 || trigger.activation.object_flags!=2 ||
            trigger.state.deadline!=150 || trigger.state.activation_time_bits!=0x42c80000 ||
            gravity.acceleration!=12.5f || report.triggers!=1 || report.events!=2 || report.gravity_actions!=1 ||
            events[1].state.actor!=123 || events[1].state.source!=trigger.handle)return 73;
        if(rf_runtime_trigger_fire(&owner,events[0].handle,123,100,0,0,0,&gravity,NULL,&report,&fired)!=RF_NOT_FOUND)return 74;
        puts("PASS runtime trigger dispatch, self-disable, gravity and limit mark");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--trigger-fire")) {
        struct {rf_trigger_activation trigger;uint32_t now,clock,blocked,actor,suppress,mutation;} input;
        struct {rf_trigger_activation trigger;uint32_t status,fired,trace;} output;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            activation_mutation=input.mutation;activation_trace=0;output.fired=0xa5a5a5a5;
            output.status=(uint32_t)rf_trigger_fire_sp(&input.trigger,(int32_t)input.now,input.clock,
                input.blocked,input.actor,input.suppress,activation_callback,NULL,&output.fired);
            output.trigger=input.trigger;output.trace=activation_trace;fwrite(&output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--trigger-contact-delay")) {
        struct {rf_trigger_contact_timer timer;int32_t now;uint32_t accepted;} input;
        struct {int32_t status,deadline;uint32_t ready;} output;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            output.ready=0xa5a5a5a5;
            output.status=rf_trigger_contact_delay(&input.timer,input.now,input.accepted,&output.ready);
            output.deadline=input.timer.deadline;fwrite(&output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--trigger-box")) {
        struct {float center[3],matrix[3][3],size[3];uint32_t flags;float current[3],start[3],end[3];} input;
        uint32_t output[2];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            output[1]=0xa5a5a5a5;
            output[0]=(uint32_t)rf_trigger_box_contact(input.center,input.matrix,input.size,input.flags,
                input.current,input.start,input.end,output+1);
            fwrite(output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--trigger-sphere")) {
        float input[7];uint32_t output[2];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            output[1]=0xa5a5a5a5;
            output[0]=(uint32_t)rf_trigger_sphere_contact(input,input[3],input+4,output+1);
            fwrite(output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--trigger-eligible")) {
        uint32_t input[26],output[2];rf_trigger_gate gate;rf_trigger_actor_facts actor;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            memcpy(&gate,input,28);gate.allowed_handles=input+18;memcpy(&actor,input+7,sizeof(actor));
            if(gate.allowed_count>8)return 2;output[1]=0xa5a5a5a5;
            output[0]=(uint32_t)rf_trigger_eligible(&gate,&actor,(int32_t)input[16],input[17],output+1);
            fwrite(output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-events")) {
        static rf_level_particle_state state;uint32_t mode,pending,i;
        for(mode=0;mode<4;mode++) {
            rf_runtime_event item={0};rf_level_owned_event authored={0};rf_runtime_events events={0};
            rf_runtime_triggers triggers={0};rf_object_registry registry;rf_physics_gravity gravity={0};
            rf_startup_events_report report;rf_level_particles particles={0};
            rf_level_particle_binding bindings[2]={{91,0},{92,0}};uint32_t ids[3]={92,999,92};
            rf_level_link_target unresolved[3]={{0}};
            memset(&state,0,sizeof(state));particles.state=&state;particles.materials.bindings=bindings;particles.materials.count=2;
            for(i=0;i<2;i++){state.slots[i].active=1;state.slots[i].source_id=bindings[i].uid;
                state.slots[i].runtime.enabled=0xaabbcc02;state.slots[i].runtime.emitter.deadline=77;}
            rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=&item;events.count=1;
            item.object_kind=6;item.authored=&authored;item.links=unresolved;item.state.type=39;
            authored.links=ids;authored.record.link_count=3;item.state.deadline=100;item.state.mode=mode;
            if(rf_runtime_events_tick(&events,&triggers,&gravity,99,&particles,&report,&pending) || report.events || pending)return 40;
            if(rf_runtime_events_tick(&events,&triggers,&gravity,100,NULL,&report,&pending) || pending!=1 || item.state.deadline!=100)return 41;
            if(rf_runtime_events_tick(&events,&triggers,&gravity,100,&particles,&report,&pending) || pending || report.events!=1 ||
                report.unsupported_actions || report.unresolved_targets!=3 || item.state.deadline!=-1)return 42;
            if(state.slots[0].runtime.enabled!=0xaabbcc02 || state.slots[0].runtime.emitter.deadline!=77 ||
                state.slots[1].runtime.enabled!=(mode?0xaabbcc01u:0xaabbcc00u) ||
                state.slots[1].runtime.emitter.deadline!=(mode?100:77))return 43;
            item.state.deadline=101;
            if(rf_runtime_events_tick(&events,&triggers,&gravity,101,&particles,&report,&pending) ||
                state.slots[1].runtime.emitter.deadline!=(mode?100:77))return 44;
        }
        {
            rf_object_registry registry;rf_runtime_event item={0};rf_level_owned_event authored={0};
            rf_runtime_trigger trigger={0};rf_level_owned_trigger raw={0};rf_runtime_triggers triggers={0};
            rf_level_link_target root={0},unresolved={0};uint32_t uid=91;rf_level_particle_binding binding={91,0};
            rf_level_particles particles={0};rf_physics_gravity gravity={0};rf_startup_events_report report;
            memset(&state,0,sizeof(state));state.slots[0].active=1;state.slots[0].source_id=91;
            state.slots[0].runtime.emitter.deadline=77;particles.state=&state;particles.materials.bindings=&binding;particles.materials.count=1;
            rf_object_registry_init(&registry);item.object_kind=6;item.authored=&authored;item.state.type=39;item.state.deadline=-1;
            authored.links=&uid;authored.record.link_count=1;item.links=&unresolved;
            if(rf_object_registry_insert(&registry,&item,&item.handle))return 45;
            root.kind=1;root.value=item.handle;raw.record.link_count=1;trigger.authored=&raw;trigger.links=&root;trigger.state.flags=8;
            triggers.registry=&registry;triggers.items=&trigger;triggers.count=1;
            if(rf_runtime_startup_events(&triggers,&gravity,200,0,&particles,&report) || report.events!=1 || report.unsupported_actions ||
                state.slots[0].runtime.enabled!=1 || state.slots[0].runtime.emitter.deadline!=200)return 46;
        }
        puts("PASS 4 scheduled Particle_State modes and immediate startup activation");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--explode")) {
        struct {rf_event_explode_state state;uint32_t action;} input;uint32_t output[3];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            explode_count=0;explode_hash=2166136261u;
            output[0]=(uint32_t)rf_event_explode_action(&input.state,input.action,explode_callback,NULL);
            output[1]=explode_count;output[2]=explode_hash;
            if(fwrite(output,sizeof(output),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--event-ticks")) {
        uint32_t mode,i,pending;
        for(mode=0;mode<4;++mode) {
            rf_runtime_event items[3]={0};rf_level_owned_event authored[3]={0};
            rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
            rf_level_link_target link={0};rf_physics_gravity gravity,expected;rf_startup_events_report report;
            rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=3;
            for(i=0;i<3;++i) {
                items[i].object_kind=6;items[i].authored=authored+i;items[i].state.type=i==2?50:44;
                items[i].state.deadline=i==1?-1:100;authored[i].record.values[0]=(float)(4-i);
                if(rf_object_registry_insert(&registry,items+i,&items[i].handle))return 30;
            }
            authored[0].record.link_count=1;items[0].links=&link;link.kind=1;link.value=items[1].handle;
            items[0].state.source=7;items[0].state.actor=8;items[0].state.mode=mode;items[0].state.flags=1;
            if(mode==3) {items[0].state.type=48;items[0].state.mode=1;}
            items[1].state.delay=.001f;
            rf_physics_gravity_set(&gravity,9.8f);
            if(rf_runtime_events_tick(&events,&triggers,&gravity,99,NULL,&report,&pending) || report.events || pending!=1)return 31;
            if(rf_runtime_events_tick(&events,&triggers,&gravity,100,NULL,&report,&pending) || report.events!=2 ||
               report.gravity_actions!=((mode==1 || mode==2)?1u:0u) || items[0].state.deadline!=-1 || items[1].state.deadline!=101)return 32;
            if(items[1].state.source!=((mode==1 || mode==3)?7u:8u) || items[1].state.actor!=8)return 33;
            if(rf_runtime_events_tick(&events,&triggers,&gravity,101,NULL,&report,&pending) || report.events!=1 ||
               report.gravity_actions!=((mode==1 || mode==3)?1u:0u) || items[1].state.deadline!=-1 || items[2].state.deadline!=100)return 34;
            rf_physics_gravity_set(&expected,(mode==1 || mode==3)?3.0f:mode==2?4.0f:9.8f);
            if(memcmp(&gravity,&expected,sizeof(gravity)))return 35;
            if(rf_runtime_events_tick(&events,&triggers,&gravity,102,NULL,&report,&pending) || report.events || pending!=1)return 36;
        }
        puts("PASS 4 delayed event fixtures");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--startup-recursion")) {
        uint32_t scenario,i;
        for(scenario=0;scenario<5;++scenario) {
            rf_runtime_event events[3]={0};rf_level_owned_event authored[3]={0};
            rf_runtime_trigger triggers[2]={0};rf_level_owned_trigger raw[2]={0};
            rf_level_link_target links[3]={0},roots[2]={0};rf_object_registry registry;
            rf_runtime_triggers owner={0};rf_startup_events_report report;rf_physics_gravity gravity;int status;
            rf_object_registry_init(&registry);owner.registry=&registry;owner.items=triggers;owner.count=2;
            for(i=0;i<3;++i) {
                events[i].object_kind=6;events[i].state.type=44;events[i].state.deadline=-1;
                events[i].authored=authored+i;authored[i].record.values[0]=(float)(4-i);
                if(rf_object_registry_insert(&registry,events+i,&events[i].handle))return 20;
            }
            for(i=0;i<2;++i) {
                triggers[i].object_kind=5;triggers[i].authored=raw+i;raw[i].record.link_count=1;
                triggers[i].links=roots+i;roots[i].kind=1;roots[i].value=events[0].handle;
                if(rf_object_registry_insert(&registry,triggers+i,&triggers[i].handle))return 21;
                triggers[i].state.handle=triggers[i].handle;triggers[i].state.flags=i?24:8;
            }
            authored[0].record.link_count=2;events[0].links=links;
            links[0].kind=links[1].kind=links[2].kind=1;
            links[0].value=events[1].handle;links[1].value=events[2].handle;
            if(scenario==1 || scenario==2) {authored[1].record.link_count=1;events[1].links=links+2;
                links[2].value=scenario==1?triggers[1].handle:events[0].handle;}
            if(scenario>=3)events[0].state.type=3;
            if(scenario==4) {events[1].state.type=3;authored[1].record.link_count=1;
                events[1].links=links+2;links[2].value=events[2].handle;}
            rf_physics_gravity_set(&gravity,9.8f);
            status=rf_runtime_startup_events(&owner,&gravity,12345,0x41400000,NULL,&report);
            if(scenario==2) {if(status!=RF_RANGE || report.events!=64)return 22;continue;}
            if(scenario>=3) {
                rf_physics_gravity expected;
                rf_physics_gravity_set(&expected,scenario==3?9.8f:2.0f);
                if(status || report.events!=(scenario==3?3u:4u) || report.gravity_actions!=(scenario==3?0u:1u) ||
                   report.unsupported_actions || memcmp(&gravity,&expected,sizeof(gravity)))return 26;
                if(events[1].state.source!=UINT32_MAX || events[2].state.source!=UINT32_MAX)return 27;
                continue;
            }
            if(status || report.events!=(scenario?6u:3u) || report.gravity_actions!=report.events ||
               report.triggers!=(scenario?2u:1u) || report.unsupported_actions || report.unresolved_targets)return 23;
            if(memcmp(&gravity,&(rf_physics_gravity){2.0f,{0,-2.0f,0}},sizeof(gravity)))return 24;
            for(i=0;i<3;++i)if(events[i].state.actor!=UINT32_MAX ||
                events[i].state.source!=triggers[scenario?1:0].handle)return 25;
        }
        puts("PASS 5 startup recursion fixtures");return 0;
    }
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
            if(rf_runtime_startup_events(&triggers,&gravity,12345,0x41400000,NULL,&report))return 15;
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
