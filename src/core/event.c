#include "rf/event.h"
#include "rf/event_hit.h"
#include "rf/collision.h"
#include "rf/level.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
int rf_campaign_countdown_step(rf_campaign_countdown *countdown,float seconds)
{
    if(!countdown || !isfinite(seconds) || seconds<0 || !isfinite(countdown->remaining) ||
       countdown->remaining<0 || countdown->expiry_pending>1)return RF_RANGE;
    if(countdown->remaining>0 && seconds>0) {
        countdown->remaining-=seconds;
        if(countdown->remaining<=0){countdown->remaining=0;countdown->expiry_pending=1;}
    }
    return RF_OK;
}
int rf_event_switch_links(const rf_switch_state *state,const rf_event_state *event,
    const rf_event_links *links,uint32_t initial,rf_switch_lookup lookup,
    rf_switch_dispatch dispatch,void *context)
{
    uint32_t i,family;int status;
    if(!state || !event || !links || !lookup || !dispatch || (links->count && !links->handles))return RF_RANGE;
    for(i=0;i<links->count;++i) {
        for(family=0;family<6;++family) {
            rf_switch_target target={0};rf_switch_request request;
            status=lookup(context,family,links->handles[i],&target);
            if(status==RF_NOT_FOUND)continue;
            if(status)return status;
            request.family=family;request.token=target.token;request.enabled=state->disabled==0;
            request.source=event->source;request.actor=event->actor;request.flags_only=0;
            if(family==RF_SWITCH_LIGHT && state->disabled>1)break;
            if(family==RF_SWITCH_EVENT) {
                if(target.event_type==17)request.flags_only=1;
                else if(initial&255u)continue;
            }
            if(family==RF_SWITCH_OBJECT && !target.renderable)continue;
            status=dispatch(context,&request);if(status)return status;
            if(family<RF_SWITCH_EVENT)break;
        }
    }
    return RF_OK;
}
int rf_event_switch_init(rf_switch_state *state,uint32_t disabled,int32_t limit,
    float mode,uint32_t unlimited)
{
    rf_switch_state value;
    if(!state || !isfinite(mode) || (double)mode< -2147483648.0 || (double)mode>=2147483648.0)return RF_RANGE;
    value.disabled=disabled;value.limit=limit;value.unlimited=unlimited&255u;
    value.activations=0;value.mode=(int32_t)mode;*state=value;return RF_OK;
}
int rf_event_switch_on(rf_switch_state *state,rf_switch_effect effect,void *context)
{
    if(!state || !effect)return RF_RANGE;
    if(!(state->unlimited&255u) && state->limit<=(int32_t)state->activations)return RF_OK;
    state->disabled=state->disabled==0;
    if((state->mode==1 && state->disabled==1) || (state->mode==2 && state->disabled==0)) {
        state->disabled=state->mode==2;
        effect(context,state,2);return RF_OK;
    }
    effect(context,state,0);effect(context,state,1);++state->activations;
    return RF_OK;
}
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
static int trigger_occupant_inside(const rf_trigger_volume *v,const float position[3],uint32_t *inside)
{
    float a,b,c,t,delta[3];long double partial;uint32_t i;
    if(v->shape==1)return rf_collision_point_oriented_box(position,v->center,v->matrix,v->size,inside);
    if(!isfinite(v->radius))return RF_FORMAT;
    for(i=0;i<3;++i) {
        if(!isfinite(position[i]) || !isfinite(v->center[i]))return RF_FORMAT;
        delta[i]=position[i]-v->center[i];if(!isfinite(delta[i]))return RF_FORMAT;
    }
    a=fabsf(delta[0]);b=fabsf(delta[1]);c=fabsf(delta[2]);
    if(a<b){t=a;a=b;b=t;}if(b<c){t=b;b=c;c=t;}if(a<b){t=a;a=b;b=t;}
    /* 4faf30 / 4fa7a0: stored differences, then extended approximate length. */
    partial=(long double)c*.125f+(long double)b*.25f;
    *inside=(partial*.5f+partial)+a<(long double)v->radius;return RF_OK;
}
int rf_trigger_occupancy(const rf_trigger_volume *volume,
    const rf_trigger_occupant *actors,uint32_t actor_count,
    const rf_trigger_occupant *items,uint32_t item_count,
    rf_trigger_occupant_wake wake,void *context,uint32_t *occupied)
{
    uint32_t i,inside,result=0;int status;
    if(!occupied || (actor_count && !actors) || (item_count && (!items || !wake)))return RF_RANGE;
    if(!volume){*occupied=0;return RF_OK;}
    for(i=0;i<actor_count;++i) {
        if(actors[i].flags&0x4000)continue;
        status=trigger_occupant_inside(volume,actors[i].position,&inside);if(status)return status;
        if(inside){*occupied=1;return RF_OK;}
    }
    for(i=0;i<item_count;++i) {
        status=trigger_occupant_inside(volume,items[i].position,&inside);if(status)return status;
        if(inside){result=1;wake(context,items[i].handle);}
    }
    *occupied=result;return RF_OK;
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
int rf_trigger_reach_point(const rf_trigger_volume *v,const float origin[3],
    float reach,float point[3],uint32_t *found)
{
    float delta[3],target[3],distance=0;uint32_t i,j;
    if(!v || !origin || !point || !found)return RF_RANGE;
    if(!isfinite(reach) || reach<0 || v->shape>1)return RF_FORMAT;
    for(i=0;i<3;i++) {
        if(!isfinite(origin[i]) || !isfinite(v->center[i]))return RF_FORMAT;
        delta[i]=origin[i]-v->center[i];target[i]=v->center[i];
    }
    if(v->shape==0) {
        float length=sqrtf(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
        if(!isfinite(v->radius) || v->radius<0 || !isfinite(length))return RF_FORMAT;
        if(length<=v->radius)memcpy(target,origin,12);
        else if(length>0)for(i=0;i<3;i++)target[i]+=delta[i]*fmaxf(0,v->radius-.0001f)/length;
    } else for(i=0;i<3;i++) {
        float local=0,half;
        if(!isfinite(v->size[i]) || v->size[i]<0)return RF_FORMAT;
        for(j=0;j<3;j++){if(!isfinite(v->matrix[i][j]))return RF_FORMAT;local+=delta[j]*v->matrix[i][j];}
        half=fmaxf(0,v->size[i]*.5f-.0001f);local=fmaxf(-half,fminf(half,local));
        for(j=0;j<3;j++)target[j]+=local*v->matrix[i][j];
    }
    for(i=0;i<3;i++){float d=target[i]-origin[i];distance+=d*d;}
    if(!isfinite(distance))return RF_FORMAT;
    *found=distance<=reach*reach;if(*found)memcpy(point,target,12);return RF_OK;
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
int rf_event_continuous_damage_action(const rf_event_damage_state *state,uint32_t action,
    const rf_event_damage_backend *backend)
{
    rf_event_damage_request request={0};rf_event_damage_target target;uint32_t i;int status;
    if(!state || action>1)return RF_RANGE;
    if(!action)return RF_OK;
    if(!backend || !backend->lookup || !backend->damage || !backend->feedback ||
       state->links.count>INT32_MAX || (state->links.count && !state->links.handles))return RF_RANGE;
    /* 4bb535 fild/fmul/fstp: preserve the integer before multiplication. */
    request.amount=state->rate?(float)((long double)state->rate*state->frame_seconds):10000.0f;
    if(!isfinite(request.amount))return RF_FORMAT;
    request.source=request.other=request.owner=UINT32_MAX;request.kind=state->kind;request.enabled=1;
    for(i=0;i<state->links.count;++i) {
        memset(&target,0,sizeof(target));
        status=backend->lookup(backend->context,state->links.handles[i],0,&target);if(status)return status;
        if(target.present) {request.target=state->links.handles[i];backend->damage(backend->context,&request);}
    }
    if(state->actor==UINT32_MAX)return RF_OK;
    memset(&target,0,sizeof(target));
    status=backend->lookup(backend->context,state->actor,1,&target);if(status)return status;
    if(!target.present)return RF_NOT_FOUND;
    if((target.exclude_a&255)==1 || (target.exclude_b&255)==1)return RF_OK;
    request.target=state->actor;request.source=target.entity_handle;
    backend->damage(backend->context,&request);
    memset(&target,0,sizeof(target));
    status=backend->lookup(backend->context,state->actor,2,&target);if(status)return status;
    if(!target.present)return RF_NOT_FOUND;
    if((target.feedback&255) && (state->kind==0 || state->kind==3 || state->kind==5 || state->kind==6))
        backend->feedback(backend->context,target.feedback_handle,.01f,.5f);
    return RF_OK;
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
int rf_trigger_links_dispatch(const rf_object_registry *registry,rf_event_links *links,
    uint32_t source,uint32_t actor,uint32_t suppress_movers,rf_trigger_link_effect effect,void *context)
{
    uint32_t i=0;
    if(!registry || !links || !effect)return RF_RANGE;
    while(i<links->count) {
        void *object;uint32_t handle,kind;int status;
        if(!links->handles)return RF_RANGE;
        handle=links->handles[i++];object=rf_object_registry_lookup(registry,handle);
        if(!object)continue;memcpy(&kind,object,4);
        if(kind!=5 && kind!=6 && (kind!=8 || (suppress_movers&255)))continue;
        status=effect(context,kind,handle,source,actor);if(status)return status;
    }
    return RF_OK;
}
typedef struct startup_context {
    rf_runtime_triggers *triggers;rf_runtime_trigger *trigger;rf_runtime_event *event;
    rf_physics_gravity *gravity;rf_startup_events_report *report;int32_t now;int status;
    uint32_t depth;rf_level_particles *particles;rf_physics_force_collection *forces;
} startup_context;
static void startup_target(startup_context *c,const rf_level_link_target *target,
    uint32_t source,uint32_t actor,uint32_t on);
int rf_level_transition_enqueue(rf_level_transition_request *request,const rf_level_event *event,uint32_t source,uint32_t actor)
{
    rf_level_transition_request next={0};uint32_t n=0,i,base;
    if(!request || !event)return RF_RANGE;
    if(request->pending)return RF_OK;
    while(n<sizeof(event->texts[0]) && event->texts[0][n])n++;
    if(!n || n==sizeof(event->texts[0]) || !memchr(event->texts[1],0,sizeof(event->texts[1])))return RF_FORMAT;
    base=n;
    /* Shipped campaign scripts retain 48 development .d4l names; the matching
     * archive entries use .rfl. Accept those two suffixes, not arbitrary paths. */
    if(n>=4 && event->texts[0][n-4]=='.' && (event->texts[0][n-1]|32)=='l' &&
       (((event->texts[0][n-3]|32)=='r' && (event->texts[0][n-2]|32)=='f') ||
        ((event->texts[0][n-3]|32)=='d' && event->texts[0][n-2]=='4')))base-=4;
    if(!base || base+4>=sizeof(next.level))return RF_RANGE;
    for(i=0;i<base;i++) {unsigned char c=(unsigned char)event->texts[0][i];
        if(!((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-'))return RF_FORMAT;}
    memcpy(next.level,event->texts[0],base);memcpy(next.level+base,".rfl",5);
    memcpy(next.entrance,event->texts[1],sizeof(next.entrance));memcpy(next.words,event->words,sizeof(next.words));memcpy(next.flags,event->flags,sizeof(next.flags));
    if(!memchr(event->name,0,sizeof(event->name)))return RF_FORMAT;
    memcpy(next.anchor,event->name,sizeof(next.anchor));memcpy(next.anchor_position,event->position,sizeof(next.anchor_position));
    next.pending=1;next.uid=event->uid;next.source=source;next.actor=actor;*request=next;return RF_OK;
}


static int transition_name_equal(const char *a,const char *b)
{
    unsigned char x,y;
    do{x=(unsigned char)*a++;y=(unsigned char)*b++;
       if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;
       if(x!=y)return 0;}while(x);return 1;
}
int rf_level_transition_offset(const rf_level_transition_request *request,const rf_level *level,float offset[3])
{
    rf_level_event_reader reader;rf_level_event event;rf_level_transition_request marker;
    float delta[3]={0};uint32_t i,found=0;int status;
    if(!request || !level || !offset)return RF_RANGE;
    if(!request->pending || !memchr(request->anchor,0,sizeof(request->anchor)) ||
       !memchr(request->level,0,sizeof(request->level)))return RF_FORMAT;
    if(!request->anchor[0])return RF_NOT_FOUND;
    if(!transition_name_equal(request->level,level->entry.name))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(request->anchor_position[i]))return RF_FORMAT;
    status=rf_level_events_begin(level,&reader);if(status)return status;
    while((status=rf_level_event_next(&reader,&event))==RF_OK) {
        if(strcmp(event.type,"Load_Level") || !transition_name_equal(event.name,request->anchor))continue;
        memset(&marker,0,sizeof(marker));status=rf_level_transition_enqueue(&marker,&event,0,0);if(status)return status;
        if(!transition_name_equal(marker.level,request->level))continue;
        if(found++)return RF_FORMAT;
        for(i=0;i<3;i++) {
            delta[i]=event.position[i]-request->anchor_position[i];
            if(!isfinite(event.position[i]) || !isfinite(delta[i]))return RF_FORMAT;
        }
    }
    if(status!=RF_NOT_FOUND)return status;
    if(!found)return RF_NOT_FOUND;
    memcpy(offset,delta,sizeof(delta));return RF_OK;
}


int rf_level_transition_place(const rf_level_transition_request *request,rf_level *level,
    const float position[3],const float orientation[9])
{
    float offset[3],placed[3],facing[9];unsigned i;int status;
    if(!request || !level || !position || !orientation)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(position[i]))return RF_FORMAT;
    for(i=0;i<9;i++)if(!isfinite(orientation[i]))return RF_FORMAT;
    status=rf_level_transition_offset(request,level,offset);if(status)return status;
    for(i=0;i<3;i++){placed[i]=position[i]+offset[i];if(!isfinite(placed[i]))return RF_RANGE;}
    memcpy(facing,orientation,sizeof(facing));
    memcpy(level->player_position,placed,sizeof(placed));memcpy(level->player_orientation,facing,sizeof(facing));
    return RF_OK;
}

static void startup_event_action(void *context,rf_event_state *state,uint32_t action,
    uint32_t source,uint32_t actor,uint32_t mode);
static int startup_switch_ready(const rf_runtime_triggers *triggers)
{
    const rf_runtime_switch_backend *b=triggers->switch_backend;
    return b && b->lookup && b->dispatch && b->sound;
}
static int startup_switch_lookup(void *context,uint32_t family,uint32_t link,rf_switch_target *target)
{
    startup_context *c=context;const rf_runtime_switch_backend *b=c->triggers->switch_backend;
    return b->lookup(b->context,family,link,target);
}
static int startup_switch_dispatch(void *context,const rf_switch_request *request)
{
    startup_context *c=context;const rf_runtime_switch_backend *b=c->triggers->switch_backend;
    void *object;uint32_t kind;startup_context child;int status;
    if(request->family!=RF_SWITCH_TRIGGER && request->family!=RF_SWITCH_EVENT)
        return b->dispatch(b->context,request);
    object=rf_object_registry_lookup(c->triggers->registry,request->token);
    if(!object)return RF_NOT_FOUND;
    memcpy(&kind,object,4);
    if(request->family==RF_SWITCH_TRIGGER) {
        rf_runtime_trigger *trigger=object;if(kind!=5)return RF_NOT_FOUND;
        if(request->enabled)trigger->state.flags&=~16u;else trigger->state.flags|=16u;
        return RF_OK;
    }
    if(kind!=6)return RF_NOT_FOUND;
    child=*c;child.event=object;
    if(request->flags_only) {
        if(child.event->state.type!=17)return RF_FORMAT;
        if(request->enabled)child.event->state.flags&=~1u;else child.event->state.flags|=1u;
        return RF_OK;
    }
    if(c->depth>=64)return RF_RANGE;
    ++child.depth;++c->report->events;
    status=rf_event_activate(&child.event->state,c->now,request->source,request->actor,
        request->enabled,startup_event_action,&child);
    if(!status)status=child.status;
    if(!status && child.event->state.deadline>=0)++c->report->delayed_events;
    return status;
}
static int startup_switch_links(startup_context *c,const rf_switch_state *state,uint32_t initial)
{
    uint32_t i;
    if(!c->event->authored || (c->event->authored->record.link_count && !c->event->links))return RF_RANGE;
    for(i=0;i<c->event->authored->record.link_count && !c->status;++i) {
        uint32_t value=c->event->links[i].value;rf_event_links link={1,&value};
        c->status=rf_event_switch_links(state,&c->event->state,&link,initial,
            startup_switch_lookup,startup_switch_dispatch,c);
    }
    return c->status;
}
int rf_runtime_switch_initialize(rf_runtime_triggers *triggers,uint32_t handle)
{
    startup_context c={0};rf_runtime_event *event;const rf_runtime_switch_backend *b;
    if(!triggers || !triggers->registry)return RF_RANGE;
    b=triggers->switch_backend;if(!b || !b->lookup || !b->dispatch)return RF_RANGE;
    event=rf_object_registry_lookup(triggers->registry,handle);
    if(!event || event->object_kind!=6 || event->state.type!=32)return RF_NOT_FOUND;
    if(!event->switch_state)return RF_RANGE;
    c.triggers=triggers;c.event=event;
    return startup_switch_links(&c,event->switch_state,1);
}
static void startup_switch_effect(void *context,const rf_switch_state *state,uint32_t effect)
{
    startup_context *c=context;const rf_runtime_switch_backend *b=c->triggers->switch_backend;
    if(c->status)return;
    if(effect)c->status=b->sound(b->context,c->event,effect,c->now);
    else c->status=startup_switch_links(c,state,0);
}
static int startup_damage_ready(const rf_runtime_triggers *triggers)
{
    const rf_runtime_damage_backend *b=triggers->damage_backend;
    return b && b->effects.lookup && b->effects.damage && b->effects.feedback;
}
int rf_runtime_goals_initialize(const rf_runtime_events *events,rf_campaign_goals *goals)
{
    uint32_t i,old;int status;
    if(!events || !goals)return RF_RANGE;
    for(i=0;i<events->count;i++)if(events->items[i].state.type==35) {
        const rf_level_event *e=&events->items[i].authored->record;
        old=goals->count;
        /* 462707 -> 4b85b0: words[1] initial count, flags[0]==1 persistent. */
        status=rf_campaign_goal_declare(goals,e->name,e->flags[0]==1);
        if(status)return status;
        if(goals->count!=old)memcpy(&goals->items[old].value,&e->words[1],4);
    }
    return RF_OK;
}
static void startup_event_action(void *context,rf_event_state *state,uint32_t action,
    uint32_t source,uint32_t actor,uint32_t mode)
{
    startup_context *c=context;uint32_t i;
    if(c->status)return;
    /* These effects own no link-target action. Their normal activation still
     * carries ordered outgoing event links after the effect callback. */
    if(action==2 && (state->type==0 || state->type==10 || state->type==15 ||
       state->type==41 || state->type==42 || state->type==61 || state->type==71)) {
        for(i=0;i<c->event->authored->record.link_count && !c->status;++i)
            startup_target(c,c->event->links+i,source,actor,(mode&255u)==1);
        return;
    }
    if(state->type==11 || state->type==12) {
        if(action!=1)return;
        if(!c->triggers->play_animation){++c->report->unsupported_actions;return;}
        for(i=0;i<c->event->authored->record.link_count;i++) {
            const rf_level_link_target *link=c->event->links+i;int status;
            if(link->kind!=1 && link->kind!=2)continue;
            status=c->triggers->play_animation(c->triggers->animation_context,link->value,&c->event->authored->record);
            if(status==RF_NOT_FOUND){++c->report->other_targets;continue;}
            if(status){c->status=status;return;}
        }
        return;
    }
    if(state->type==46) {
        if(action==2)return;
        if(!c->triggers->alarm){++c->report->unsupported_actions;return;}
        c->status=c->triggers->alarm(c->triggers->alarm_context,&c->event->authored->record,
            c->event->links,c->now,action==1);
        return;
    }
    if(state->type==0) {
        if(action==2)return;
        if(!c->triggers->play_sound){++c->report->unsupported_actions;return;}
        c->status=c->triggers->play_sound(c->triggers->sound_context,&c->event->authored->record,c->now,action==1);
        if(c->status==RF_NOT_FOUND){++c->report->other_targets;c->status=RF_OK;}
        return;
    }
    if(state->type==41 || state->type==42) {
        if(action==2)return;
        if(!c->triggers->music){++c->report->unsupported_actions;return;}
        c->status=c->triggers->music(c->triggers->music_context,
            &c->event->authored->record,c->now,action==1);
        return;
    }
    if(state->type==69) {
        if(action==2)return;
        if(!c->triggers->navpoint){++c->report->unsupported_actions;return;}
        for(i=0;i<c->event->authored->record.link_count;i++)if(c->event->links[i].kind==3) {
            int status=c->triggers->navpoint(c->triggers->navpoint_context,c->event->links[i].value,action==1);
            if(status==RF_NOT_FOUND){++c->report->other_targets;continue;}
            if(status){c->status=status;return;}
        }
        return;
    }
    if(state->type==61) {
        if(action==2)return;
        if(!c->triggers->black_out_player){++c->report->unsupported_actions;return;}
        c->status=c->triggers->black_out_player(c->triggers->blackout_context,
            &c->event->authored->record,c->now,action==1);
        return;
    }
    if(state->type==71) {
        if(action!=1)return;
        if(!c->triggers->endgame){++c->report->unsupported_actions;return;}
        c->status=c->triggers->endgame(c->triggers->endgame_context,
            &c->event->authored->record,c->now);
        return;
    }
    if(state->type==67) {
        if(action==2)return;
        if(!c->triggers->clear_endgame_if_killed){++c->report->unsupported_actions;return;}
        for(i=0;i<c->event->authored->record.link_count;i++) {
            const rf_level_link_target *link=c->event->links+i;int status;
            if(link->kind!=1 && link->kind!=2)continue;
            status=c->triggers->clear_endgame_if_killed(c->triggers->clear_endgame_context,link->value,action==1);
            if(status==RF_NOT_FOUND){++c->report->other_targets;continue;}
            if(status){c->status=status;return;}
        }
        return;
    }
    if(state->type==10) {
        if(action==2)return;
        if(!c->triggers->explode){++c->report->unsupported_actions;return;}
        c->status=c->triggers->explode(c->triggers->explode_context,
            &c->event->authored->record,c->now,action==1);
        return;
    }
    if(state->type==7) {
        if(action==2)return;
        if(!c->triggers->look_at){++c->report->unsupported_actions;return;}
        c->status=c->triggers->look_at(c->triggers->look_at_context,
            &c->event->authored->record,c->event->links,action==1);
        return;
    }
    if(state->type==15) {
        int status;
        if(action==2)return;
        if(!c->triggers->show_message){++c->report->unsupported_actions;return;}
        status=c->triggers->show_message(c->triggers->message_context,&c->event->authored->record,c->now,action==1);
        if(status==RF_NOT_FOUND){++c->report->other_targets;return;}
        if(status)c->status=status;
        return;
    }
    if(state->type==1) {
        if(action!=1)return;
        if(!c->triggers->slay_object){++c->report->unsupported_actions;return;}
        for(i=0;i<c->event->authored->record.link_count;i++) {
            const rf_level_link_target *link=c->event->links+i;int status;
            if(link->kind!=1 && link->kind!=2)continue;
            status=c->triggers->slay_object(c->triggers->slay_context,link->value,source,c->now);
            if(status==RF_NOT_FOUND){++c->report->other_targets;continue;}
            if(status){c->status=status;return;}
        }
        return;
    }
    if(state->type==2) {
        if(action!=1)return;
        for(i=0;i<c->event->authored->record.link_count;i++) {
            const rf_level_link_target *link=c->event->links+i;void *object;uint32_t kind;int status;
            if(link->kind!=1 && link->kind!=2)continue;
            object=rf_object_registry_lookup(c->triggers->registry,link->value);if(!object)continue;
            memcpy(&kind,object,4);
            if(kind==6) {
                rf_runtime_event *removed=object;
                removed->state.deadline=-1;removed->state.flags|=1;
                removed->retired=1;
                removed->unhide.on=removed->unhide.off=0;removed->death_fired=1;
            } else if(kind==5) {
                rf_runtime_trigger *removed=object;
                removed->state.flags|=16;removed->activation.object_flags|=2;
            } else {
                if(!c->triggers->remove_object){++c->report->other_targets;continue;}
                status=c->triggers->remove_object(c->triggers->removal_context,link->value);
                if(status==RF_NOT_FOUND){++c->report->other_targets;continue;}
                if(status){c->status=status;return;}continue;
            }
            /* Retain allocation until scene teardown: dispatch may still hold
             * a pointer, including self-removal. Stale handles stop resolving. */
            status=rf_object_registry_remove(c->triggers->registry,link->value);
            if(status){c->status=status;return;}
        }
        return;
    }
    if(state->type==50) {
        if(action!=2)c->status=rf_unhide_request(&c->event->unhide,action==1);
        return;
    }
    if(state->type==24) {
        if(action==2)return;
        if(!c->triggers->set_invulnerable){++c->report->unsupported_actions;return;}
        for(i=0;i<c->event->authored->record.link_count;i++) {
            const rf_level_link_target *link=c->event->links+i;int status;
            if(link->kind!=1 && link->kind!=2)continue;
            status=c->triggers->set_invulnerable(c->triggers->invulnerability_context,link->value,action==1);
            if(status==RF_NOT_FOUND){++c->report->other_targets;continue;}
            if(status){c->status=status;return;}
        }
        return;
    }
    if(action==2) {
        for(i=0;i<c->event->authored->record.link_count && !c->status;++i)
            startup_target(c,c->event->links+i,source,actor,(mode&255u)==1);
        return;
    }
    if(state->type==55) {
        if(action!=1)return;
        if(!c->triggers->start_cutscene){++c->report->unsupported_actions;return;}
        c->status=c->triggers->start_cutscene(c->triggers->cutscene_context,&c->event->authored->record,c->now);
        return;
    }
    if(state->type==83)return; /* When_Cutscene_Over: its outgoing links act in action 2. */
    if(state->type==73 || state->type==74) {
        rf_campaign_countdown *timer=c->triggers->countdown;
        int32_t seconds;
        if(action!=1)return;
        if(!timer){++c->report->unsupported_actions;return;}
        if(state->type==74){timer->remaining=0;return;}
        memcpy(&seconds,&c->event->authored->record.words[0],4);
        if(seconds<0){c->status=RF_FORMAT;return;}
        if(!strcmp(c->event->authored->record.name,"station_blowup")) {
            static const float difficulty_seconds[4]={90,55,45,35};
            if(timer->difficulty<4)timer->remaining=difficulty_seconds[timer->difficulty];
        } else timer->remaining=(float)seconds;
        return;
    }
    if(state->type==34) {
        static const int32_t actions[6]={1,2,4,5,11,-1};
        uint32_t authored_mode=c->event->authored->record.words[0];
        if(action!=1)return; /* Original base OFF has no actor effect. */
        if(!c->triggers->set_ai_mode){++c->report->unsupported_actions;return;}
        /* Factory4b84c0 table59c014, loader translate=1. Guard malformed enums. */
        if(authored_mode>=6){c->status=RF_RANGE;return;}
        for(i=0;i<c->event->authored->record.link_count;i++) {
            const rf_level_link_target *link=c->event->links+i;int status;
            if(link->kind!=1 && link->kind!=2)continue;
            if(!rf_object_registry_lookup(c->triggers->registry,link->value))continue;
            status=c->triggers->set_ai_mode(c->triggers->ai_mode_context,
                link->value,actions[authored_mode],c->now);
            if(status==RF_NOT_FOUND){++c->report->other_targets;continue;}
            if(status){c->status=status;return;}
        }
        return;
    }
    if(state->type==47) {
        if(!c->triggers->set_player_form){++c->report->unsupported_actions;return;}
        c->status=c->triggers->set_player_form(c->triggers->player_form_context,
            c->event->authored->record.words[0],action==1,c->now);
        return;
    }
    if(state->type==76) {
        if(!c->triggers->set_nano_shield){++c->report->unsupported_actions;return;}
        for(i=0;i<c->event->authored->record.link_count;i++) {
            const rf_level_link_target *link=c->event->links+i;int status;
            if(link->kind!=1 && link->kind!=2)continue;
            if(!rf_object_registry_lookup(c->triggers->registry,link->value))continue;
            status=c->triggers->set_nano_shield(c->triggers->nano_shield_context,link->value,action==1);
            if(status==RF_NOT_FOUND){++c->report->other_targets;continue;}
            if(status){c->status=status;return;}
        }
        return;
    }
    if(state->type==20) {
        c->status=rf_event_cycle_enable(&c->event->cycle,action==1);return;
    }
    if(state->type==63) {
        int status;if(action!=1)return;
        if(!c->triggers->teleport_player){++c->report->unsupported_actions;return;}
        status=c->triggers->teleport_player(c->triggers->teleport_context,&c->event->authored->record);
        if(status==RF_NOT_FOUND)++c->report->other_targets;else if(status)c->status=status;
        return;
    }
    if(state->type==56) {
        if(action!=1)return;
        if(!c->triggers->strip_weapons){++c->report->unsupported_actions;return;}
        c->status=c->triggers->strip_weapons(c->triggers->strip_weapons_context);return;
    }
    if(state->type==19) {
        int status;if(action!=1)return;
        if(!c->triggers->give_item){++c->report->unsupported_actions;return;}
        status=c->triggers->give_item(c->triggers->give_item_context,c->event->authored->record.texts[0]);
        if(status==RF_NOT_FOUND)++c->report->other_targets;else c->status=status;
        return;
    }
    if(state->type==13 || state->type==14) {
        const rf_level_event *e=&c->event->authored->record;int32_t amount;int status;
        if(action!=1)return;
        if(!c->triggers->adjust_vitals){++c->report->unsupported_actions;return;}
        memcpy(&amount,e->words,4);
        for(i=0;i<e->link_count;i++) {
            const rf_level_link_target *link=c->event->links+i;
            if(link->kind!=1 && link->kind!=2)continue;
            status=c->triggers->adjust_vitals(c->triggers->vitals_context,link->value,amount,state->type==14);
            if(status==RF_NOT_FOUND){++c->report->other_targets;continue;}
            if(status){c->status=status;return;}
        }
        if(e->flags[0]) {
            status=c->triggers->adjust_vitals(c->triggers->vitals_context,UINT32_MAX,amount,state->type==14);
            if(status!=RF_NOT_FOUND)c->status=status;
        }
        return;
    }
    if(state->type==38) {
        int status;
        if(!c->triggers->attack_npc){++c->report->unsupported_actions;return;}
        status=c->triggers->attack_npc(c->triggers->attack_context,&c->event->authored->record,c->event->links,action==1);
        if(status==RF_NOT_FOUND)++c->report->other_targets;else if(status)c->status=status;
        return;
    }
    if(state->type==5 || state->type==6 || state->type==28) {
        if(!c->triggers->move_npc){++c->report->unsupported_actions;return;}
        for(i=0;i<c->event->authored->record.link_count;i++) {
            const rf_level_link_target *link=c->event->links+i;int status;
            if(link->kind!=1 && link->kind!=2)continue;
            status=c->triggers->move_npc(c->triggers->move_context,link->value,&c->event->authored->record,action==1);
            if(status==RF_NOT_FOUND){++c->report->other_targets;continue;}
            if(status){c->status=status;return;}
        }
        return;
    }
    if(state->type>=35 && state->type<=37) {
        const rf_level_event *e=&c->event->authored->record;uint32_t passed;int status;int32_t threshold;
        /* Create declares at load time; its base action only propagates links. */
        if(state->type==35)return;
        if(!c->triggers->goals){++c->report->unsupported_actions;return;}
        if(state->type==37) {
            status=rf_campaign_goal_adjust(c->triggers->goals,e->texts[0],action==1);
            if(status!=RF_NOT_FOUND)c->status=status;
            return;
        }
        memcpy(&threshold,&e->words[0],4);
        c->status=rf_campaign_goal_check(c->triggers->goals,e->texts[0],threshold,&passed);
        if(!c->status && passed)for(i=0;i<e->link_count && !c->status;i++)
            startup_target(c,c->event->links+i,source,actor,action==1);
        return;
    }
    if(state->type==22) {
        if(!action)return;
        if(!c->triggers->load_level){++c->report->unsupported_actions;return;}
        c->status=c->triggers->load_level(c->triggers->load_level_context,&c->event->authored->record,source,actor);return;
    }
    if(state->type==32) {
        if(!action)return; /* Original off-action has no effect. */
        if(!c->event->switch_state || !startup_switch_ready(c->triggers)) {++c->report->unsupported_actions;return;}
        {
            int status=rf_event_switch_on(c->event->switch_state,startup_switch_effect,c);
            if(status)c->status=status;
        }
        return;
    }
    if(state->type==30) {
        if(!action)return; /* Original4b9f80 off action. */
        if(!c->triggers->set_friendliness){++c->report->unsupported_actions;return;}
        for(i=0;i<c->event->authored->record.link_count;++i) {
            const rf_level_link_target *target=c->event->links+i;int status;
            if((target->kind!=1 && target->kind!=2) ||
               !rf_object_registry_lookup(c->triggers->registry,target->value))continue;
            status=c->triggers->set_friendliness(c->triggers->friendliness_context,
                target->value,c->event->authored->record.words[0]);
            if(status==RF_NOT_FOUND){++c->report->other_targets;continue;}
            if(status){c->status=status;return;}
        }
        return;
    }
    if(state->type==17) {
        const rf_runtime_damage_backend *b=c->triggers->damage_backend;
        rf_event_damage_state damage={0};
        if(!action)return; /* Original4b9f80 off action. */
        if(!startup_damage_ready(c->triggers)){++c->report->unsupported_actions;return;}
        damage.rate=(int32_t)c->event->authored->record.words[0];
        damage.kind=c->event->authored->record.words[1];damage.frame_seconds=b->frame_seconds;
        damage.actor=UINT32_MAX;
        /* Resolved targets are interleaved records, not a contiguous handle
         * array. Dispatch each link without transient allocation, then actor.
         * The documented stable-link contract preserves authored ordering. */
        for(i=0;i<c->event->authored->record.link_count;++i) {
            const rf_level_link_target *target=c->event->links+i;
            if(target->kind!=1 && target->kind!=2)continue;
            damage.links.handles=&target->value;damage.links.count=1;
            c->status=rf_event_continuous_damage_action(&damage,1,&b->effects);if(c->status)return;
        }
        damage.links.handles=NULL;damage.links.count=0;damage.actor=actor;
        c->status=rf_event_continuous_damage_action(&damage,1,&b->effects);return;
    }
    if(state->type==3) {
        /* 4b9930/4ba330: invert, discard actor, reread source per target.
         * Unlike common propagation, off does not suppress movers; mover
         * and auxiliary target effects remain unimplemented below. */
        for(i=0;i<c->event->authored->record.link_count && !c->status;++i)
            startup_target(c,c->event->links+i,state->source,UINT32_MAX,action==0);
        return;
    }
    if(state->type==51) {
        if(!c->forces){++c->report->unsupported_actions;return;}
        c->status=rf_physics_forces_set_state(c->forces->items,c->forces->count,
            c->event->authored->links,c->event->authored->record.link_count,action);
        return;
    }
    if(state->type==39) {
        if(!c->particles || !c->particles->state){++c->report->unsupported_actions;return;}
        c->status=rf_level_particles_set_state(c->particles,c->event->authored->links,
            c->event->authored->record.link_count,action,c->now);
        return;
    }
    /* Delay (48) uses no-op base actions; common scheduling/propagation own it. */
    if(state->type==48 || state->type==16 || state->type==52 || state->type==75 || state->type==84)return;
    if(state->type!=44) {++c->report->unsupported_actions;return;}
    c->status=rf_event_gravity_action(c->gravity,c->event->authored->record.values[0],action);
    if(!c->status && action==1)++c->report->gravity_actions;
}
static void startup_target(startup_context *c,const rf_level_link_target *target,
    uint32_t source,uint32_t actor,uint32_t on)
{
    void *object;uint32_t kind;startup_context child;int status;
    if(target->kind!=1 && target->kind!=2) {++c->report->unresolved_targets;return;}
    object=rf_object_registry_lookup(c->triggers->registry,target->value);
    if(!object) {++c->report->unresolved_targets;return;}
    memcpy(&kind,object,4);
    if(kind==5) {
        rf_runtime_trigger *trigger=object;
        if(on)trigger->state.flags&=~16u;else trigger->state.flags|=16u;
        return;
    }
    if(kind==8) {
        if(!on)return;
        if(!c->triggers->activate_mover){++c->report->other_targets;return;}
        status=c->triggers->activate_mover(c->triggers->mover_context,target->value,source,actor,c->now);
        if(status==RF_NOT_FOUND)++c->report->other_targets;else c->status=status;
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
int rf_runtime_event_fire(rf_runtime_triggers *triggers,uint32_t handle,
    uint32_t source,uint32_t actor,int32_t now,rf_physics_gravity *gravity,
    rf_level_particles *particles, rf_physics_force_collection *forces,rf_startup_events_report *report)
{
    startup_context c={0};rf_level_link_target target={handle,1,0};void *object;uint32_t kind;
    if(!triggers || !triggers->registry || !gravity || !report)return RF_RANGE;
    object=rf_object_registry_lookup(triggers->registry,handle);if(!object)return RF_NOT_FOUND;
    memcpy(&kind,object,4);if(kind!=6)return RF_NOT_FOUND;
    memset(report,0,sizeof(*report));c.triggers=triggers;c.gravity=gravity;c.particles=particles;c.forces=forces;c.report=report;c.now=now;
    startup_target(&c,&target,source,actor,1);return c.status;
}
static void startup_trigger_dispatch(void *context,const rf_auto_trigger_state *state,
    uint32_t actor,uint32_t suppress_movers)
{
    startup_context *c=context;uint32_t i;(void)suppress_movers;++c->report->triggers;
    for(i=0;i<c->trigger->authored->record.link_count && !c->status;++i) {
        startup_target(c,c->trigger->links+i,state->handle,actor,1);
    }
}
int rf_trigger_contact_filter_authored(const rf_runtime_trigger *trigger,
    uint32_t actor_handle,int32_t attached_handle,rf_trigger_contact_filter *result)
{
    rf_trigger_contact_filter value={0};uint32_t i;
    if(!trigger || !trigger->authored || !result)return RF_RANGE;
    value.kind=trigger->authored->record.value_byte;value.attached=attached_handle;
    if(value.kind>4)return RF_NOT_FOUND;
    if(value.kind==2) {
        if(trigger->authored->record.link_count && !trigger->links)return RF_RANGE;
        /* Original 4c06d0 compares the actor against the current +2d4 list.
         * This borrowed singleton preserves that decision without a copy. */
        for(i=0;i<trigger->authored->record.link_count;++i)
            if(trigger->links[i].value==actor_handle) {
                value.allowed_count=1;value.allowed_handles=&trigger->links[i].value;break;
            }
    }
    *result=value;return RF_OK;
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
    rf_physics_force_collection *forces,rf_startup_events_report *report,uint32_t *fired)
{
    startup_context context={0};rf_runtime_trigger *trigger;uint32_t kind;int status;
    if(!triggers || !triggers->registry || !gravity || !report || !fired)return RF_RANGE;
    trigger=rf_object_registry_lookup(triggers->registry,handle);if(!trigger)return RF_NOT_FOUND;
    memcpy(&kind,trigger,4);if(kind!=5)return RF_NOT_FOUND;
    memset(report,0,sizeof(*report));context.triggers=triggers;context.trigger=trigger;
    context.gravity=gravity;context.particles=particles;context.forces=forces;context.report=report;context.now=now;
    status=rf_trigger_fire_sp(&trigger->activation,now,clock_bits,blocked,actor,suppress_movers,
        runtime_trigger_dispatch,&context,fired);
    return status?status:context.status;
}
typedef struct runtime_link_context {
    rf_runtime_triggers *owner;rf_runtime_trigger *trigger;
    rf_trigger_link_effect effect;void *context;int status;
} runtime_link_context;
static void runtime_link_dispatch(void *context,rf_trigger_activation *activation,
    uint32_t actor,uint32_t suppress_movers)
{
    runtime_link_context *c=context;uint32_t i;
    for(i=0;i<c->trigger->authored->record.link_count;++i) {
        const rf_level_link_target *target=c->trigger->links+i;rf_event_links links;
        if(target->kind!=1 && target->kind!=2)continue;
        links.count=1;links.handles=&target->value;
        c->status=rf_trigger_links_dispatch(c->owner->registry,&links,activation->state.handle,
            actor,suppress_movers,c->effect,c->context);
        if(c->status)return;
    }
}
int rf_runtime_trigger_fire_links(rf_runtime_triggers *triggers,uint32_t handle,
    uint32_t actor,int32_t now,uint32_t clock_bits,uint32_t blocked,uint32_t suppress_movers,
    rf_trigger_link_effect effect,void *context,uint32_t *fired)
{
    runtime_link_context c={0};rf_runtime_trigger *trigger;uint32_t kind;int status;
    if(!triggers || !triggers->registry || !effect || !fired)return RF_RANGE;
    trigger=rf_object_registry_lookup(triggers->registry,handle);if(!trigger)return RF_NOT_FOUND;
    memcpy(&kind,trigger,4);if(kind!=5)return RF_NOT_FOUND;
    if(!trigger->authored || (trigger->authored->record.link_count && !trigger->links))return RF_RANGE;
    c.owner=triggers;c.trigger=trigger;c.effect=effect;c.context=context;
    status=rf_trigger_fire_sp(&trigger->activation,now,clock_bits,blocked,actor,suppress_movers,
        runtime_link_dispatch,&c,fired);
    return status?status:c.status;
}
int rf_runtime_startup_events(rf_runtime_triggers *triggers,rf_physics_gravity *gravity,
    int32_t now,uint32_t clock_bits,rf_level_particles *particles, rf_physics_force_collection *forces,rf_startup_events_report *report)
{
    startup_context context={0};uint32_t i;int status;
    if(!triggers || !gravity || !report || !triggers->registry || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    memset(report,0,sizeof(*report));context.particles=particles;context.forces=forces;context.triggers=triggers;context.gravity=gravity;context.report=report;context.now=now;
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
/* Original 4bb3a0 polls links once after base timer processing. A nonzero
 * flags[0] additionally permits any missing object; otherwise no live linked
 * object may remain. Fire targets with sentinel source/actor, once per scene. */
static int runtime_death_poll(startup_context *c,rf_runtime_event *event)
{
    uint32_t i,any_missing=0,any_alive=0,unknown=0;
    if(!c->triggers->death_query || event->death_fired || event->state.deadline>=0)return RF_OK;
    for(i=0;i<event->authored->record.link_count;i++) {
        uint32_t present=0,alive=0,kind=0;int status=RF_NOT_FOUND;void *object=NULL;
        const rf_level_link_target *link=event->links+i;
        if(link->kind==1 || link->kind==2)object=rf_object_registry_lookup(c->triggers->registry,link->value);
        if(object)memcpy(&kind,object,4);
        if((link->kind==1 || link->kind==2) && !object) {
            /* Previously resolved runtime identity, now removed. This is
             * known absence, unlike a never-resolved authored object. */
            present=alive=0;status=RF_OK;
        } else if(kind==5 || kind==6) {present=1;status=RF_OK;}
        else if(c->triggers->death_query)status=c->triggers->death_query(c->triggers->death_context,
            event->authored->links[i],&present,&alive);
        if(status==RF_NOT_FOUND){unknown=1;continue;}
        if(status)return status;
        any_missing|=!present;any_alive|=alive;
    }
    /* Unsupported objects cannot safely be interpreted as dead or absent. */
    if(unknown){++c->report->unsupported_actions;return RF_OK;}
    if(any_alive && !(event->authored->record.flags[0] && any_missing))return RF_OK;
    event->death_fired=1;event->death_time=(uint32_t)c->now;c->event=event;++c->report->events;
    for(i=0;i<event->authored->record.link_count && !c->status;i++)
        startup_target(c,event->links+i,UINT32_MAX,UINT32_MAX,1);
    return c->status;
}
static int runtime_threshold_query(void *context,uint32_t handle,uint32_t armor,float *value)
{
    startup_context *c=context;
    if(!rf_object_registry_lookup(c->triggers->registry,handle))return RF_NOT_FOUND;
    return c->triggers->query_vitals(c->triggers->query_vitals_context,handle,armor,value);
}
static int runtime_threshold_effect(void *context,uint32_t handle)
{
    startup_context *c=context;void *object;uint32_t kind;rf_level_link_target link={handle,1,0};
    if(c->event->retired)return RF_OK;
    object=rf_object_registry_lookup(c->triggers->registry,handle);if(!object)return RF_NOT_FOUND;
    memcpy(&kind,object,4);
    if(kind!=5 && kind!=6 && kind!=8)return RF_NOT_FOUND;
    startup_target(c,&link,UINT32_MAX,UINT32_MAX,1);return c->status;
}
static int runtime_hit_query(void *context,uint32_t handle,uint32_t *flags)
{
    startup_context *c=context;
    if(!rf_object_registry_lookup(c->triggers->registry,handle))return RF_NOT_FOUND;
    return c->triggers->query_hit_flags(c->triggers->hit_flags_context,handle,flags);
}
static int runtime_hit_effect(void *context,uint32_t handle)
{
    startup_context *c=context;void *object;uint32_t kind;rf_level_link_target link={handle,1,0};
    if(c->event->retired)return RF_OK;
    object=rf_object_registry_lookup(c->triggers->registry,handle);if(!object)return RF_NOT_FOUND;
    memcpy(&kind,object,4);
    if(kind!=6 && kind!=8)return RF_NOT_FOUND;
    startup_target(c,&link,UINT32_MAX,UINT32_MAX,1);return c->status;
}
static int runtime_unhide_target(void *context,uint32_t uid,int visible)
{
    startup_context *c=context;uint32_t i;
    for(i=0;i<c->event->authored->record.link_count;i++)if(c->event->authored->links[i]==uid) {
        const rf_level_link_target *link=c->event->links+i;int status;
        if(link->kind!=1 && link->kind!=2)return 1;
        status=c->triggers->set_visible(c->triggers->visibility_context,link->value,visible!=0);
        if(status==RF_NOT_FOUND){++c->report->other_targets;return 1;}
        if(status)c->status=status;
        return status==RF_OK;
    }
    return 1;
}
int rf_runtime_events_tick(rf_runtime_events *events,rf_runtime_triggers *triggers,
    rf_physics_gravity *gravity,int32_t now,rf_level_particles *particles, rf_physics_force_collection *forces,rf_startup_events_report *report,
    uint32_t *unsupported_pending)
{
    startup_context context={0};uint32_t i;int status,expired;
    if(!events || !triggers || !gravity || !report || !unsupported_pending ||
       !events->registry || events->registry!=triggers->registry || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    memset(report,0,sizeof(*report));*unsupported_pending=0;
    context.particles=particles;context.forces=forces;context.triggers=triggers;context.gravity=gravity;context.report=report;context.now=now;context.depth=1;
    for(i=0;i<events->count;++i) {
        rf_runtime_event *event=events->items+i;
        if(event->retired)continue;
        if(event->state.type==50) {
            if(!triggers->set_visible) {
                if(event->state.deadline>=0 || event->unhide.on || event->unhide.off)++*unsupported_pending;
                continue;
            }
            context.event=event;
            /* Keep authored base delay, then service deferred visibility requests. */
            if(event->state.deadline>=0) {
                status=rf_event_tick(&event->state,now,startup_event_action,&context);
                if(status)return status;if(context.status)return context.status;
            }
            status=rf_unhide_tick(&event->unhide,now,event->authored->links,
                event->authored->record.link_count,runtime_unhide_target,&context);
            if(status)return status;if(context.status)return context.status;
            continue;
        }
        if(event->state.type==16) {
            if(event->state.deadline>=0) {
                context.event=event;status=rf_event_tick(&event->state,now,startup_event_action,&context);
                if(status)return status;if(context.status)return context.status;
            }
            status=runtime_death_poll(&context,event);if(status)return status;continue;
        }
        if(event->state.type==52) {
            context.event=event;
            if(event->state.deadline>=0) {
                status=rf_event_tick(&event->state,now,startup_event_action,&context);
                if(status)return status;if(context.status)return context.status;
            }
            if(event->retired)continue;
            if(!triggers->query_hit_flags){++*unsupported_pending;continue;}
            status=rf_event_hit_poll(event->state.deadline,event->links,event->authored->record.link_count,
                runtime_hit_query,runtime_hit_effect,&context);
            if(status)return status;
            continue;
        }
        if(event->state.type==87 || event->state.type==88) {
            /* Original4bd400/4bd500 ignore base disabled/delay state. */
            if(event->threshold.fired)continue;
            if(!triggers->query_vitals){++*unsupported_pending;continue;}
            context.event=event;
            status=rf_event_threshold_poll(&event->threshold,event->state.type==88,
                event->links,event->authored->record.link_count,
                runtime_threshold_query,runtime_threshold_effect,&context);
            if(status)return status;
            if(event->threshold.fired)++report->events;
            continue;
        }
        if(event->state.type==75 || event->state.type==84) {
            rf_campaign_countdown *timer=triggers->countdown;uint32_t j,fire=0;
            context.event=event;
            if(event->state.deadline>=0) {
                status=rf_event_tick(&event->state,now,startup_event_action,&context);
                if(status)return status;if(context.status)return context.status;
            }
            if(event->retired)continue;
            if(!timer){++*unsupported_pending;continue;}
            if(event->state.type==75)fire=timer->expiry_pending;
            else if(timer->remaining>0) {
                int32_t threshold=(int32_t)event->authored->record.words[0];
                if(timer->remaining>(float)threshold)event->countdown_armed=1;
                else if(!event->countdown_fired && timer->remaining<(float)threshold) {
                    int l17=triggers->countdown_level &&
                        (!strcmp(triggers->countdown_level,"L17S1.rfl") ||
                         !strcmp(triggers->countdown_level,"L17S2.rfl") ||
                         !strcmp(triggers->countdown_level,"L17S3.rfl"));
                    fire=!l17 || !strcmp(event->authored->record.name,"countdown_sound") || event->countdown_armed;
                }
            }
            if(!fire)continue;
            for(j=0;j<event->authored->record.link_count;j++) {
                const rf_level_link_target *link=event->links+j;void *object;uint32_t kind;
                if(link->kind!=1 && link->kind!=2)continue;
                object=rf_object_registry_lookup(events->registry,link->value);if(!object)continue;
                memcpy(&kind,object,4);
                if(kind==6 || kind==8 || (event->state.type==84 && kind==5))
                    startup_target(&context,link,UINT32_MAX,UINT32_MAX,1);
                if(context.status)return context.status;
                if(event->retired)break;
            }
            if(event->state.type==75)timer->expiry_pending=0;
            else event->countdown_fired=1;
            ++report->events;
            continue;
        }
        if(event->state.type==20) {
            uint32_t pulse,j;context.event=event;
            /* Original4bb7b0 services the common delayed action first. */
            if(event->state.deadline>=0) {
                status=rf_event_tick(&event->state,now,startup_event_action,&context);
                if(status)return status;if(context.status)return context.status;
            }
            if(event->retired)continue;
            status=rf_event_cycle_tick(&event->cycle,now,&pulse);if(status)return status;
            if(pulse)for(j=0;j<event->authored->record.link_count;j++) {
                const rf_level_link_target *link=event->links+j;void *object;uint32_t kind;
                if(link->kind!=1 && link->kind!=2)continue;
                object=rf_object_registry_lookup(events->registry,link->value);if(!object)continue;
                memcpy(&kind,object,4);
                /* Typed registry makes these independent original lookups
                 * mutually exclusive. Cyclic pulses do not enable triggers. */
                if(kind==6)startup_target(&context,link,UINT32_MAX,UINT32_MAX,1);
                else if(kind==8)startup_target(&context,link,event->state.source,event->state.actor,1);
                if(context.status)return context.status;
                if(event->retired)break;
            }
            continue;
        }
        if(event->state.deadline<0)continue;
        if(event->state.type!=2 && event->state.type!=3 && event->state.type!=44 && event->state.type!=48 &&
           !(event->state.type==39 && particles && particles->state) &&
           !(event->state.type==51 && forces) &&
           !((event->state.type==5 || event->state.type==6 || event->state.type==28) && triggers->move_npc) &&
           !(event->state.type==22 && triggers->load_level) &&
           !(event->state.type==55 && triggers->start_cutscene) &&
           !(event->state.type==83) &&
           !(event->state.type>=35 && event->state.type<=37 && triggers->goals) &&
           !((event->state.type==13 || event->state.type==14) && triggers->adjust_vitals) &&
           !(event->state.type==63 && triggers->teleport_player) &&
           !(event->state.type==56 && triggers->strip_weapons) &&
           !(event->state.type==19 && triggers->give_item) &&
           !(event->state.type==30 && triggers->set_friendliness) &&
           !(event->state.type==24 && triggers->set_invulnerable) &&
           !(event->state.type==76 && triggers->set_nano_shield) &&
           !(event->state.type==34 && triggers->set_ai_mode) &&
           !(event->state.type==47 && triggers->set_player_form) &&
           !(event->state.type==38 && triggers->attack_npc) &&
           !(event->state.type==46 && triggers->alarm) &&
           !((event->state.type==11 || event->state.type==12) && triggers->play_animation) &&
           !(event->state.type==1 && triggers->slay_object) &&
           !(event->state.type==0 && triggers->play_sound) &&
           !((event->state.type==41 || event->state.type==42) && triggers->music) &&
           !(event->state.type==69 && triggers->navpoint) &&
           !((event->state.type==73 || event->state.type==74) && triggers->countdown) &&
           !(event->state.type==61 && triggers->black_out_player) &&
           !(event->state.type==71 && triggers->endgame) &&
           !(event->state.type==67 && triggers->clear_endgame_if_killed) &&
           !(event->state.type==10 && triggers->explode) &&
           !(event->state.type==7 && triggers->look_at) &&
           !(event->state.type==15 && triggers->show_message) &&
           !(event->state.type==17 && startup_damage_ready(triggers)) &&
           !(event->state.type==32 && event->switch_state && startup_switch_ready(triggers))) {++*unsupported_pending;continue;}
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
        if(events->items[i].state.type==69){
            events->items[i].links[j]=(rf_level_link_target){
                events->items[i].authored->links[j],0,UINT32_MAX};
            continue;
        }
        status=rf_level_link_resolve(events->items[i].authored->links[j],objects,object_count,
            keys,key_count,events->items[i].links+j);
        if(status)return status;
    }
    return RF_OK;
}
int rf_runtime_events_bind_navigation(rf_runtime_events *events,
    const rf_level_owned_navigation *navigation)
{
    uint32_t i,j,n;
    if(!events || !navigation || (navigation->count && !navigation->nodes))return RF_RANGE;
    for(i=0;i<events->count;i++)if(events->items[i].state.type==69) {
        rf_runtime_event *event=events->items+i;
        for(j=0;j<event->authored->record.link_count;j++){
            uint32_t uid=event->authored->links[j];rf_level_link_target *link=event->links+j;
            for(n=0;n<navigation->count && navigation->nodes[n].uid!=uid;n++){}
            *link=(rf_level_link_target){n<navigation->count?n:uid,
                n<navigation->count?3u:0u,n<navigation->count?n:UINT32_MAX};
        }
    }
    return RF_OK;
}
int rf_runtime_events_open(const rf_level *level,rf_object_registry *registry,
    uint32_t budget,rf_runtime_events *result)
{
    rf_runtime_events value={0};uint64_t bytes,link_bytes=0;uint32_t i,j,switch_count=0;int status;
    rf_switch_state *switch_cursor=NULL;
    rf_level_link_target *cursor=NULL;
    if(!level || !registry || !result || result->items || result->decoded.storage ||
       result->count || result->registry || budget<sizeof(value))return RF_RANGE;
    status=rf_level_owned_events_open(level,budget-(uint32_t)sizeof(value),&value.decoded);
    if(status==RF_NOT_FOUND)status=RF_OK;
    if(status)return status;
    bytes=sizeof(value)+(uint64_t)value.decoded.allocated_bytes+
        (uint64_t)value.decoded.count*sizeof(*value.items);
    for(i=0;i<value.decoded.count;++i) {
        const rf_level_event *record=&value.decoded.items[i].record;
        int32_t type=rf_event_type_id(record->type);
        if(type<0 || !isfinite(record->delay)) {status=RF_FORMAT;goto failed;}
        link_bytes+=(uint64_t)record->link_count*sizeof(*cursor);
        if(type==32) {
            rf_switch_state checked;
            status=rf_event_switch_init(&checked,record->words[0],(int32_t)record->words[1],record->values[0],record->flags[0]);
            if(status)goto failed;++switch_count;
        }
    }
    bytes+=link_bytes+(uint64_t)switch_count*sizeof(*switch_cursor);
    if(bytes>budget || value.decoded.count>registry->count) {status=RF_RANGE;goto failed;}
    if(value.decoded.count) {
        value.items=calloc(1,(size_t)(bytes-sizeof(value)-value.decoded.allocated_bytes));
        if(!value.items) {status=RF_RANGE;goto failed;}
        cursor=(rf_level_link_target *)(value.items+value.decoded.count);
        switch_cursor=(rf_switch_state *)((unsigned char *)cursor+(size_t)link_bytes);
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
        if(item->state.type==32) {
            const rf_level_event *record=&item->authored->record;
            item->switch_state=switch_cursor++;
            status=rf_event_switch_init(item->switch_state,record->words[0],(int32_t)record->words[1],record->values[0],record->flags[0]);
            if(status)goto failed;
        }
        if(item->state.type==87 || item->state.type==88)
            item->threshold.threshold=(int32_t)item->authored->record.words[0];
        if(item->state.type==20) {
            const rf_level_event *record=&item->authored->record;
            /* Existing scene simulation starts at zero before startup events. */
            status=rf_event_cycle_init(&item->cycle,record->values[0],(int32_t)record->words[0],record->flags[0],0);
            if(status)goto failed;
        }
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

static int trigger_checkpoint_remaining(int32_t deadline,int32_t now,int32_t *remaining)
{
    int status;if(deadline<0){*remaining=-1;return RF_OK;}
    status=rf_timer_remaining(deadline,now,remaining);
    if(!status && *remaining<0)*remaining=0;
    return status;
}
int rf_runtime_trigger_save(const rf_runtime_trigger *trigger,int32_t now,rf_campaign_trigger_state *result)
{
    rf_campaign_trigger_state v;int status;
    if(!trigger || !result || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    v.flags=trigger->state.flags&~64u;v.count=trigger->state.count;
    v.object_flags=trigger->activation.object_flags;v.limit=trigger->activation.limit;
    v.activation_time_bits=trigger->state.activation_time_bits;
    status=trigger_checkpoint_remaining(trigger->state.deadline,now,&v.cooldown_remaining);if(status)return status;
    status=trigger_checkpoint_remaining(trigger->contact_timer.deadline,now,&v.contact_remaining);if(status)return status;
    *result=v;return RF_OK;
}
int rf_runtime_trigger_restore(rf_runtime_trigger *trigger,int32_t now,const rf_campaign_trigger_state *saved)
{
    int32_t cooldown=-1,contact=-1;int status;
    if(!trigger || !saved || now<0 || now>RF_TIMER_PERIOD || saved->cooldown_remaining<-1 || saved->contact_remaining<-1 ||
        saved->cooldown_remaining>RF_TIMER_PERIOD || saved->contact_remaining>RF_TIMER_PERIOD)return RF_RANGE;
    if(saved->cooldown_remaining>=0){status=rf_timer_set(&cooldown,now,saved->cooldown_remaining);if(status)return status;}
    if(saved->contact_remaining>=0){status=rf_timer_set(&contact,now,saved->contact_remaining);if(status)return status;}
    trigger->state.flags=saved->flags&~64u;trigger->state.count=saved->count;
    trigger->activation.object_flags=saved->object_flags;trigger->activation.limit=saved->limit;
    trigger->state.activation_time_bits=saved->activation_time_bits;
    trigger->state.deadline=cooldown;trigger->contact_timer.deadline=contact;
    return RF_OK;
}
