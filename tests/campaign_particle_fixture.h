/* Shared non-reentrant, process-local campaign replay fixture. */
#ifndef RF_CAMPAIGN_PARTICLE_FIXTURE_H
#define RF_CAMPAIGN_PARTICLE_FIXTURE_H
#include "rf/event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
char rf_campaign_particle_text[8192];
uint32_t rf_campaign_particle_text_size;
static void campaign_trace(const char *format,...)
{
    va_list args;int n;uint32_t left=sizeof(rf_campaign_particle_text)-rf_campaign_particle_text_size;
    if(!left)return;
    va_start(args,format);n=vsnprintf(rf_campaign_particle_text+rf_campaign_particle_text_size,left,format,args);va_end(args);
    if(n<0 || (uint32_t)n>=left){rf_campaign_particle_text_size=sizeof(rf_campaign_particle_text);return;}
    rf_campaign_particle_text_size+=(uint32_t)n;
}
static int campaign_particle_events(const rf_level *level,rf_level_particles *particles)
{
    rf_runtime_events events={0};static rf_object_registry registry;rf_runtime_triggers triggers={0};
    rf_runtime_trigger trigger={0};rf_level_owned_trigger raw={0};rf_level_link_target target={0};
    rf_startup_events_report report;rf_physics_gravity gravity={0};rf_level_particle_state *initial;
    uint32_t i,j,n=0,pending,fired;int status;
    rf_object_registry_init(&registry);triggers.registry=&registry;triggers.items=&trigger;triggers.count=1;
    trigger.object_kind=5;trigger.authored=&raw;trigger.links=&target;raw.record.link_count=1;target.kind=1;
    status=rf_runtime_events_open(level,&registry,512*1024,&events);if(status)return status;
    initial=malloc(sizeof(*initial));if(!initial){rf_runtime_events_close(&events);return RF_IO;}
    status=rf_object_registry_insert(&registry,&trigger,&trigger.handle);
    if(status){free(initial);rf_runtime_events_close(&events);return status;}
    memcpy(initial,particles->state,sizeof(*initial));
    for(i=0;i<events.count;i++)if(events.items[i].state.type==39)++n;
    campaign_trace("EVENTS %u %u\n",n,particles->materials.count);
    for(i=0;i<events.count;i++)if(events.items[i].state.type==39) {
        rf_runtime_event *event=events.items+i;int32_t deadline,fire_time=100;
        memcpy(particles->state,initial,sizeof(*initial));
        /* Controlled replay precondition: all emitters disabled before the
         * authored on-event. Existing particles and timers remain intact. */
        for(j=0;j<particles->materials.count;j++)particles->state->slots[j].runtime.enabled&=~255u;
        trigger.activation=(rf_trigger_activation){0};trigger.state.handle=trigger.handle;
        trigger.state.cooldown_ms=50;trigger.activation.limit=1;target.value=event->handle;
        status=rf_runtime_trigger_fire(&triggers,trigger.handle,123,100,0x42c80000,1,0,&gravity,particles,&report,&fired);
        if(status)goto done;
        if(fired || trigger.state.count || report.triggers){status=RF_FORMAT;goto done;}
        status=rf_runtime_trigger_fire(&triggers,trigger.handle,123,100,0x42c80000,0,0,&gravity,particles,&report,&fired);
        if(status)goto done;
        if(!fired || trigger.state.count!=1 || trigger.state.flags!=64 || trigger.activation.object_flags!=2 ||
            trigger.state.deadline!=150 || trigger.state.activation_time_bits!=0x42c80000 ||
            event->state.actor!=123 || event->state.source!=trigger.handle){status=RF_FORMAT;goto done;}
        campaign_trace("TRIGGER %u %u %u %d %u\n",trigger.state.count,trigger.state.flags,
            trigger.activation.object_flags,trigger.state.deadline,event->state.actor);
        deadline=event->state.deadline;
        if(deadline>=0) {
            for(j=0;j<particles->materials.count;j++)if(particles->state->slots[j].runtime.enabled&255u){status=RF_FORMAT;goto done;}
            status=rf_runtime_events_tick(&events,&triggers,&gravity,deadline-1,particles,&report,&pending);if(status)goto done;
            for(j=0;j<particles->materials.count;j++)if(particles->state->slots[j].runtime.enabled&255u){status=RF_FORMAT;goto done;}
            fire_time=deadline;status=rf_runtime_events_tick(&events,&triggers,&gravity,deadline,particles,&report,&pending);if(status)goto done;
        }
        if(event->state.deadline!=-1){status=RF_FORMAT;goto done;}
        campaign_trace("EVENT %u %d %d\n",event->authored->record.uid,deadline,fire_time);
        for(j=0;j<particles->materials.count;j++) {
            rf_particle_emitter_runtime *runtime=&particles->state->slots[j].runtime;
            campaign_trace("EMITTER %u %u %d %d\n",particles->materials.bindings[j].uid,runtime->enabled&255u,
                runtime->emitter.deadline,initial->slots[j].runtime.emitter.deadline);
        }
    }
    status=RF_OK;
done:
    memcpy(particles->state,initial,sizeof(*initial));free(initial);rf_object_registry_remove(&registry,trigger.handle);rf_runtime_events_close(&events);return status;
}
#endif
