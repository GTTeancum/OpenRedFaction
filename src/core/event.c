#include "rf/event.h"
#include "rf/collision.h"
#include "rf/level.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
int rf_trigger_sphere_contact(const float center[3],float radius,
    const float actor_center[3],uint32_t *contact)
{
    float delta[3];double distance;uint32_t i;
    if(!center || !actor_center || !contact)return RF_RANGE;
    if(!isfinite(radius))return RF_FORMAT;
    for(i=0;i<3;i++) {
        if(!isfinite(center[i]) || !isfinite(actor_center[i]))return RF_FORMAT;
        delta[i]=(float)((double)center[i]-actor_center[i]);
        if(!isfinite(delta[i]))return RF_FORMAT;
    }
    distance=((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+
        (double)delta[2]*delta[2];
    *contact=distance<=(double)radius*radius;return RF_OK;
}
/* 506dd0 specialized to its three-vertex use in 5065b0. */
static uint32_t trigger_triangle(const float point[3],const float vertices[3][3],
    const float normal[3])
{
    static const uint32_t axes[3][2]={{2,1},{0,2},{1,0}};
    uint32_t axis,x,y;float px,py,bx,by,cx,cy,stored;double v,t;
    if(fabsf(normal[0])>fabsf(normal[1]))axis=fabsf(normal[2])<fabsf(normal[0])?0:2;
    else axis=fabsf(normal[2])<fabsf(normal[1])?1:2;
    x=axes[axis][normal[axis]>0?0:1];y=axes[axis][normal[axis]>0?1:0];
    px=point[x]-vertices[0][x];py=point[y]-vertices[0][y];
    bx=vertices[1][x]-vertices[0][x];by=vertices[1][y]-vertices[0][y];
    cx=vertices[2][x]-vertices[0][x];cy=vertices[2][y]-vertices[0][y];
    if(bx<=-.0001f || bx>=.0001f)v=((double)bx*py-(double)by*px)/((double)cy*bx-(double)cx*by);
    else v=(double)px/cx;
    stored=(float)v;
    if(!(v>=0 && stored<=1))return 0;
    if(bx<=-.0001f || bx>=.0001f)t=((double)px-(double)stored*cx)/bx;
    else t=((double)py-(double)stored*cy)/by;
    return t>=0 && (double)stored+t<=1;
}
int rf_trigger_box_contact(const float center[3],const float matrix[3][3],
    const float size[3],uint32_t flags,const float actor_center[3],
    const float actor_start[3],const float actor_end[3],uint32_t *contact)
{
    float delta[3],corners[4][3],triangle[3][3],point[3]={0},plane[4],fraction;
    float half[3],right,up,forward;double length,dot;uint32_t i,j,hit;int status;
    if(!center || !matrix || !size || !actor_center || !actor_start || !actor_end || !contact)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(center[i]) || !isfinite(size[i]) || size[i]<0 ||
            !isfinite(actor_center[i]) || !isfinite(actor_start[i]) || !isfinite(actor_end[i]))return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(matrix[i][j]))return RF_FORMAT;
    }
    if(!(flags&32))return rf_collision_segment_oriented_box(center,matrix,size,actor_start,actor_end,point,contact);
    for(i=0;i<3;i++) {
        delta[i]=actor_end[i]-actor_center[i];half[i]=size[i]*.5f;
        if(!isfinite(delta[i]))return RF_FORMAT;
    }
    length=sqrt(((double)delta[2]*delta[2]+(double)delta[1]*delta[1])+(double)delta[0]*delta[0]);
    dot=((double)delta[2]*matrix[2][2]+(double)delta[1]*matrix[2][1])+(double)delta[0]*matrix[2][0];
    if(length<=.00001f || dot>=0) {*contact=0;return RF_OK;}
    for(i=0;i<3;i++) {
        right=matrix[0][i]*half[0];up=matrix[1][i]*half[1];forward=matrix[2][i]*half[2];
        for(j=0;j<4;j++) {
            volatile float a=(j&1)?center[i]-right:center[i]+right;
            volatile float b=(j&2)?a+up:a-up;
            corners[j][i]=b+forward;
            if(!isfinite(corners[j][i]))return RF_FORMAT;
        }
        plane[i]=matrix[2][i];
    }
    plane[3]=(float)-(((double)plane[2]*corners[0][2]+(double)plane[1]*corners[0][1])+(double)plane[0]*corners[0][0]);
    status=rf_collision_segment_plane(actor_start,delta,plane,&fraction,&hit);if(status)return status;
    if(!hit) {*contact=0;return RF_OK;}
    for(i=0;i<3;i++) {volatile float offset=delta[i]*fraction;point[i]=actor_start[i]+offset;}
    memcpy(triangle,corners,sizeof(triangle));hit=trigger_triangle(point,triangle,plane);
    if(!hit) {memcpy(triangle[2],corners[3],12);hit=trigger_triangle(point,triangle,plane);}
    *contact=hit;return RF_OK;
}
int rf_trigger_contact_delay(rf_trigger_contact_timer *timer,int32_t now,
    uint32_t accepted,uint32_t *ready)
{
    double milliseconds;int expired,status;int32_t deadline;
    if(!timer || !ready || accepted>1 || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!isfinite(timer->seconds))return RF_FORMAT;
    deadline=timer->deadline;
    if(timer->seconds>0) {
        if(!accepted)rf_timer_clear(&deadline);
        else if(deadline<0) {
            milliseconds=(double)timer->seconds*1000.0+0.5;
            if(milliseconds>RF_TIMER_PERIOD)return RF_RANGE;
            status=rf_timer_set(&deadline,now,(int32_t)milliseconds);if(status)return status;
            accepted=0;
        } else {
            status=rf_timer_expired(deadline,now,&expired);if(status)return status;
            if(expired)rf_timer_clear(&deadline);
            else accepted=0;
        }
    }
    timer->deadline=deadline;*ready=accepted;return RF_OK;
}
static uint32_t trigger_player_selected(const rf_entity_registry *registry,
    const rf_entity_view *actor,const int32_t *players,uint32_t count)
{
    uint32_t i;const rf_entity_view *entity;
    if(!actor)return 0;if(actor->flags_7c&8)return 1;
    for(i=0;i<count;i++) {
        entity=rf_entity_lookup(registry,players[i]);
        if(entity && entity->linked_handle==actor->handle)return 1;
    }
    return 0;
}
int rf_trigger_actor_resolve(const rf_entity_registry *registry,const rf_entity_view *actor,
    int32_t owner_handle,int32_t attached_handle,const int32_t *players,uint32_t player_count,
    rf_trigger_actor_facts *facts)
{
    rf_trigger_actor_facts value={0};const rf_entity_view *entity,*linked,*attached;
    if(!registry || !actor || !facts || player_count>INT32_MAX || (player_count && !players))return RF_RANGE;
    value.handle=(uint32_t)actor->handle;value.kind=(uint32_t)actor->type;
    value.test_4895d0=(actor->flags_7c&8)!=0;
    value.test_48aaf0=trigger_player_selected(registry,actor,players,player_count);
    entity=rf_entity_lookup(registry,actor->handle);value.entity_present=entity!=NULL;
    if(entity) {
        value.test_429990=entity->class_type==1;
        linked=rf_entity_lookup(registry,entity->linked_handle);
        value.test_4290d0=linked && linked->class_type==1;
    }
    value.owner_test_48aaf0=trigger_player_selected(registry,rf_object_lookup(registry,owner_handle),players,player_count);
    attached=rf_object_lookup(registry,attached_handle);value.attached_present=attached && attached->type==4;
    *facts=value;return RF_OK;
}
int rf_trigger_eligible(const rf_trigger_gate *g,const rf_trigger_actor_facts *a,
    int32_t now,uint32_t input,uint32_t *eligible)
{
    uint32_t i;int expired,status;
    if(!g || !a || !eligible || g->allowed_count>INT32_MAX ||
        (g->allowed_count && !g->allowed_handles))return RF_RANGE;
    status=rf_timer_expired(g->deadline,now,&expired);if(status)return status;
    *eligible=0;
    if((g->flags&0x58u) || (g->limit!=-1 && g->activations>=g->limit) || !expired)return RF_OK;
    if(g->filter==0 && !(a->test_4895d0&255u) && !(g->flags&2))return RF_OK;
    if(g->filter==3 && (a->test_48aaf0&255u)==1)return RF_OK;
    if(g->filter==4 && (!a->entity_present || !(a->test_429990&255u) || !(a->test_48aaf0&255u)))return RF_OK;
    if(g->filter==2) {
        for(i=0;i<g->allowed_count;i++)if(g->allowed_handles[i]==a->handle)break;
        if(i==g->allowed_count)return RF_OK;
    }
    if((g->flags&1) && !(input&255u))return RF_OK;
    if((g->flags&2) && (a->kind!=2 || !(a->owner_test_48aaf0&255u)))return RF_OK;
    if((g->flags&128) && (!a->entity_present || !(a->test_4290d0&255u)))return RF_OK;
    if(g->attached!=-1 && !a->attached_present)return RF_OK;
    *eligible=1;return RF_OK;
}
int rf_trigger_volume_init(const rf_level_trigger *record,rf_trigger_volume *volume)
{
    rf_trigger_volume value={0};uint32_t i;
    if(!record || !volume)return RF_RANGE;
    if(record->shape>1)return RF_FORMAT;
    value.shape=record->shape;
    for(i=0;i<3;i++) {
        if(!isfinite(record->position[i]))return RF_FORMAT;
        value.center[i]=record->position[i];
    }
    if(!record->shape) {
        if(!isfinite(record->radius))return RF_FORMAT;
        value.radius=record->radius;
    } else {
        for(i=0;i<9;i++)if(!isfinite(record->orientation_disk[i]))return RF_FORMAT;
        for(i=0;i<3;i++)if(!isfinite(record->dimensions_disk[i]) || record->dimensions_disk[i]<0)return RF_FORMAT;
        memcpy(value.matrix[0],record->orientation_disk+3,12);
        memcpy(value.matrix[1],record->orientation_disk+6,12);
        memcpy(value.matrix[2],record->orientation_disk,12);
        value.size[0]=record->dimensions_disk[1];value.size[1]=record->dimensions_disk[0];value.size[2]=record->dimensions_disk[2];
    }
    *volume=value;return RF_OK;
}
int rf_trigger_contact_poll(const rf_trigger_gate *gate,const rf_trigger_actor_facts *actor,
    const rf_trigger_volume *volume,const float pose[3][3],rf_trigger_contact_timer *timer,
    int32_t now,uint32_t input,uint32_t *ready)
{
    uint32_t accepted;int status;
    if(!volume || !pose || !timer || !ready)return RF_RANGE;
    status=rf_trigger_eligible(gate,actor,now,input,&accepted);if(status)return status;
    if(accepted && !(gate->flags&4)) {
        if(volume->shape==0)status=rf_trigger_sphere_contact(volume->center,volume->radius,pose[0],&accepted);
        else if(volume->shape==1)status=rf_trigger_box_contact(volume->center,volume->matrix,volume->size,
            gate->flags,pose[0],pose[1],pose[2],&accepted);
        else accepted=0;
        if(status)return status;
    }
    return rf_trigger_contact_delay(timer,now,accepted,ready);
}
int rf_event_explode_action(rf_event_explode_state *state,uint32_t action,
    rf_event_explode_callback callback,void *context)
{
    rf_event_explode_request request={0};
    if(!state || !callback || action>1)return RF_RANGE;
    if(!action)return RF_OK;
    if((state->geometry_flag&255u)==1 && state->room) {
        request.geometry=1;request.effect=-1;request.room=state->room;
        memcpy(request.position,state->position,sizeof(request.position));
        request.direction[0]=1;request.scale=state->scale;
        callback(context,&request);
    }
    memset(&request,0,sizeof(request));request.effect=state->effect;request.room=state->room;
    memcpy(request.position,state->position,sizeof(request.position));
    request.scale=state->scale;request.secondary=state->secondary;
    callback(context,&request);return RF_OK;
}
int rf_event_links_propagate(rf_event_links *links,uint32_t source,uint32_t actor,
    uint32_t mode,rf_event_link_callback callback,void *context)
{
    uint32_t i=0,on=(mode&255u)==1;
    if(!links || !callback)return RF_RANGE;
    for(;;) {
        if(links->count>INT32_MAX || (links->count && !links->handles))return RF_RANGE;
        if(i>=links->count)return RF_OK;
        callback(context,links->handles[i],source,actor,on,!on);
        ++i;
    }
}
typedef struct startup_context {
    rf_runtime_triggers *triggers;rf_runtime_trigger *trigger;rf_runtime_event *event;
    rf_physics_gravity *gravity;rf_startup_events_report *report;int32_t now;int status;
    uint32_t depth;rf_level_particles *particles;
} startup_context;
static void startup_target(startup_context *c,const rf_level_link_target *target,
    uint32_t source,uint32_t actor,uint32_t on);
static void startup_event_action(void *context,rf_event_state *state,uint32_t action,
    uint32_t source,uint32_t actor,uint32_t mode)
{
    startup_context *c=context;uint32_t i;
    if(c->status)return;
    if(action==2) {
        for(i=0;i<c->event->authored->record.link_count && !c->status;++i)
            startup_target(c,c->event->links+i,source,actor,(mode&255u)==1);
        return;
    }
    if(state->type==3) {
        /* 4b9930/4ba330: invert, discard actor, reread source per target.
         * Unlike common propagation, off does not suppress movers; mover
         * and auxiliary target effects remain unimplemented below. */
        for(i=0;i<c->event->authored->record.link_count && !c->status;++i)
            startup_target(c,c->event->links+i,state->source,UINT32_MAX,action==0);
        return;
    }
    if(state->type==39) {
        if(!c->particles || !c->particles->state){++c->report->unsupported_actions;return;}
        c->status=rf_level_particles_set_state(c->particles,c->event->authored->links,
            c->event->authored->record.link_count,action,c->now);
        return;
    }
    /* Delay (48) uses no-op base actions; common scheduling/propagation own it. */
    if(state->type==48)return;
    if(state->type!=44) {++c->report->unsupported_actions;return;}
    c->status=rf_event_gravity_action(c->gravity,c->event->authored->record.values[0],action);
    if(!c->status && action==1)++c->report->gravity_actions;
}
static void startup_target(startup_context *c,const rf_level_link_target *target,
    uint32_t source,uint32_t actor,uint32_t on)
{
    void *object;uint32_t kind;startup_context child;int status;
    if(target->kind!=1) {++c->report->unresolved_targets;return;}
    object=rf_object_registry_lookup(c->triggers->registry,target->value);
    if(!object) {++c->report->unresolved_targets;return;}
    memcpy(&kind,object,4);
    if(kind==5) {
        rf_runtime_trigger *trigger=object;
        if(on)trigger->state.flags&=~16u;else trigger->state.flags|=16u;
        return;
    }
    if(kind!=6) {++c->report->other_targets;return;}
    /* Fail explicitly before exhausting the stock Xbox stack on an immediate
     * cycle. This is a defensive limit, not an original event rule. */
    if(c->depth>=64) {c->status=RF_RANGE;return;}
    child=*c;child.event=object;++child.depth;++c->report->events;
    status=rf_event_activate(&child.event->state,c->now,on?source:actor,actor,on,startup_event_action,&child);
    c->status=status?status:child.status;
    if(!c->status && child.event->state.deadline>=0)++c->report->delayed_events;
}
static void startup_trigger_dispatch(void *context,const rf_auto_trigger_state *state,
    uint32_t actor,uint32_t suppress_movers)
{
    startup_context *c=context;uint32_t i;(void)suppress_movers;++c->report->triggers;
    for(i=0;i<c->trigger->authored->record.link_count && !c->status;++i) {
        startup_target(c,c->trigger->links+i,state->handle,actor,1);
    }
}
int rf_runtime_trigger_contact(rf_runtime_triggers *triggers,uint32_t handle,
    const rf_trigger_actor_facts *actor,const float pose[3][3],
    const rf_trigger_contact_filter *filter,int32_t now,uint32_t input,uint32_t *ready)
{
    rf_runtime_trigger *trigger;rf_trigger_gate gate;uint32_t kind;
    if(!triggers || !triggers->registry || !filter)return RF_RANGE;
    trigger=rf_object_registry_lookup(triggers->registry,handle);if(!trigger)return RF_NOT_FOUND;
    memcpy(&kind,trigger,4);if(kind!=5)return RF_NOT_FOUND;
    gate.flags=trigger->state.flags;gate.activations=(int32_t)trigger->state.count;
    gate.limit=trigger->activation.limit;gate.deadline=trigger->state.deadline;
    gate.filter=filter->kind;gate.attached=filter->attached;
    gate.allowed_count=filter->allowed_count;gate.allowed_handles=filter->allowed_handles;
    return rf_trigger_contact_poll(&gate,actor,&trigger->volume,pose,&trigger->contact_timer,now,input,ready);
}
static void runtime_trigger_dispatch(void *context,rf_trigger_activation *trigger,
    uint32_t actor,uint32_t suppress_movers)
{
    startup_trigger_dispatch(context,&trigger->state,actor,suppress_movers);
}
int rf_runtime_trigger_fire(rf_runtime_triggers *triggers,uint32_t handle,
    uint32_t actor,int32_t now,uint32_t clock_bits,uint32_t blocked,uint32_t suppress_movers,
    rf_physics_gravity *gravity,rf_level_particles *particles,
    rf_startup_events_report *report,uint32_t *fired)
{
    startup_context context={0};rf_runtime_trigger *trigger;uint32_t kind;int status;
    if(!triggers || !triggers->registry || !gravity || !report || !fired)return RF_RANGE;
    trigger=rf_object_registry_lookup(triggers->registry,handle);if(!trigger)return RF_NOT_FOUND;
    memcpy(&kind,trigger,4);if(kind!=5)return RF_NOT_FOUND;
    memset(report,0,sizeof(*report));context.triggers=triggers;context.trigger=trigger;
    context.gravity=gravity;context.particles=particles;context.report=report;context.now=now;
    status=rf_trigger_fire_sp(&trigger->activation,now,clock_bits,blocked,actor,suppress_movers,
        runtime_trigger_dispatch,&context,fired);
    return status?status:context.status;
}
int rf_runtime_startup_events(rf_runtime_triggers *triggers,rf_physics_gravity *gravity,
    int32_t now,uint32_t clock_bits,rf_level_particles *particles,rf_startup_events_report *report)
{
    startup_context context={0};uint32_t i;int status;
    if(!triggers || !gravity || !report || !triggers->registry || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    memset(report,0,sizeof(*report));context.particles=particles;context.triggers=triggers;context.gravity=gravity;context.report=report;context.now=now;
    for(i=0;i<triggers->count;++i) {
        rf_runtime_trigger *trigger=triggers->items+i;context.trigger=trigger;
        if(!(trigger->state.flags&8) || (trigger->state.flags&16))continue;
        /* Script-backed eligibility is unresolved; don't invent its result. */
        if(trigger->authored->record.script[0]) {++report->script_gates;continue;}
        status=rf_auto_trigger_fire(&trigger->state,now,clock_bits,1,startup_trigger_dispatch,&context);
        if(status)return status;if(context.status)return context.status;
    }
    return RF_OK;
}
int rf_runtime_events_tick(rf_runtime_events *events,rf_runtime_triggers *triggers,
    rf_physics_gravity *gravity,int32_t now,rf_level_particles *particles,rf_startup_events_report *report,
    uint32_t *unsupported_pending)
{
    startup_context context={0};uint32_t i;int status,expired;
    if(!events || !triggers || !gravity || !report || !unsupported_pending ||
       !events->registry || events->registry!=triggers->registry || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    memset(report,0,sizeof(*report));*unsupported_pending=0;
    context.particles=particles;context.triggers=triggers;context.gravity=gravity;context.report=report;context.now=now;context.depth=1;
    for(i=0;i<events->count;++i) {
        rf_runtime_event *event=events->items+i;
        if(event->state.deadline<0)continue;
        if(event->state.type!=3 && event->state.type!=44 && event->state.type!=48 &&
           !(event->state.type==39 && particles && particles->state)) {++*unsupported_pending;continue;}
        status=rf_timer_expired(event->state.deadline,now,&expired);if(status)return status;
        if(!expired)continue;
        context.event=event;++report->events;
        status=rf_event_tick(&event->state,now,startup_event_action,&context);
        if(status)return status;if(context.status)return context.status;
    }
    return RF_OK;
}
int rf_runtime_triggers_resolve(rf_runtime_triggers *triggers,
    const rf_level_uid_object *objects,uint32_t object_count,
    const rf_level_uid_key *keys,uint32_t key_count)
{
    uint32_t i,j;int status;
    if(!triggers || (object_count && !objects) || (key_count && !keys))return RF_RANGE;
    for(i=0;i<triggers->count;++i) {
        rf_runtime_trigger *item=triggers->items+i;
        for(j=0;j<item->authored->record.link_count;++j) {
            status=rf_level_link_resolve(item->authored->links[j],objects,object_count,keys,key_count,item->links+j);
            if(status)return status;
        }
    }
    return RF_OK;
}
void rf_runtime_triggers_close(rf_runtime_triggers *triggers)
{
    uint32_t i;if(!triggers)return;
    for(i=0;i<triggers->count;++i)rf_object_registry_remove(triggers->registry,triggers->items[i].handle);
    free(triggers->items);rf_level_owned_triggers_close(&triggers->decoded);memset(triggers,0,sizeof(*triggers));
}
int rf_runtime_triggers_open(const rf_level *level,rf_object_registry *registry,
    uint32_t budget,int32_t now,rf_runtime_triggers *result)
{
    rf_runtime_triggers value={0};uint64_t bytes,payload;uint32_t i,j;int status;
    rf_level_link_target *cursor=NULL;
    if(!level || !registry || !result || result->items || result->decoded.storage ||
       result->count || result->registry || budget<sizeof(value) || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    status=rf_level_owned_triggers_open(level,budget-(uint32_t)sizeof(value),&value.decoded);
    if(status==RF_NOT_FOUND)status=RF_OK;
    if(status)return status;
    bytes=sizeof(value)+(uint64_t)value.decoded.allocated_bytes+
        (uint64_t)value.decoded.count*sizeof(*value.items);
    for(i=0;i<value.decoded.count;++i)bytes+=(uint64_t)value.decoded.items[i].record.link_count*sizeof(*cursor);
    if(bytes>budget || value.decoded.count>registry->count) {status=RF_RANGE;goto failed;}
    if(value.decoded.count) {
        payload=bytes-sizeof(value)-value.decoded.allocated_bytes;
        value.items=calloc(1,(size_t)payload);
        if(!value.items) {status=RF_RANGE;goto failed;}
        cursor=(rf_level_link_target *)(value.items+value.decoded.count);
    }
    for(i=0;i<value.decoded.count;++i) {
        rf_runtime_trigger *item=value.items+i;item->object_kind=5;item->authored=value.decoded.items+i;
        item->links=cursor;
        for(j=0;j<item->authored->record.link_count;++j) {
            cursor->value=item->authored->links[j];cursor->kind=0;cursor->index=UINT32_MAX;++cursor;
        }
        status=rf_auto_trigger_init(&item->state,&item->authored->record,UINT32_MAX,now);
        if(status)goto failed;
        item->activation.limit=(int32_t)item->authored->record.unknown_word;
        status=rf_trigger_volume_init(&item->authored->record,&item->volume);if(status)goto failed;
        item->contact_timer.seconds=item->authored->record.values[1];
        item->contact_timer.deadline=-1;
    }
    value.registry=registry;value.allocated_bytes=(uint32_t)bytes;
    for(i=0;i<value.decoded.count;++i) {
        rf_runtime_trigger *item=value.items+i;
        status=rf_object_registry_insert(registry,item,&item->handle);if(status)goto failed;
        item->state.handle=item->handle;++value.count;
    }
    *result=value;return RF_OK;
failed:
    rf_runtime_triggers_close(&value);return status;
}
void rf_runtime_events_close(rf_runtime_events *events)
{
    uint32_t i;if(!events)return;
    for(i=0;i<events->count;++i)rf_object_registry_remove(events->registry,events->items[i].handle);
    free(events->items);rf_level_owned_events_close(&events->decoded);memset(events,0,sizeof(*events));
}
int rf_runtime_events_resolve(rf_runtime_events *events,
    const rf_level_uid_object *objects,uint32_t object_count,
    const rf_level_uid_key *keys,uint32_t key_count)
{
    uint32_t i,j;int status;
    if(!events || (object_count && !objects) || (key_count && !keys))return RF_RANGE;
    for(i=0;i<events->count;++i)for(j=0;j<events->items[i].authored->record.link_count;++j) {
        status=rf_level_link_resolve(events->items[i].authored->links[j],objects,object_count,
            keys,key_count,events->items[i].links+j);
        if(status)return status;
    }
    return RF_OK;
}
int rf_runtime_events_open(const rf_level *level,rf_object_registry *registry,
    uint32_t budget,rf_runtime_events *result)
{
    rf_runtime_events value={0};uint64_t bytes;uint32_t i,j;int status;
    rf_level_link_target *cursor=NULL;
    if(!level || !registry || !result || result->items || result->decoded.storage ||
       result->count || result->registry || budget<sizeof(value))return RF_RANGE;
    status=rf_level_owned_events_open(level,budget-(uint32_t)sizeof(value),&value.decoded);
    if(status==RF_NOT_FOUND)status=RF_OK;
    if(status)return status;
    bytes=sizeof(value)+(uint64_t)value.decoded.allocated_bytes+
        (uint64_t)value.decoded.count*sizeof(*value.items);
    for(i=0;i<value.decoded.count;++i)bytes+=(uint64_t)value.decoded.items[i].record.link_count*sizeof(*cursor);
    if(bytes>budget || value.decoded.count>registry->count) {status=RF_RANGE;goto failed;}
    for(i=0;i<value.decoded.count;++i) {
        const rf_level_event *record=&value.decoded.items[i].record;
        if(rf_event_type_id(record->type)<0 || !isfinite(record->delay)) {status=RF_FORMAT;goto failed;}
    }
    if(value.decoded.count) {
        value.items=calloc(1,(size_t)(bytes-sizeof(value)-value.decoded.allocated_bytes));
        if(!value.items) {status=RF_RANGE;goto failed;}
        cursor=(rf_level_link_target *)(value.items+value.decoded.count);
    }
    value.registry=registry;value.allocated_bytes=(uint32_t)bytes;
    for(i=0;i<value.decoded.count;++i) {
        rf_runtime_event *item=value.items+i;item->object_kind=6;
        item->authored=value.decoded.items+i;
        item->links=cursor;
        for(j=0;j<item->authored->record.link_count;++j) {
            cursor->value=item->authored->links[j];cursor->kind=0;cursor->index=UINT32_MAX;++cursor;
        }
        item->state.type=(uint32_t)rf_event_type_id(item->authored->record.type);
        item->state.delay=item->authored->record.delay;item->state.deadline=-1;
        /* Generic creator clears flags; actor/source/mode start deterministically
         * at zero here, instead of preserving original uninitialized storage. */
        status=rf_object_registry_insert(registry,item,&item->handle);
        if(status)goto failed;
        ++value.count;
    }
    *result=value;return RF_OK;
failed:
    rf_runtime_events_close(&value);return status;
}
/* Original type table 5a1a3c..5a1ba4, lookup 4bd700. */
static const char *const event_names[90]={
    "Play_Sound", /* 0 */
    "Slay_Object", /* 1 */
    "Remove_Object", /* 2 */
    "Invert", /* 3 */
    "Teleport", /* 4 */
    "Goto", /* 5 */
    "Goto_Player", /* 6 */
    "Look_At", /* 7 */
    "Shoot_At", /* 8 */
    "Shoot_Once", /* 9 */
    "Explode", /* 10 */
    "Play_Animation", /* 11 */
    "Play_Custom_Animation", /* 12 */
    "Heal", /* 13 */
    "Armor", /* 14 */
    "Message", /* 15 */
    "When_Dead", /* 16 */
    "Continuous_Damage", /* 17 */
    "Shake_Player", /* 18 */
    "Give_Item_To_Player", /* 19 */
    "Cyclic_Timer", /* 20 */
    "Switch_Model", /* 21 */
    "Load_Level", /* 22 */
    "Spawn_Object", /* 23 */
    "Make_Invulnerable", /* 24 */
    "Make_Walk", /* 25 */
    "Make_Fly", /* 26 */
    "Drop_Point_Marker", /* 27 */
    "Follow_Waypoints", /* 28 */
    "Follow_Player", /* 29 */
    "Set_Friendliness", /* 30 */
    "Set_Light_State", /* 31 */
    "Switch", /* 32 */
    "Swap_Textures", /* 33 */
    "Set_AI_Mode", /* 34 */
    "Goal_Create", /* 35 */
    "Goal_Check", /* 36 */
    "Goal_Set", /* 37 */
    "Attack", /* 38 */
    "Particle_State", /* 39 */
    "Set_Liquid_Depth", /* 40 */
    "Music_Start", /* 41 */
    "Music_Stop", /* 42 */
    "Bolt_State", /* 43 */
    "Set_Gravity", /* 44 */
    "Alarm_Siren", /* 45 */
    "Alarm", /* 46 */
    "Go_Undercover", /* 47 */
    "Delay", /* 48 */
    "Monitor_State", /* 49 */
    "UnHide", /* 50 */
    "Push_Region_State", /* 51 */
    "When_Hit", /* 52 */
    "Headlamp_State", /* 53 */
    "Item_Pickup_State", /* 54 */
    "Cutscene", /* 55 */
    "Strip_Player_Weapons", /* 56 */
    "Fog_State", /* 57 */
    "Detach", /* 58 */
    "Skybox_State", /* 59 */
    "Force_Monitor_Update", /* 60 */
    "Black_Out_Player", /* 61 */
    "Turn_Off_Physics", /* 62 */
    "Teleport_Player", /* 63 */
    "Holster_Weapon", /* 64 */
    "Holster_Player_Weapon", /* 65 */
    "Modify_Rotating_Mover", /* 66 */
    "Clear_Endgame_If_Killed", /* 67 */
    "Win_PS2_Demo", /* 68 */
    "Enable_Navpoint", /* 69 */
    "Play_Vclip", /* 70 */
    "Endgame", /* 71 */
    "Mover_Pause", /* 72 */
    "Countdown_Begin", /* 73 */
    "Countdown_End", /* 74 */
    "When_Countdown_Over", /* 75 */
    "Activate_Capek_Shield", /* 76 */
    "When_Enter_Vehicle", /* 77 */
    "When_Try_Exit_Vehicle", /* 78 */
    "Fire_Weapon_No_Anim", /* 79 */
    "Never_Leave_Vehicle", /* 80 */
    "Drop_Weapon", /* 81 */
    "Ignite_Entity", /* 82 */
    "When_Cutscene_Over", /* 83 */
    "When_Countdown_Reaches", /* 84 */
    "Display_Fullscreen_Image", /* 85 */
    "Defuse_Nuke", /* 86 */
    "When_Life_Reaches", /* 87 */
    "When_Armor_Reaches", /* 88 */
    "Reverse_Mover", /* 89 */
};
int32_t rf_event_type_id(const char *name)
{
    uint32_t i,j;
    if(!name)return -1;
    for(i=0;i<90;++i) {
        for(j=0;;++j) {
            unsigned char a=(unsigned char)name[j],b=(unsigned char)event_names[i][j];
            if(a>='A' && a<='Z')a+=32;
            if(b>='A' && b<='Z')b+=32;
            if(a!=b)break;
            if(!a)return (int32_t)i;
        }
    }
    return -1;
}
/* Exact binary32 seconds * 1000, truncated toward zero. Integer arithmetic
 * avoids dependence on an ambient x87 precision mode (0.01f is below 10 ms).
 * The original extended-precision product is exact before its ftol call. */
static int trigger_milliseconds(float seconds,int32_t *result)
{
    uint32_t raw,exponent,shift;uint64_t magnitude;
    memcpy(&raw,&seconds,4);exponent=(raw>>23)&255;
    if(exponent==255)return RF_RANGE;
    magnitude=((raw&0x7fffff)|(exponent?0x800000:0))*UINT64_C(1000);
    if(!exponent)exponent=1;
    if(exponent>150) {
        shift=exponent-150;if(shift>=31)return RF_RANGE;
        magnitude<<=shift;
    } else {shift=150-exponent;magnitude=shift>=64?0:magnitude>>shift;}
    if(magnitude>((raw>>31)?UINT64_C(2147483648):(uint64_t)RF_TIMER_PERIOD))return RF_RANGE;
    *result=(raw>>31)?(int32_t)(-(int64_t)magnitude):(int32_t)magnitude;
    return RF_OK;
}
int rf_auto_trigger_init(rf_auto_trigger_state *s,const rf_level_trigger *record,
    uint32_t handle,int32_t now)
{
    static const uint32_t bits[5]={1,2,4,8,128};
    rf_auto_trigger_state value={0};uint32_t i;int status;
    if(!s || !record || record->shape>1 || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    status=trigger_milliseconds(record->timing,&value.cooldown_ms);if(status)return status;
    for(i=0;i<5;++i)if(record->flags[i]==1)value.flags|=bits[i];
    if(record->shape==1 && record->box_flag==1)value.flags|=32;
    if(record->tail_flag)value.flags|=16;
    value.deadline=now;
    value.activation_time_bits=UINT32_C(0xbf800000);value.handle=handle;
    *s=value;return RF_OK;
}
int rf_auto_trigger_fire(rf_auto_trigger_state *s,int32_t now,uint32_t clock_bits,
    int eligible,rf_auto_trigger_callback callback,void *context)
{
    int32_t deadline;int status;
    if(!s || !callback)return RF_RANGE;
    if(!eligible || !(s->flags&8) || (s->flags&16))return RF_OK;
    if(now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    deadline=s->deadline;
    if(s->cooldown_ms>0) {
        status=rf_timer_set(&deadline,now,s->cooldown_ms);if(status)return status;
    }
    callback(context,s,UINT32_MAX,0);
    ++s->count;s->deadline=deadline;s->activation_time_bits=clock_bits;s->flags|=64;
    return RF_OK;
}
int rf_trigger_fire_sp(rf_trigger_activation *trigger,int32_t now,uint32_t clock_bits,
    uint32_t blocked,uint32_t actor,uint32_t suppress_movers,
    rf_trigger_activation_callback callback,void *context,uint32_t *fired)
{
    int32_t deadline;int status;
    if(!trigger || !callback || !fired || blocked>1 || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    if(blocked) {*fired=0;return RF_OK;}
    deadline=trigger->state.deadline;
    if(trigger->state.cooldown_ms>0) {
        status=rf_timer_set(&deadline,now,trigger->state.cooldown_ms);if(status)return status;
    }
    callback(context,trigger,actor,suppress_movers);
    ++trigger->state.count;
    if(trigger->limit!=-1 && (int32_t)trigger->state.count>=trigger->limit && !(trigger->state.flags&8))
        trigger->object_flags|=2;
    trigger->state.deadline=deadline;trigger->state.activation_time_bits=clock_bits;
    trigger->state.flags|=64;*fired=1;return RF_OK;
}
int rf_event_gravity_action(rf_physics_gravity *gravity,float value,uint32_t action)
{
    if(!gravity || action>2)return RF_RANGE;
    return action==1?rf_physics_gravity_set(gravity,value):RF_OK;
}
static int propagates(uint32_t type)
{
    return type!=2 && type!=3 && type!=32 && type!=36 && type!=66 && type!=69 && type!=89;
}
int rf_event_activate(rf_event_state *s,int32_t now,uint32_t source,uint32_t actor,
    uint32_t mode,rf_event_callback callback,void *context)
{
    double milliseconds;int32_t deadline=-1;int status;
    if(!s || !callback || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    mode&=255;
    /* Validate before mutation; original valid inputs use x87 intermediates. */
    if(!(s->flags&1)) {
        if(!isfinite(s->delay))return RF_RANGE;
        if(s->delay>0) {
            milliseconds=(double)s->delay*(s->type==79?(double)0.9827237725257874f:1.0)*1000.0+0.5;
            if(milliseconds>=((double)RF_TIMER_PERIOD+1.0))return RF_RANGE;
            status=rf_timer_set(&deadline,now,(int32_t)milliseconds);if(status)return status;
        }
    }
    s->actor=actor;s->source=source;
    if(s->flags&1)return RF_OK;
    if(s->delay>0) {s->deadline=deadline;s->mode=mode;return RF_OK;}
    rf_timer_clear(&s->deadline);
    callback(context,s,mode==1?1:0,source,actor,mode);
    if(propagates(s->type))callback(context,s,2,source,s->actor,mode);
    return RF_OK;
}
int rf_event_tick(rf_event_state *s,int32_t now,rf_event_callback callback,void *context)
{
    int expired,status;if(!s || !callback)return RF_RANGE;
    status=rf_timer_expired(s->deadline,now,&expired);if(status)return status;
    if(!expired)return RF_OK;
    callback(context,s,(s->mode&255)?1:0,s->source,s->actor,s->mode&255);
    if(propagates(s->type))callback(context,s,2,s->source,s->actor,s->mode&255);
    rf_timer_clear(&s->deadline);return RF_OK;
}
int rf_unhide_init(rf_unhide_state *s,int32_t now)
{
    int32_t deadline;int status;if(!s)return RF_RANGE;
    status=rf_timer_set(&deadline,now,0);if(status)return status;
    s->deadline=deadline;s->on=0;s->off=0;return RF_OK;
}
int rf_unhide_request(rf_unhide_state *s,int unhide)
{
    if(!s)return RF_RANGE;
    if(unhide)s->on=1;else s->off=1;
    return RF_OK;
}
int rf_unhide_tick(rf_unhide_state *s,int32_t now,const uint32_t *links,
    uint32_t count,rf_unhide_target_callback callback,void *context)
{
    uint32_t i;int expired,status,processed=1;
    if(!s || !callback || (count && !links))return RF_RANGE;
    status=rf_timer_expired(s->deadline,now,&expired);if(status)return status;
    if(s->on==1 && expired) {
        status=rf_timer_set(&s->deadline,now,500);if(status)return status;
        for(i=0;i<count;++i)if(!callback(context,links[i],1))processed=0;
        if(processed)s->on=0;
    }
    status=rf_timer_expired(s->deadline,now,&expired);if(status)return status;
    if(s->off==1 && expired) {
        status=rf_timer_set(&s->deadline,now,500);if(status)return status;
        for(i=0;i<count;++i)(void)callback(context,links[i],0);
        s->off=0;
    }
    return RF_OK;
}
