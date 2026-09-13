#include "rf/entity.h"
#include "rf/collision.h"
#include "rf/timer.h"
#include <math.h>
#include <float.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
int rf_entity_ai_set_action(rf_entity_ai_transition_state *s,int32_t action,
    uint32_t a,uint32_t b,float clock,uint32_t network_a,uint32_t network_b)
{
    if(!s || !isfinite(clock) || (double)clock < -2147483648.0 || (double)clock >= 2147483648.0)return RF_RANGE;
    if((network_a|network_b)&255u) {
        if(action==2)action=3;
        s->flags_530&=0xf07fffffu;
    }
    s->action_280=action;s->clock_288=(int32_t)clock;s->argument_28c=a;s->argument_290=b;
    return RF_OK;
}
int rf_entity_ai_set_state(rf_entity_ai_transition_state *s,int32_t requested,float clock)
{
    if(!s || !isfinite(clock) || (double)clock < -2147483648.0 || (double)clock >= 2147483648.0)return RF_RANGE;
    s->state_2b4=requested;s->clock_2bc=(int32_t)clock;return RF_OK;
}
int rf_entity_ai_arbitrate(rf_entity_ai_arbitration_actor *s,const rf_entity_ai_arbitration_frame *f,
    const rf_entity_ai_arbitration_backend *b,uint32_t *result)
{
    rf_entity_ai_arbitration_actor *actor;rf_entity_ai_arbitration_event *event;uint32_t value,i;int status;
    if(!s || !s->owner || !f || !b || !result || !b->global_gate || !b->event || !b->actor || !b->predicate ||
       !b->destination || !b->stance || (f->peer_count && !f->peers) || f->peer_count>65536 ||
       !isfinite(f->clock) || (double)f->clock < -2147483648.0 || (double)f->clock>=2147483648.0)return RF_RANGE;
    for(i=0;i<f->peer_count;++i)if(!f->peers[i])return RF_RANGE;
#define ARB_CALL(expr) do {status=(expr);if(status)return status;if(!s->owner)return RF_FORMAT;} while(0)
#define ARB_NO() do {*result=0;return RF_OK;} while(0)
    if(!(s->owner->scalar_8c0>0))ARB_NO();
    ARB_CALL(b->global_gate(b->context,&value));if((value&255u)==1)ARB_NO();
    ARB_CALL(b->event(b->context,s->event_76c,&event));if(!event || event->type_290!=46)ARB_NO();
    ARB_CALL(b->actor(b->context,s->owner->handle,&actor));if(!actor)ARB_NO();
    for(i=0;i<f->peer_count;++i) {
        rf_entity_ai_arbitration_actor *peer=f->peers[i];
        ARB_CALL(b->predicate(b->context,peer,0x40a110,&value));
        if(!(value&255u) && peer->handle!=s->owner->handle) {
            ARB_CALL(b->predicate(b->context,peer,0x427020,&value));
            if((value&255u)!=1 && peer->event_76c==s->event_76c && peer->transition.action_280==16)ARB_NO();
        }
    }
    ARB_CALL(b->destination(b->context,actor->handle,event->position_40,&value));if(!(value&255u))ARB_NO();
    ARB_CALL(b->stance(b->context,actor));
    memcpy(actor->destination_6ec,event->position_40,sizeof(actor->destination_6ec));
    status=rf_entity_ai_set_action(&s->transition,16,UINT32_MAX,UINT32_MAX,f->clock,f->network_a,f->network_b);if(status)return status;
    status=rf_entity_ai_set_state(&s->transition,1,f->clock);if(status)return status;
    s->transition.flags_530&=0xffdfffffu;*result=1;return RF_OK;
#undef ARB_NO
#undef ARB_CALL
}
int rf_entity_ai_weapon_limit(const int32_t weapons[2],uint32_t override_flag,float override_value,
    uint32_t mode,const float *scalars,uint32_t scalar_count,float *result)
{
    int32_t weapon;float value=.5f;
    if(!weapons || !result)return RF_RANGE;
    if((override_flag&255u)==1){memcpy(result,&override_value,4);return RF_OK;}
    weapon=weapons[(mode&255u)?0:1];
    if(weapon>0){if(!scalars || (uint32_t)weapon>=scalar_count)return RF_RANGE;memcpy(&value,scalars+weapon,4);}
    memcpy(result,&value,4);return RF_OK;
}
int rf_entity_ai_reset_motion(rf_entity_ai_motion_state **owner,uint32_t secondary,
    const rf_entity_ai_motion_backend *b)
{
    int status;uint32_t active;double remaining;int32_t motion;
    if(!owner || !*owner || !b || !b->active || !b->stop || secondary>1 || (secondary && !b->remaining))return RF_RANGE;
    if(secondary) {
        motion=(*owner)->motion_1368;
        if(motion!=-1) {
            status=b->remaining(b->context,(*owner)->model,motion,&remaining);if(status)return status;
            if(!*owner)return RF_FORMAT;
            if(!isnan(remaining) && remaining!=0) {
                status=b->stop(b->context,(*owner)->model);if(status)return status;
                if(!*owner)return RF_FORMAT;(*owner)->motion_1368=-1;
            }
        }
        (*owner)->flags_810&=0xfdffffffu;(*owner)->word_834=UINT32_MAX;
    }
    motion=(*owner)->action_1364;
    if(motion!=-1) {
        status=b->active(b->context,*owner,motion,&active);if(status)return status;
        if(!*owner)return RF_FORMAT;
        if((active&255u)==1) {
            status=b->stop(b->context,(*owner)->model);if(status)return status;
            if(!*owner)return RF_FORMAT;(*owner)->action_1364=-1;
        }
    }
    return RF_OK;
}
static int ai_recovery_timer(int32_t *timer,int32_t now,double seconds)
{
    double milliseconds=seconds*1000.0;
    if(!isfinite(milliseconds) || milliseconds < -2147483648.0 || milliseconds>=2147483648.0)return RF_RANGE;
    return rf_timer_set(timer,now,(int32_t)milliseconds);
}
int rf_entity_ai_recover(rf_entity_ai_recovery *s,int32_t now,const rf_entity_ai_recovery_backend *b)
{
    rf_entity_ai_recovery *actor=NULL;uint32_t found;float health;double seconds=0;int status,pending;
    if(!s || !s->owner || !b || !b->actor || !b->object_health || !b->playback || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    status=b->actor(b->context,s->owner->handle,&actor);if(status)return status;if(!actor)return RF_OK;
    if(s->action==13) {
        if(!s->owner)return RF_FORMAT;
        status=b->object_health(b->context,s->owner->parent,&found,&health);if(status)return status;
        if(found && health>0)return RF_OK;
        actor->word_7bc=0;
    }
    if(!(s->flags_7d0&0x100u))return RF_OK;
    status=rf_timer_pending(s->timer_514,now,&pending);if(status)return status;
    if(pending || (actor->flags_810&1u) || actor->motion_cd4==-1)return RF_OK;
    status=b->playback(b->context,actor,RF_AI_RECOVERY_STOP,&seconds);if(status)return status;
    status=b->playback(b->context,actor,RF_AI_RECOVERY_START,&seconds);if(status)return status;
    status=b->playback(b->context,actor,RF_AI_RECOVERY_DURATION,&seconds);if(status)return status;
    status=ai_recovery_timer(&s->timer_514,now,seconds);if(status)return status;
    return ai_recovery_timer(&s->timer_518,now,(double)actor->class_seconds_f78);
}
int rf_entity_ai_select(rf_entity_ai_actor *s,const rf_entity_ai_select_frame *f,const rf_entity_ai_select_backend *b)
{
    rf_entity_ai_actor *actor,*target;uint32_t value,i;int status;int32_t next=2;
    float point[3]={0},delta[3],distance;double scalar=0;
    if(!s || !s->owner || !f || !b || !b->lookup || !b->call || !f->random ||
       (f->peer_count && !f->peers) || f->peer_count>65536 || !isfinite(f->clock) ||
       (double)f->clock < -2147483648.0 || (double)f->clock>=2147483648.0 ||
       f->now_ms<0 || f->now_ms>RF_TIMER_PERIOD)return RF_RANGE;
    for(i=0;i<f->peer_count;++i)if(!f->peers[i])return RF_RANGE;
#define AI_CALL(op,subject) do {status=b->call(b->context,s,(subject),(op),point,&value,&scalar);if(status)return status;if(!s->owner)return RF_FORMAT;} while(0)
#define AI_STATE(n) rf_entity_ai_set_state(&s->transition,(n),f->clock)
    status=b->lookup(b->context,s->owner->handle,&actor);if(status)return status;
    if(!actor || (s->transition.flags_530&0x40000000u))return RF_OK;
    AI_CALL(0x427020,actor);if((value&255u)==1)return RF_OK;
    AI_CALL(0x4174c0,s);if(value){AI_CALL(0x407ee0,s);return RF_OK;}
    if(s->transition.action_280==17)return RF_OK;
    AI_CALL(0x4087a0,s);if((value&255u)==1)return RF_OK;
    AI_CALL(0x408dc0,actor);if(!(value&255u))return RF_OK;
    actor->flags_810&=0xfdffffffu;
    if(s->transition.action_280==11)return AI_STATE(9);
    AI_CALL(0x4091d0,s);AI_CALL(0x409210,s);
    s->owner->word_834=UINT32_MAX;s->owner->flags_810&=0xfffffff7u;
    AI_CALL(0x408ef0,s);if((value&255u)==1){AI_CALL(0x408f20,s);}
    status=rf_timer_set_random(&s->timer_4d4,f->now_ms,2000,4000,f->random);if(status)return status;
    AI_CALL(0x427fb0,actor);if(value&255u){AI_CALL(0x42a020,actor);if(!(value&255u)){AI_CALL(0x4280b0,actor);}}
    if(!((f->network_a|f->network_b)&255u))for(i=0;i<f->peer_count;++i) {
        rf_entity_ai_actor *peer=f->peers[i];
        if(peer->group==s->owner->group && peer!=s->owner &&
           (peer->transition.action_280==2 || peer->transition.action_280==4)) {
            AI_CALL(0x40a110,actor);if((value&255u)!=1)peer->transition.flags_530|=0x20000u;
        }
    }
    if(s->transition.action_280==3){AI_CALL(0x40a210,s->owner);if((value&255u)==1)return RF_OK;}
    else {status=rf_entity_ai_set_action(&s->transition,3,UINT32_MAX,UINT32_MAX,f->clock,f->network_a,f->network_b);if(status)return status;}
    AI_CALL(0x408d90,s);if((value&255u)==1)next=3;
    status=b->lookup(b->context,s->target_560,&target);if(status)return status;
    if(target) {
        AI_CALL(0x406b70,s);if(value&255u) {
            point[0]=point[1]=point[2]=0;AI_CALL(0x401060,s);AI_CALL(0x40ac90,s->owner);
            if((value&255u)==1)next=1;
        }
        for(i=0;i<3;++i){delta[i]=(float)((double)s->owner->position[i]-target->position[i]);if(!isfinite(delta[i]))return RF_RANGE;}
        distance=(float)sqrt((double)delta[0]*delta[0]+(double)delta[1]*delta[1]+(double)delta[2]*delta[2]);
        if(!isfinite(distance))return RF_RANGE;
        AI_CALL(0x4062c0,s);
        if(!(value&255u) && (next==2 || next==3)) {
            AI_CALL(0x401cc0,s);
            /* Original FCOMP tests C0: unordered also enters this branch. */
            if(isnan(scalar) || scalar<(double)distance) {
                AI_CALL(0x4065d0,s);if(s->transition.state_2b4==11)return RF_OK;
                AI_CALL(0x408d90,s);if(value&255u)return RF_OK;
                return AI_STATE(2);
            }
        }
    }
    return AI_STATE(next);
#undef AI_CALL
#undef AI_STATE
}
rf_entity_loader_created *rf_entity_loader_create(const rf_entity_loader_creation *input,
    rf_entity_loader_created *(*create)(void *,const rf_entity_loader_create_request *),void *context)
{
    rf_entity_loader_create_request request;rf_entity_loader_created *main,*secondary;
    if(input->class_id<0 || ((input->multiplayer&255u) && (input->excluded&255u)))return NULL;
    request.class_id=input->class_id;request.name=input->name;request.uid=-1;
    request.position=input->position;request.orientation=input->orientation;
    request.flags=(input->hidden&255u?2u:0u)|(input->other_flag&255u?4u:0u);request.player_index=-1;
    main=create(context,&request);if(!main)return NULL;
    main->linked_146c=UINT32_MAX;
    if((input->special_name&255u) && input->matching_level && input->special_class>=0) {
        request.class_id=input->special_class;request.name="masako_endgame";request.flags=2;
        secondary=create(context,&request);
        if(secondary) {
            secondary->field_7c8=1;secondary->flags_814|=0x10u;secondary->field_7cc=10.0f;
            main->linked_146c=secondary->handle;*secondary->class_flags_728|=0x100u;
        }
    }
    return main;
}

int rf_entity_impact_damage(float impact_speed,uint32_t falling,int32_t contact_material,
    uint32_t kind_one,uint32_t object_flags,float *amount,uint32_t *eligible)
{
    float excess,value;double scaled;
    if(!amount || !eligible || !isfinite(impact_speed))return RF_RANGE;
    excess=(float)((double)impact_speed-7.0);
    if(excess<0)excess=0;
    scaled=excess;
    if(!(falling&255u) && contact_material==3)scaled*=.5;
    value=(float)(scaled*scaled);
    if(kind_one&255u)value=(float)((double)value+value);
    if(!isfinite(value))return RF_RANGE;
    *amount=value;*eligible=value>=10 && value>0 && !(object_flags&4u);return RF_OK;
}
int rf_entity_impact_process_sp(rf_entity_impact_actor *actor,float speed,const rf_entity_impact_backend *backend)
{
    float amount;uint32_t eligible,suppressed=0,falling;int status;rf_damage_request request;
    if(!actor || !backend || !backend->suppressed || !backend->damage || !backend->lethal_sound || !backend->player_feedback)return RF_RANGE;
    falling=rf_entity_falling((int32_t)actor->movement_mode,actor->use_kind,actor->support_material);
    status=rf_entity_impact_damage(speed,falling,actor->contact_material,actor->use_kind==1,actor->object_flags,&amount,&eligible);
    if(status || !eligible)return status;
    status=backend->suppressed(backend->context,actor,&suppressed);if(status || (suppressed&255u))return status;
    request.amount=amount;request.source=UINT32_MAX;request.kind=9;request.argument6=0;request.auxiliary_uid=UINT32_MAX;request.force=0;
    status=backend->damage(backend->context,actor,&request);if(status)return status;
    /* Original fcomp/test AH bit0 treats unordered health like negative. */
    if(!(actor->health>=0))return backend->lethal_sound(backend->context,actor);
    return backend->player_feedback(backend->context,actor,amount);
}
void rf_entity_creation_vitals(rf_entity_creation_vitals_state *state,
    const rf_entity_creation_vitals_class *definition,uint32_t network_mode)
{
    if(network_mode&255u)state->armor=0;
    else memcpy(&state->armor,&definition->armor,4);
    state->field_840=definition->field_764;
    if(!(definition->health>=0)) {state->health=100;state->object_flags|=4u;}
    else memcpy(&state->health,&definition->health,4);
}
int rf_entity_sphere_overrides(rf_entity_class_sphere *spheres,uint32_t count,
    const rf_entity_sphere_override *overrides,uint32_t override_count,uint8_t network_mode)
{
    uint32_t i,j,k;
    if(count>8 || override_count>8 || (count && !spheres) || (override_count && !overrides))return RF_RANGE;
    for(i=0;i<count;++i)if(!memchr(spheres[i].name,0,24))return RF_FORMAT;
    for(i=0;i<override_count;++i)if(!memchr(overrides[i].name,0,24))return RF_FORMAT;
    for(i=0;i<override_count;++i) {
        const rf_entity_sphere_override *o=overrides+i;
        for(j=0;j<count;++j) {
            rf_entity_class_sphere *s=spheres+j;
            for(k=0;o->name[k];++k) {
                unsigned a=(unsigned char)s->name[k],b=(unsigned char)o->name[k];
                if(a>='A' && a<='Z')a+=32;if(b>='A' && b<='Z')b+=32;
                if(a!=b)break;
            }
            if(o->name[k])continue;
            s->scalar_sp=o->scalar_sp;s->scalar_mp=o->scalar_mp;
            if(o->radius>0)s->radius=o->radius;
            if(o->parameter_10>0) {s->parameter_10=o->parameter_10;s->opaque_14=o->opaque_14;}
            s->selected_scalar=network_mode?o->scalar_mp:o->scalar_sp;
            break;
        }
    }
    return RF_OK;
}
uint32_t rf_entity_creation_physics_flags(uint32_t creation_flags,uint32_t class_flags_724,
    uint32_t class_flags_728,uint32_t class_kind_1b4,uint8_t network_mode)
{
    uint32_t flags=0x80000000u;
    if((class_flags_728&2) || !(class_flags_724&0x40000))flags|=0x70;
    if(class_kind_1b4==4)flags|=0x1000;
    if((class_flags_724&0x4000) && (creation_flags&1))flags|=0x80;
    flags|=(class_flags_724&0x401200)?0x4000:8;
    if(network_mode && (creation_flags&1))flags|=0x8000;
    return flags;
}
uint32_t rf_entity_creation_object_flags(uint32_t creation_flags,uint32_t descriptor_kind)
{
    uint32_t flags=descriptor_kind==3?0x10000u:0u;
    if(creation_flags&1)flags|=8;
    if(creation_flags&2)flags|=0x4000;
    if(creation_flags&4)flags|=0x20000;
    return flags;
}

int rf_entity_view_register(rf_object_registry *objects,rf_entity_registry *entities,
    rf_entity_view *view,rf_registered_entity_view *wrapper)
{
    uint32_t slot,handle,i;int status;
    if(!objects || !entities || !view || !wrapper || wrapper->view || view->type!=0 || !objects->count)return RF_RANGE;
    if(objects->head>=RF_OBJECT_CAPACITY)return RF_RANGE;
    for(i=0;i<RF_OBJECT_SLOTS;i++)if(entities->slots[i]==view)return RF_RANGE;
    slot=objects->free_slots[objects->head];
    if(slot>=RF_OBJECT_SLOTS || entities->slots[slot])return RF_RANGE;
    status=rf_object_registry_insert(objects,wrapper,&handle);if(status)return status;
    wrapper->object_kind=0;wrapper->handle=handle;wrapper->view=view;
    view->handle=(int32_t)handle;entities->slots[handle&0xffffu]=view;return RF_OK;
}
int rf_entity_view_unregister(rf_object_registry *objects,rf_entity_registry *entities,
    rf_registered_entity_view *wrapper)
{
    uint32_t slot;int status;
    if(!objects || !entities || !wrapper || !wrapper->view)return RF_RANGE;
    slot=wrapper->handle&0xffffu;
    if(slot>=RF_OBJECT_SLOTS || rf_object_registry_lookup(objects,wrapper->handle)!=wrapper ||
       entities->slots[slot]!=wrapper->view || (uint32_t)wrapper->view->handle!=wrapper->handle)return RF_NOT_FOUND;
    status=rf_object_registry_remove(objects,wrapper->handle);if(status)return status;
    entities->slots[slot]=NULL;wrapper->view->handle=-1;memset(wrapper,0,sizeof(*wrapper));return RF_OK;
}
const rf_entity_view *rf_object_lookup(const rf_entity_registry *registry, int32_t handle)
{
    uint32_t index=(uint32_t)handle & 0xffffu;
    const rf_entity_view *object;
    if (!registry || handle==-1 || index>=RF_OBJECT_SLOTS) return 0;
    object=registry->slots[index];
    return object && object->handle==handle ? object : 0;
}
const rf_entity_view *rf_entity_lookup(const rf_entity_registry *registry, int32_t handle)
{
    const rf_entity_view *object=rf_object_lookup(registry,handle);
    return object && object->type==0 ? object : 0;
}

int rf_entity_has_weapon(const rf_entity_registry *registry, const rf_entity_view *entity, int *result)
{
    const rf_entity_view *owner;
    uint32_t depth=0,i; int32_t handle;
    if (!registry || !result) return RF_RANGE;
    while (entity) {
        if (entity->weapons[0]!=-1 || entity->weapons[1]!=-1) { *result=1; return RF_OK; }
        owner=entity->weapon_owner;
        if (!owner) break;
        if (!isfinite(owner->base_speed)) return RF_FORMAT;
        if (owner->base_speed!=0) break;
        if (owner->occupant_count && !owner->occupants) return RF_RANGE;
        handle=-1;
        for (i=0;i<owner->occupant_count;++i) if (owner->occupants[i]!=-1) { handle=owner->occupants[i]; break; }
        entity=rf_entity_lookup(registry,handle);
        /* Once followed, every node is in the 1024-slot registry. A chain
         * longer than it must cycle. The original repeats unsuccessful walks
         * twice; stable read-only views make the repeated result identical. */
        if (entity && ++depth>RF_OBJECT_SLOTS) return RF_FORMAT;
    }
    *result=0; return RF_OK;
}

int rf_entity_ai_route_limit(const rf_entity_registry *registry,const rf_entity_view *inventory,
    const float *scalars,uint32_t scalar_count,
    int (*override_read)(void *,const rf_entity_view *,uint32_t *,float *),void *context,float *result)
{
    const rf_entity_view *current=inventory,*owner,*next;uint32_t depth=0,i,flag;int has,status;int32_t handle;
    float override_value,primary,secondary;
    if(!registry || !current || !override_read || !result)return RF_RANGE;
    for(;;) {
        status=rf_entity_has_weapon(registry,current,&has);if(status)return status;if(has)break;
        owner=current->weapon_owner;if(!owner || owner->base_speed!=0)break;
        if(owner->occupant_count && !owner->occupants)return RF_RANGE;
        handle=-1;for(i=0;i<owner->occupant_count;++i)if(owner->occupants[i]!=-1){handle=owner->occupants[i];break;}
        next=rf_entity_lookup(registry,handle);if(!next)break;
        if(++depth>RF_OBJECT_SLOTS)return RF_FORMAT;current=next;
    }
    status=override_read(context,current,&flag,&override_value);if(status)return status;
    status=rf_entity_ai_weapon_limit(current->weapons,flag,override_value,0,scalars,scalar_count,&secondary);if(status)return status;
    status=rf_entity_ai_weapon_limit(current->weapons,flag,override_value,1,scalars,scalar_count,&primary);if(status)return status;
    if(primary>secondary)memcpy(result,&primary,4);else memcpy(result,&secondary,4);
    return RF_OK;
}

int rf_entity_combat_predicates(const rf_entity_registry *registry, const rf_entity_view *entity,
    const int32_t *attached, uint32_t attached_count, int *ready, int *eligible)
{
    const rf_entity_view *other;
    int selected=0,has,status,linked_turret; uint32_t i;
    if (!registry || !ready || !eligible || (attached_count && !attached)) return RF_RANGE;
    if (entity) {
        if (entity->flags_810 & 0x10u) selected=1;
        else {
            status=rf_entity_has_weapon(registry,entity,&has); if (status!=RF_OK) return status;
            if (has) {
                selected=(entity->flags_7c & 8u)!=0;
                for (i=0;!selected && i<attached_count;++i) {
                    other=rf_entity_lookup(registry,attached[i]);
                    if (other && other->linked_handle==entity->handle) selected=1;
                }
                if (!selected && entity->action_520!=7 && entity->action_520!=16 && !(entity->flags_7d0 & 4u))
                    selected=((entity->flags_7d0 & 2u) && !(entity->flags_810 & 1u)) || (entity->flags_7d0 & 1u);
            }
        }
    }
    has=0;
    if (selected && !(entity->flags_810 & 1u)) {
        other=rf_entity_lookup(registry,entity->linked_handle);
        linked_turret=other && other->class_type==4;
        has=linked_turret || !(entity->flags_810 & 0x800u);
    }
    *ready=selected; *eligible=has; return RF_OK;
}

int rf_entity_room_refresh(rf_entity_room_state *state,const float position[3],
    int local_player,rf_entity_room_locator locate,rf_entity_room_notify notify,void *context)
{
    rf_entity_room_result found={0};uint32_t i;int moved=0,status;
    if(!state || !position || !locate)return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(position[i]) || !isfinite(state->query_position[i]))return RF_FORMAT;
        if(position[i]!=state->query_position[i])moved=1;
    }
    /* For finite float coordinates, original positive squared distance is
     * equivalent to any differing component (including subnormal movement). */
    if(!state->room || moved) {
        status=locate(context,position,&found);if(status!=RF_OK)return status;
        if(found.room) {
            if(found.room!=state->room && local_player && notify) {
                int underwater=found.liquid &&
                    (double)found.minimum_y+(double)found.liquid_depth>=(double)position[1];
                notify(context,underwater?"underwater":found.name);
            }
            state->room=found.room;
            memcpy(state->query_position,position,12);
        }
    }
    state->flags&=~0x04000000u;
    return RF_OK;
}

int rf_entity_controller_alert(const rf_entity_registry *registry,int32_t actor,
    const rf_entity_view *local,uint32_t gate_7cabd4,uint32_t gate_7cabb0,uint32_t *request)
{
    const rf_entity_view *object,*linked;uint32_t restricted=0;
    if(!registry || !request)return RF_RANGE;
    object=rf_object_lookup(registry,actor);
    if(!object || !(object->flags_7c&8)) {*request=0;return RF_OK;}
    if(local) {
        linked=rf_entity_lookup(registry,local->linked_handle);
        restricted=!(linked && linked->class_type==4) && (local->flags_810&0x800);
    }
    *request=!restricted || !(gate_7cabd4&255) || (gate_7cabb0&255)==1;
    return RF_OK;
}

int rf_entity_damage_vitals_sp(rf_entity_damage_vitals *state,float amount,
    int32_t kind,float multiplier,uint32_t clock_bits,float *scaled_amount)
{
    rf_entity_damage_vitals value;float scaled;long double absorbed=0;
    if(!state || !scaled_amount)return RF_RANGE;
    if(!isfinite(state->health) || !isfinite(state->armor) || !isfinite(amount) ||
       (kind!=-1 && !isfinite(multiplier)))return RF_FORMAT;
    value=*state;scaled=kind==-1?amount:(float)((long double)amount*multiplier);
    if(!isfinite(scaled))return RF_FORMAT;
    if(value.armor>0) {
        if(kind!=5 && kind!=6) {
            absorbed=kind==4?(long double)scaled:(long double)scaled*.52f;
            if((long double)value.armor-absorbed<=0)absorbed=value.armor;
        }
        value.armor=(float)((long double)value.armor-absorbed);
    }
    value.health=(float)((long double)value.health-((long double)scaled-absorbed));
    value.last_damage_time=clock_bits;
    if(!isfinite(value.health) || !isfinite(value.armor))return RF_FORMAT;
    if(value.health>=0 && value.health<=.5f)value.health=-.1f;
    *state=value;*scaled_amount=scaled;return RF_OK;
}

int rf_entity_damage_credit_sp(rf_entity_damage_credit *state,int32_t kind,
    uint32_t source,int32_t auxiliary_uid,const rf_entity_damage_uid *entities,uint32_t count)
{
    uint32_t i,responsible=source;
    if(!state)return RF_RANGE;
    if(!isfinite(state->health))return RF_FORMAT;
    if(state->health>0)return RF_OK;
    if(kind==4 && source==UINT32_MAX) {
        if(state->burn_present)responsible=state->burn_source_handle;
        else if(auxiliary_uid!=-1) {
            if(count>INT32_MAX || (count && !entities))return RF_RANGE;
            for(i=0;i<count;++i)if(entities[i].uid==auxiliary_uid){responsible=entities[i].handle;break;}
        }
    }
    state->responsible_handle=responsible;return RF_OK;
}

int rf_entity_damage_sp(rf_entity_damage_state *s,float amount,int32_t kind,
    uint32_t source,int32_t auxiliary_uid,float multiplier,uint32_t clock_bits,
    const rf_damage_effect_backend *be,float *result)
{
    rf_entity_damage_vitals vitals;rf_damage_effect_input input;float scaled;int status;
    if(!s || !be || !be->resolve_uid || !result)return RF_RANGE;
    input.old_health=s->effects.health;input.incoming=amount;input.kind=kind;
    input.source=source;input.auxiliary_uid=auxiliary_uid;
    vitals.health=s->effects.health;vitals.armor=s->effects.armor;vitals.last_damage_time=s->last_damage_time;
    status=rf_entity_damage_vitals_sp(&vitals,amount,kind,multiplier,clock_bits,&scaled);
    if(status)return status;
    s->effects.health=vitals.health;s->effects.armor=vitals.armor;s->last_damage_time=vitals.last_damage_time;
    if(vitals.health<=0) {
        uint32_t credit=source;
        if(kind==4 && source==UINT32_MAX) {
            if(s->effects.burn)credit=s->burn_source;
            else if(auxiliary_uid!=-1)credit=be->resolve_uid(be->context,auxiliary_uid);
        }
        s->responsible_handle=credit;
    }
    input.scaled=scaled;
    status=rf_entity_damage_effects(&s->effects,&input,be);if(status)return status;
    *result=scaled;return RF_OK;
}
int rf_entity_damage_effects(rf_damage_effect_state *s,const rf_damage_effect_input *in,
    const rf_damage_effect_backend *be)
{
    double fraction;float duration;uint32_t source,affiliation;int exists,create;
    if(!s || !in || !be || !be->predicate || !be->resolve_uid || !be->source ||
       !be->create_burn || !be->random || !be->notify || !be->playing || !be->play_kind6)return RF_RANGE;
    if(!isfinite(s->health) || !isfinite(s->armor) || !isfinite(s->class_health) ||
       !isfinite(s->class_armor) || !isfinite(in->incoming) || !isfinite(in->scaled) ||
       !isfinite(in->old_health) || s->class_health==0)return RF_FORMAT;
    if(in->incoming>5)be->notify(be->context,RF_DAMAGE_PAIN_ANIMATION,s->handle,0,0);
    fraction=(double)in->incoming/s->class_health;
    if(fraction>.001 && in->kind!=10) {
        float argument=(float)fraction;if(!isfinite(argument))return RF_FORMAT;
        be->notify(be->context,RF_DAMAGE_PAIN_SOUND,s->handle,argument,0);
    }
    if(in->kind==4) {
        if(!s->burn) {
            if(s->class_armor==0 || (double)s->armor/s->class_armor<.5 || (s->flags_814&0x8000)) {
                if(!(be->predicate(be->context,RF_DAMAGE_CLASS_ONE,s->handle)&255) &&
                   !(be->predicate(be->context,RF_DAMAGE_LINKED_CLASS_ONE,s->handle)&255) &&
                   !(be->predicate(be->context,RF_DAMAGE_PLAYER,s->handle)&255) &&
                   !(be->predicate(be->context,RF_DAMAGE_UNOWNED_PLAYER,s->handle)&255)) {
                    source=in->source;
                    if(source==UINT32_MAX && in->auxiliary_uid!=-1)
                        source=be->resolve_uid(be->context,in->auxiliary_uid);
                    exists=be->source(be->context,source,&affiliation);create=!exists;
                    if(exists)create=((be->predicate(be->context,RF_DAMAGE_PLAYER,source)&255) &&
                        !(s->flags_814&0x2000)) || affiliation!=s->affiliation;
                    if(create)s->burn=be->create_burn(be->context,s->handle,exists?source:UINT32_MAX);
                    if(s->burn && !(s->class_flags_728&16)) {
                        duration=be->random(be->context,5,10);if(!isfinite(duration))return RF_FORMAT;
                        be->notify(be->context,RF_DAMAGE_BURN_REACTION,s->handle,duration,0);
                    }
                }
            } else if(s->flags_814&0x2000) {
                duration=be->random(be->context,3,5);if(!isfinite(duration))return RF_FORMAT;
                be->notify(be->context,RF_DAMAGE_ARMOR_REACTION,s->handle,duration,in->source);
            }
        }
        s->flags_814&=0xffff5fff;
    }
    if(!(s->flags_810&0x80000000) && in->kind==6 && !be->playing(be->context,s->voice))
        s->voice=be->play_kind6(be->context,s->handle);
    if((be->predicate(be->context,RF_DAMAGE_PLAYER,s->handle)&255) && in->old_health>0 && in->scaled>0 && in->kind!=10)
        be->notify(be->context,RF_DAMAGE_PLAYER_FEEDBACK,s->handle,0,0);
    if(!(be->predicate(be->context,RF_DAMAGE_OBJECT_PLAYER_FLAG,s->handle)&255) && s->health>0)
        be->notify(be->context,RF_DAMAGE_AI_REACTION,s->handle,in->incoming,in->source);
    return RF_OK;
}
uint32_t rf_entity_armor_immunity(float armor,uint32_t class_flags_724,uint32_t flags_814)
{
    return (class_flags_724&0x02000000) && armor>0 && !(flags_814&0x20);
}
int rf_damage_dispatch_sp(uint32_t target,const rf_damage_request *request,
    float difficulty_multiplier,const rf_damage_backend *backend,float *result)
{
    rf_damage_object *object;float amount,value=0,next;
    if(!request || !backend || !backend->lookup || !backend->predicate || !backend->effect || !result)return RF_RANGE;
    if(!isfinite(request->amount) || !isfinite(difficulty_multiplier))return RF_FORMAT;
    object=backend->lookup(backend->context,target);
    if(!object){*result=0;return RF_OK;}
    if(!isfinite(object->health))return RF_FORMAT;
    amount=request->amount;
    if(amount<.001f){*result=0;return RF_OK;}
    object->flags|=0x200000;
    if(!(request->force&255)) {
        if(object->flags&4){*result=0;return RF_OK;}
        if(backend->predicate(backend->context,0,target,object) &&
           (backend->predicate(backend->context,1,target,object)&255)){*result=0;return RF_OK;}
        if(request->kind!=9 && (backend->predicate(backend->context,2,target,object)&255)) {
            amount*=difficulty_multiplier;
            if(!isfinite(amount))return RF_FORMAT;
        }
    }
    if(object->type==0 || object->type==4 || object->type==7) {
        next=backend->effect(backend->context,object,amount,request->source,request->kind,
            object->type==0?request->auxiliary_uid:object->type==4?request->argument6:0);
        if(object->type==0)value=next;
    } else if(object->type==2 || (object->type==3 && amount>100)) {
        next=object->health-amount;if(!isfinite(next))return RF_FORMAT;object->health=next;
    }
    if(backend->predicate(backend->context,2,target,object)&255)
        if(object->health>0 && object->health<=.5f)object->health=0;
    if(!isfinite(value) || !isfinite(object->health))return RF_FORMAT;
    *result=value;return RF_OK;
}
int rf_entity_damage_sound(rf_entity_damage_sound_state *state,float fraction,
    uint32_t predicate_a,uint32_t predicate_b,int32_t now,const rf_entity_damage_sound_backend *backend)
{
    int32_t sample,deadline;int expired,status;uint32_t i;
    if(!state || !backend || !backend->resolve || !backend->playing || !backend->play)return RF_RANGE;
    if(!isfinite(state->health) || !isfinite(fraction))return RF_FORMAT;
    for(i=0;i<3;++i)if(!isfinite(state->position[i]))return RF_FORMAT;
    if(state->health<=0 && state->death_descriptor!=-1) {
        if(!(state->flags&4)) {
            sample=backend->resolve(backend->context,state->death_class);
            backend->play(backend->context,state->position,sample);
            state->flags|=4;
        }
        return RF_OK;
    }
    if((predicate_a&255) && (predicate_b&255))return RF_OK;
    if(state->action==1 || state->action==17)return RF_OK;
    status=rf_timer_expired(state->deadline,now,&expired);if(status)return status;
    if(!expired)return RF_OK;
    status=rf_timer_set(&deadline,now,1000);if(status)return status;
    state->deadline=deadline;
    /* Original compares the float input to binary64 .3, not .3f. */
    sample=backend->resolve(backend->context,(double)fraction>.3?state->heavy_class:state->light_class);
    if(sample!=-1 && !backend->playing(backend->context,state->voice))
        backend->play(backend->context,state->position,sample);
    return RF_OK;
}

int rf_entity_plan_footsteps(const rf_entity_footstep_input *input,rf_motion_playback_state *playback,
    const rf_motion_marker_names *markers,uint32_t marker_count,
    const rf_entity_footstep_group *groups,uint32_t group_count,rf_entity_footstep_plan *plan)
{
    rf_entity_footstep_plan next={0};uint32_t side,fired;int status;
    static const char *names[2]={"footstep_left","footstep_right"};
    if(!input || !playback || !plan)return RF_RANGE;
    if(input->linked_handle!=-1){*plan=next;return RF_OK;}
    if((input->object_flags&8) && input->player_present && !input->view_mode){next.alternate=1;*plan=next;return RF_OK;}
    if(input->surface>=10 || (!groups && group_count))return RF_RANGE;
    for(side=0;side<2;++side) {
        int32_t group;const rf_entity_footstep_group *source;rf_entity_footstep_request *request;
        status=rf_motion_consume_marker(playback,markers,marker_count,names[side],&fired);if(status)return status;
        if(!fired)continue;
        group=input->groups[input->surface];if(group<0)group=input->groups[0];
        if(group<0){*plan=next;return RF_OK;}
        if((uint32_t)group>=group_count || groups[group].count<0)return RF_RANGE;
        source=groups+group;request=next.requests+next.count++;
        request->group=group;request->count=source->count/2;
        if((uint64_t)source->first+(side?(uint32_t)request->count:0)>UINT32_MAX)return RF_RANGE;
        request->first=source->first+(side?(uint32_t)request->count:0);
        memcpy(request->position,input->position,12);request->position[1]=input->position[1]-input->vertical_offset;
        request->parameters[0]=1;request->parameters[1]=input->side_value;
    }
    *plan=next;return RF_OK;
}

int rf_entity_animation_should_advance(const rf_entity_animation_gate *input)
{
    int allowed;
    if(!input || !input->model_present || input->model_kind!=2)return 0;
    allowed=input->descriptor_present!=0;
    if(allowed && !(input->flags&0x80000000u)) {
        if(!(input->descriptor_flag&255u) &&
           (input->action_520==1 || input->action_520==2 || input->action_520==13))allowed=0;
        else if(input->lod_distance_count>2 && input->distance>45.0f)allowed=0;
    }
    return (input->predicate&255u)==1 || allowed;
}

int rf_entity_support_route(const rf_entity_support_gate *input)
{
    if(!input)return RF_ENTITY_SUPPORT_NONE;
    if(input->flags_810&2u)return RF_ENTITY_SUPPORT_FALL;
    if((input->falling&255u) || (input->movement_mode==1 && input->linked_handle==-1 &&
       ((input->moved&255u) || (input->body_flags&0x400000u) || (input->special&255u))))
        return RF_ENTITY_SUPPORT_QUERY;
    return RF_ENTITY_SUPPORT_NONE;
}

int rf_entity_support_moved(uint32_t *flags,const float previous[3],const float current[3])
{
    int moved;uint32_t i;double distance=0;
    if(!flags)return 0;
    moved=(*flags&0x02000000u)!=0;
    if(!moved && (*flags&0x04000000u)) {
        if(!previous || !current)return 0;
        for(i=0;i<3;++i) {
            volatile float delta=(float)((double)previous[i]-current[i]);
            distance+=(double)delta*delta;
        }
        moved=distance>0;
    }
    if(moved)*flags=(*flags&~0x02000000u)|0x04000000u;
    return moved;
}

void rf_entity_position_snapshot(uint32_t *flags,float previous[3],const float published[3])
{
    if(!flags || !previous || !published)return;
    memmove(previous,published,12);
    *flags &= ~0x01000000u;
}

int rf_entity_support_contact_route(float fraction,double upward_dot,uint32_t resolved,
    uint32_t object_type,uint32_t body_flags,uint32_t falling)
{
    if(fraction>=1 || !(upward_dot>=0.5) || (resolved && object_type==3 && (body_flags&0x80000000u)))
        return (falling&255u)?RF_ENTITY_CONTACT_NONE:RF_ENTITY_CONTACT_FALL;
    return resolved?RF_ENTITY_CONTACT_MOVING:RF_ENTITY_CONTACT_STATIC;
}

int rf_entity_land_process(rf_entity_land_actor *actor,const rf_entity_land_backend *backend)
{
    double speed,relative;uint32_t i,slot=0;int status;rf_entity_landing_state *s;
    if(!actor || !backend || !backend->sound || !backend->transition)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(actor->velocity[i]) || !isfinite(actor->contact_velocity[i]) || !isfinite(actor->previous_support[i]))return RF_FORMAT;
    relative=(double)actor->contact_velocity[1]-actor->velocity[1];
    speed=sqrt(((double)actor->velocity[0]*actor->velocity[0]+(double)actor->velocity[1]*actor->velocity[1])+(double)actor->velocity[2]*actor->velocity[2]);
    s=&actor->state;
    if(relative>.25 || speed>.5) {
        if(actor->material>=10)return RF_RANGE;
        if(actor->material>=0 && actor->groups[actor->material]>0)slot=(uint32_t)actor->material;
        if((s->actor_flags&0x1000u) && actor->groups[4]>0)slot=4;
        status=backend->sound(backend->context,actor,actor->groups[slot]);if(status)return status;
    }
    status=rf_physics_landing_velocity(actor->velocity,actor->previous_support,actor->contact_velocity,actor->velocity);if(status)return status;
    if(s->action==4)return backend->transition(backend->context,actor,(s->actor_flags&0x100000u)?RF_ENTITY_LAND_NORMAL:RF_ENTITY_LAND_SLOW);
    if(s->class_flags&0x02000000u) {
        status=backend->transition(backend->context,actor,RF_ENTITY_LAND_SPECIAL);if(status)return status;
    }
    status=backend->transition(backend->context,actor,(s->actor_flags&0x400u)?RF_ENTITY_LAND_CROUCH:RF_ENTITY_LAND_NORMAL);if(status)return status;
    s->body_flags&=~0x200000u;return RF_OK;
}
int rf_entity_landing_finish(rf_entity_landing_state *state,rf_entity_landing_effect effect,void *context)
{
    if(!state || !effect)return RF_RANGE;
    if(state->action==4) {
        effect(context,state,(state->actor_flags&0x100000u)?RF_ENTITY_LAND_NORMAL:RF_ENTITY_LAND_SLOW);
        return RF_OK;
    }
    if(state->class_flags&0x02000000u)effect(context,state,RF_ENTITY_LAND_SPECIAL);
    effect(context,state,(state->actor_flags&0x400u)?RF_ENTITY_LAND_CROUCH:RF_ENTITY_LAND_NORMAL);
    state->body_flags&=~0x200000u;return RF_OK;
}

int rf_entity_pain_react(rf_entity_pain_state *s,const rf_entity_pain_backend *b)
{
    int32_t action,delay;double milliseconds;
    if(!s || !b || !b->query || !b->effect || !b->duration)return RF_RANGE;
    if((b->query(b->context,RF_PAIN_PLAYER)&255) && !b->query(b->context,RF_PAIN_PLAYER_MODE))return RF_OK;
    if(!(b->query(b->context,RF_PAIN_COOLDOWN)&255))return RF_OK;
    if((b->query(b->context,RF_PAIN_EXCLUDED)&255)==1)return RF_OK;
    if(!(b->query(b->context,RF_PAIN_AI_ENABLED)&255) && s->motions[28]!=-1)return RF_OK;
    if((b->query(b->context,RF_PAIN_AI_BLOCKED)&255)==1 ||
       (b->query(b->context,RF_PAIN_AI_TIMER)&255)==1 ||
       (b->query(b->context,RF_PAIN_FIRE_PRIMARY)&255)==1 ||
       (b->query(b->context,RF_PAIN_FIRE_SECONDARY)&255)==1)return RF_OK;
    action=s->selected_action;
    if(action==-1)action=(b->query(b->context,RF_PAIN_COMBAT)&255)==1?23:22;
    if(action<0 || action>=45)return RF_RANGE;
    if(s->motions[action]==-1)return RF_OK;
    b->effect(b->context,RF_PAIN_RESET_WEAPON,s->handle,(uint32_t)s->primary_weapon);
    s->selected_action=action;
    b->effect(b->context,RF_PAIN_START,(uint32_t)action,0);
    b->effect(b->context,RF_PAIN_RESET_COOLDOWN,1000,2000);
    milliseconds=b->duration(b->context,s->model,s->motions[action])*1000.0+0.5;
    if(!isfinite(milliseconds) || milliseconds < -2147483648.0 || milliseconds>=2147483398.0)return RF_RANGE;
    delay=(int32_t)milliseconds+250;
    b->effect(b->context,RF_PAIN_SET_LOCK,(uint32_t)delay,0);
    return RF_OK;
}

uint32_t rf_entity_falling(int32_t movement_mode,uint32_t use_kind,int32_t contact_material)
{
    return movement_mode==3 || movement_mode==8 || (use_kind==1 && contact_material==-1);
}

uint32_t rf_entity_death_entry_sp(rf_entity_death_entry_state *state,uint32_t falling)
{
    if(state->flags_810&1u)return 0;
    memset(state->vector_714,0,sizeof(state->vector_714));
    if(!(falling&255u))memset(state->vector_144,0,sizeof(state->vector_144));
    memset(state->vector_150,0,sizeof(state->vector_150));
    state->flags_810|=1u;
    state->flags_1a8&=~0x8000u;
    return 1;
}

int rf_entity_death_motion_sp(rf_entity_death_motion_state *s,uint32_t player,
    const rf_entity_death_motion_backend *b)
{
    int32_t action;uint32_t pose,i,blend;
    if(!s || !b || !b->call)return RF_RANGE;
    if(player&255u){s->action_824=-1;return RF_OK;}
    if(s->flags_810&0x80u)return RF_OK;
    action=s->requested_83c;
    if(action==-1)action=(int32_t)b->call(b->context,RF_DEATH_MOTION_SELECT,0,0);
    if(action<-1 || action>=45)return RF_RANGE;
    if(b->call(b->context,RF_DEATH_MOTION_SKELETAL,0,0)&255u) {
        b->call(b->context,RF_DEATH_MOTION_RESET,s->model,0);
        pose=b->call(b->context,RF_DEATH_MOTION_POSE,s->model,0);
        if(!pose)return RF_RANGE;
        for(i=0;i<3;++i)if(s->base_bones[i]>=0) {
            if(i==2)s->word_1468=0;else s->word_1464=0;
            b->call(b->context,RF_DEATH_MOTION_CLEAR_BONE,pose,(uint32_t)s->base_bones[i]);
        }
    }
    if(action==-1 || s->motions[action]==-1){s->action_824=-1;return RF_OK;}
    if(s->model) {
        pose=b->call(b->context,RF_DEATH_MOTION_POSE,s->model,0);
        if(pose)for(i=0;i<2;++i)
            b->call(b->context,RF_DEATH_MOTION_CLEAR_BONE,pose,(uint32_t)s->effective_bones[i]);
    }
    s->action_824=action;blend=1;
    if(s->class_flags_724&0x200000u){s->flags_810|=0x02000000u;blend=0;}
    b->call(b->context,RF_DEATH_MOTION_PLAY,(uint32_t)action,blend);
    if(blend)s->flags_810|=8u;
    return RF_OK;
}

static int death_drop_name(const char *name,const char *query)
{
    unsigned char a,b;if(!name)return 0;
    do {a=(unsigned char)*name++;b=(unsigned char)*query++;
        if(a>='A' && a<='Z')a+=32;if(b>='A' && b<='Z')b+=32;
        if(a!=b)return 0;
    }while(a);return 1;
}
int rf_entity_death_drop(const rf_entity_death_drop_source *s,const rf_entity_death_drop_backend *b,
    rf_entity_death_drop_item **result)
{
    float start[3],end_y,delta[3]={0},size,offset;rf_entity_death_drop_hit hit={0};
    rf_entity_death_drop_item *item;uint32_t i;int status;
    if(!s || !b || !b->query || !b->create || !b->bounds || !result)return RF_RANGE;
    *result=NULL;if(s->item==-1)return RF_OK;
    if(s->item<0 || !isfinite(s->extent_7c4))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(s->position[i]))return RF_RANGE;
    memcpy(start,s->position,12);start[1]=(float)((double)start[1]+.5);
    end_y=(float)((double)s->position[1]-s->extent_7c4);
    delta[1]=(float)((double)end_y-start[1]);
    if(!isfinite(start[1]) || !isfinite(delta[1]))return RF_RANGE;
    status=b->query(b->context,start,delta,&hit);if(status)return status;
    if(hit.count<=0)return RF_OK;
    for(i=0;i<64 && !s->owned[i];++i){}if(i==64)return RF_OK;
    for(i=0;i<3;++i)if(!isfinite(hit.point[i]) || !isfinite(hit.normal[i]))return RF_RANGE;
    item=b->create(b->context,s->item,s->handle,hit.point);*result=item;
    if(!item)return RF_OK;item->flags_2bc|=8u;
    if(death_drop_name(item->name,"medical kit") || death_drop_name(item->name,"riot_stick_battery")) {
        item->base_position[1]=(float)((double)item->base_position[1]+(double).05f);
    } else {
        status=b->bounds(b->context,item->model,&size);if(status)return status;
        if(!isfinite(size))return RF_RANGE;
        for(i=0;i<3;++i) {
            offset=(float)((double)hit.normal[i]*size);
            item->base_position[i]=(float)((double)item->base_position[i]+offset);
        }
    }
    memcpy(item->position,item->base_position,12);return RF_OK;
}

static int corpse_retention_eligible(const rf_corpse_retention_node *n)
{return !(n->flags_29c&0x43u) && !(n->object_flags_7c&0x4000u);}
int rf_corpse_fade_step(rf_corpse_fade_state *s,float dt,uint32_t *continue_tick)
{
    double remaining;
    if(!s || !continue_tick || !isfinite(s->health_34))return RF_RANGE;
    if(s->health_34<0) {s->object_flags_7c|=2u;*continue_tick=0;return RF_OK;}
    if(s->flags_29c&1u) {
        if(!isfinite(dt) || dt<0 || !isfinite(s->fade_298))return RF_RANGE;
        remaining=(double)s->fade_298-dt;if(!isfinite((float)remaining))return RF_RANGE;
        s->fade_298=(float)remaining;
        if(remaining<=0)s->object_flags_7c|=2u;
    }
    *continue_tick=1;return RF_OK;
}
int rf_corpse_retention_apply(rf_corpse_retention_node *head,uint32_t limit,uint32_t *faded)
{
    rf_corpse_retention_node *n,*oldest;uint32_t visits=0,count=0,total,i;
    if(!faded)return RF_RANGE;
    for(n=head;n;n=n->next) {
        if(visits==limit)return RF_RANGE;++visits;
        if(!isfinite(n->created_294))return RF_RANGE;
        if(corpse_retention_eligible(n))++count;
    }
    total=count>5?count-5:0;
    for(i=0;i<total;i++) {
        oldest=NULL;
        for(n=head;n;n=n->next)if(corpse_retention_eligible(n) && (!oldest || n->created_294<oldest->created_294))oldest=n;
        oldest->fade_298=1.0f;oldest->flags_29c|=1u;
    }
    *faded=total;return RF_OK;
}

int rf_corpse_update(rf_corpse_update_state *s,float dt,int32_t now,
    rf_corpse_emitter_link *emitters,uint32_t limit,const rf_corpse_update_backend *b)
{
    rf_corpse_emitter_link *e;rf_corpse_item_view *item;uint32_t proceed,visits=0;
    int status,expired;double value,duration;float point[3],seconds;
    if(!s || !b || !b->reset || !b->play || !b->duration || !b->advance || !b->pose ||
       !b->item || !b->follow_point || !b->move_item)return RF_RANGE;
    status=rf_corpse_fade_step(&s->fade,dt,&proceed);if(status || !proceed)return status;
    if(!isfinite(dt) || dt<0)return RF_RANGE;
    status=rf_timer_expired(s->emitter_deadline_2ac,now,&expired);if(status)return status;
    if(expired) {
        for(e=emitters;e;e=e->next) {if(visits==limit || !e->enabled)return RF_RANGE;++visits;}
        rf_timer_clear(&s->emitter_deadline_2ac);
        for(e=emitters;e;e=e->next)*e->enabled&=~255u;
    }
    if(!isfinite(s->value_2b0))return RF_RANGE;
    if(s->value_2b0>0) {
        if(!isfinite(s->class_value))return RF_RANGE;
        value=(double)s->value_2b0-(((double)dt*(double)0.002f)*s->class_value);
        if(!isfinite((float)value))return RF_RANGE;s->value_2b0=(float)value;
    }
    if(s->motion_2b8>=0 && (s->fade.flags_29c&8u)) {
        b->reset(b->context,s->model);
        b->play(b->context,s->model,s->motion_2b8);
        duration=b->duration(b->context,s->model,s->motion_2b8);seconds=(float)duration;
        if(!isfinite(seconds))return RF_RANGE;
        b->advance(b->context,s->model,seconds,NULL,NULL);
        b->advance(b->context,s->model,0.3f,NULL,NULL);
        s->fade.flags_29c&=~8u;b->pose(b->context);
    }
    if(s->item_2cc!=-1) {
        item=b->item(b->context,s->item_2cc);
        if(item) {
            memset(point,0,sizeof(point));status=b->follow_point(b->context,point);if(status)return status;
            memcpy(item->position,point,sizeof(point));b->move_item(b->context,item,point);
        }
    }
    if(s->model)b->advance(b->context,s->model,dt,s->position,s->basis);
    return RF_OK;
}

static int corpse_link_valid(const rf_corpse_list_link *n)
{return n->next && n->previous && n->next!=n && n->previous!=n && n->next->previous==n && n->previous->next==n;}
void rf_corpse_pool_init(rf_corpse_pool *p)
{
    uint32_t i;if(!p)return;
    for(i=0;i<RF_CORPSE_CAPACITY;i++)p->next[i]=i+1;
    p->next[RF_CORPSE_CAPACITY-1]=UINT32_MAX;
    p->free_head=p->live=p->peak=p->active_mask=0;
}
int rf_corpse_pool_acquire(rf_corpse_pool *p,uint32_t *index)
{
    uint32_t slot;
    if(!p || !index)return RF_RANGE;
    if(p->live==RF_CORPSE_CAPACITY)return RF_NOT_FOUND;
    slot=p->free_head;
    if(p->live>RF_CORPSE_CAPACITY || slot>=RF_CORPSE_CAPACITY || (p->active_mask&(1u<<slot)))return RF_RANGE;
    p->free_head=p->next[slot];p->active_mask|=1u<<slot;++p->live;
    if(p->live>p->peak)p->peak=p->live;
    *index=slot;return RF_OK;
}
int rf_corpse_pool_release(rf_corpse_pool *p,uint32_t index)
{
    if(!p || index>=RF_CORPSE_CAPACITY || !p->live || p->live>RF_CORPSE_CAPACITY || !(p->active_mask&(1u<<index)))return RF_RANGE;
    p->next[index]=p->free_head;p->free_head=index;--p->live;p->active_mask&=~(1u<<index);return RF_OK;
}
static void corpse_link_remove(rf_corpse_list_link *n)
{
    rf_corpse_list_link *next=n->next,*previous=n->previous;
    n->next=n->previous=NULL;previous->next=next;next->previous=previous;
}
int rf_corpse_delete(rf_corpse_delete_state *s,rf_object_registry *registry,
    uint32_t *corpse_count,uint32_t *object_count,uint32_t limit,const rf_corpse_delete_backend *b)
{
    rf_corpse_delete_emitter *e,*next;uint32_t visits=0,*item,handle;int status;
    if(!s || !s->update || !b || !b->effect || !b->item_flags || !corpse_count || !object_count ||
       corpse_count==object_count || !*corpse_count || !*object_count || s->lifecycle || !s->registered_object ||
       !corpse_link_valid(&s->corpse_link) || !corpse_link_valid(&s->object_link))return RF_RANGE;
    if(rf_object_registry_lookup(registry,s->handle)!=s->registered_object)return RF_NOT_FOUND;
    for(e=s->emitters;e;e=e->next) {if(visits==limit)return RF_RANGE;++visits;}
    handle=s->handle;s->lifecycle=1;
    b->effect(b->context,RF_CORPSE_DELETE_PAIRS,handle);
    b->effect(b->context,RF_CORPSE_DELETE_STRING,handle);
    item=b->item_flags(b->context,s->update->item_2cc);
    if(item) {*item|=2u;s->update->item_2cc=-1;}
    if(s->burn)b->effect(b->context,RF_CORPSE_DELETE_BURN,s->burn);
    corpse_link_remove(&s->corpse_link);--*corpse_count;
    b->effect(b->context,RF_CORPSE_DELETE_PHYSICS,handle);
    if(!(s->update->fade.object_flags_7c&0x400u) && s->update->model)
        b->effect(b->context,RF_CORPSE_DELETE_MODEL,s->update->model);
    while((e=s->emitters)!=NULL) {
        next=e->next;b->effect(b->context,RF_CORPSE_DELETE_EMITTER,e->token);s->emitters=next;
    }
    b->effect(b->context,RF_CORPSE_DELETE_OBJECT_STRING,handle);
    corpse_link_remove(&s->object_link);--*object_count;s->lifecycle=2;
    b->effect(b->context,RF_CORPSE_DELETE_RECYCLE,handle);
    status=rf_object_registry_remove(registry,handle);return status;
}

static rf_corpse *corpse_from_link(rf_corpse_list_link *link)
{return (rf_corpse *)((unsigned char *)link-(offsetof(rf_corpse,deletion)+offsetof(rf_corpse_delete_state,corpse_link)));}
int rf_corpse_body_open(const rf_corpse_physics_seed *seed,float elasticity,float friction,
    float density,uint32_t budget,rf_physics_body *result)
{
    if(!seed || (seed->flags!=0x33u && seed->flags!=0x73u))return RF_RANGE;
    return rf_physics_creation_body_open(seed,elasticity,friction,density,budget,result);
}
int rf_corpse_owners_init(rf_corpse_owners *owners,uint32_t budget)
{
    if(!owners || budget<sizeof(*owners))return RF_RANGE;
    memset(owners,0,sizeof(*owners));rf_corpse_pool_init(&owners->pool);
    owners->allocated_bytes=sizeof(*owners);owners->budget=budget;return RF_OK;
}
int rf_corpse_owners_acquire(rf_corpse_owners *owners,const rf_corpse_physics_seed *seed,
    float elasticity,float friction,float density,uint32_t *index)
{
    uint32_t slot,remaining;int status;rf_physics_body *body;
    if(!owners || !index || owners->allocated_bytes<sizeof(*owners) ||
       owners->allocated_bytes>owners->budget)return RF_RANGE;
    remaining=owners->budget-owners->allocated_bytes;
    /* The body record is already accounted for in owners, not another heap allocation. */
    if(remaining>UINT32_MAX-sizeof(rf_physics_body))remaining=UINT32_MAX-sizeof(rf_physics_body);
    status=rf_corpse_pool_acquire(&owners->pool,&slot);if(status)return status;
    body=&owners->slots[slot].body;
    status=rf_corpse_body_open(seed,elasticity,friction,density,remaining+sizeof(*body),body);
    if(status) {rf_corpse_pool_release(&owners->pool,slot);return status;}
    owners->allocated_bytes+=body->spheres.count*sizeof(rf_physics_sphere);
    *index=slot;return RF_OK;
}
int rf_corpse_owners_recycle(rf_corpse_owners *owners,uint32_t index)
{
    rf_physics_body *body;
    if(!owners || index>=RF_CORPSE_CAPACITY || !(owners->pool.active_mask&(1u<<index)))return RF_RANGE;
    if(owners->slots[index].names[0].bytes || owners->slots[index].names[1].bytes)return RF_RANGE;
    body=&owners->slots[index].body;
    owners->allocated_bytes-=body->spheres.count*sizeof(rf_physics_sphere);
    rf_physics_body_close(body);return rf_corpse_pool_release(&owners->pool,index);
}
int rf_corpse_name_assign(rf_corpse_owners *owners,uint32_t index,uint32_t kind,const char *name)
{
    rf_corpse_name *target;size_t length;uint32_t old_bytes,new_bytes;char *copy;
    if(!owners || index>=RF_CORPSE_CAPACITY || kind>=RF_CORPSE_NAME_COUNT ||
       !(owners->pool.active_mask&(1u<<index)))return RF_RANGE;
    target=&owners->slots[index].names[kind];
    if(name && name==target->bytes)return RF_OK;
    length=name?strlen(name):0;if(length>=UINT32_MAX)return RF_RANGE;
    old_bytes=target->bytes?target->length+1:0;new_bytes=length?(uint32_t)length+1:0;
    if((uint64_t)owners->allocated_bytes-old_bytes+new_bytes>owners->budget)return RF_RANGE;
    if(length==target->length) {
        if(target->bytes)memcpy(target->bytes,name,length+1);
        return RF_OK;
    }
    if(target->bytes)free(target->bytes);target->bytes=NULL;target->length=0;owners->allocated_bytes-=old_bytes;
    if(!length)return RF_OK;
    copy=malloc(new_bytes);if(!copy)return RF_RANGE;
    memcpy(copy,name,new_bytes);target->bytes=copy;target->length=(uint32_t)length;
    owners->allocated_bytes+=new_bytes;return RF_OK;
}
typedef struct corpse_owned_delete_context {
    rf_corpse_owners *owners;uint32_t index;const rf_corpse_delete_backend *backend;
} corpse_owned_delete_context;
static void corpse_owned_delete_effect(void *context,uint32_t operation,uint32_t token)
{
    corpse_owned_delete_context *c=context;rf_physics_body *body;
    switch(operation) {
    case RF_CORPSE_DELETE_STRING:
        rf_corpse_name_assign(c->owners,c->index,RF_CORPSE_DEATH_NAME,NULL);break;
    case RF_CORPSE_DELETE_PHYSICS:
        body=&c->owners->slots[c->index].body;
        c->owners->allocated_bytes-=body->spheres.count*sizeof(rf_physics_sphere);
        rf_physics_body_close(body);break;
    case RF_CORPSE_DELETE_OBJECT_STRING:
        rf_corpse_name_assign(c->owners,c->index,RF_CORPSE_OBJECT_NAME,NULL);break;
    case RF_CORPSE_DELETE_RECYCLE:
        rf_corpse_owners_recycle(c->owners,c->index);break;
    default:c->backend->effect(c->backend->context,operation,token);break;
    }
}
static uint32_t *corpse_owned_delete_item(void *context,int32_t id)
{
    corpse_owned_delete_context *c=context;return c->backend->item_flags(c->backend->context,id);
}
int rf_corpse_owned_delete(rf_corpse_owners *owners,uint32_t index,rf_object_registry *registry,
    uint32_t *corpse_count,uint32_t *object_count,uint32_t limit,const rf_corpse_delete_backend *backend)
{
    rf_corpse *corpse;corpse_owned_delete_context context;rf_corpse_delete_backend bridge;
    if(!owners || index>=RF_CORPSE_CAPACITY || !(owners->pool.active_mask&(1u<<index)) ||
       owners->allocated_bytes<sizeof(*owners) || owners->allocated_bytes>owners->budget ||
       !backend || !backend->effect || !backend->item_flags)return RF_RANGE;
    corpse=&owners->slots[index].corpse;
    if(corpse->deletion.registered_object!=corpse || corpse->deletion.update!=&corpse->update)return RF_RANGE;
    context.owners=owners;context.index=index;context.backend=backend;
    bridge.effect=corpse_owned_delete_effect;bridge.item_flags=corpse_owned_delete_item;bridge.context=&context;
    return rf_corpse_delete(&corpse->deletion,registry,corpse_count,object_count,limit,&bridge);
}
int rf_corpse_owned_abort(rf_corpse_owners *owners,uint32_t index,rf_object_registry *registry,
    uint32_t *corpse_count,uint32_t *object_count,uint32_t limit,const rf_corpse_delete_backend *backend)
{
    rf_corpse_owned *owner;rf_corpse *c;rf_corpse_delete_emitter *e,*next;uint32_t stage,visits=0,handle;
    if(!owners || index>=RF_CORPSE_CAPACITY || !(owners->pool.active_mask&(1u<<index)) ||
       !corpse_count || !object_count || corpse_count==object_count || !*object_count ||
       !backend || !backend->effect || owners->allocated_bytes<sizeof(*owners) || owners->allocated_bytes>owners->budget)return RF_RANGE;
    owner=&owners->slots[index];c=&owner->corpse;stage=owner->construction;
    if(stage<RF_CORPSE_CONSTRUCT_ALLOCATED || stage>=RF_CORPSE_CONSTRUCT_COMPLETE || c->deletion.lifecycle ||
       c->deletion.registered_object!=c || c->deletion.update!=&c->update || !corpse_link_valid(&c->deletion.object_link))return RF_RANGE;
    if(rf_object_registry_lookup(registry,c->deletion.handle)!=c)return RF_NOT_FOUND;
    if(stage>=RF_CORPSE_CONSTRUCT_LINKED) {
        if(!*corpse_count || !corpse_link_valid(&c->deletion.corpse_link))return RF_RANGE;
    } else if(c->deletion.corpse_link.next || c->deletion.corpse_link.previous)return RF_RANGE;
    for(e=c->deletion.emitters;e;e=e->next) {if(visits==limit)return RF_RANGE;++visits;}
    handle=c->deletion.handle;c->deletion.lifecycle=1;
    rf_corpse_name_assign(owners,index,RF_CORPSE_DEATH_NAME,NULL);
    if(stage>=RF_CORPSE_CONSTRUCT_TAIL && c->deletion.burn)backend->effect(backend->context,RF_CORPSE_DELETE_BURN,c->deletion.burn);
    if(stage>=RF_CORPSE_CONSTRUCT_LINKED) {corpse_link_remove(&c->deletion.corpse_link);--*corpse_count;}
    owners->allocated_bytes-=owner->body.spheres.count*sizeof(rf_physics_sphere);rf_physics_body_close(&owner->body);
    if(stage>=RF_CORPSE_CONSTRUCT_MODEL && !(c->update.fade.object_flags_7c&0x400u) && c->update.model)
        backend->effect(backend->context,RF_CORPSE_DELETE_MODEL,c->update.model);
    while((e=c->deletion.emitters)!=NULL) {
        next=e->next;backend->effect(backend->context,RF_CORPSE_DELETE_EMITTER,e->token);c->deletion.emitters=next;
    }
    rf_corpse_name_assign(owners,index,RF_CORPSE_OBJECT_NAME,NULL);
    corpse_link_remove(&c->deletion.object_link);--*object_count;c->deletion.lifecycle=2;
    rf_corpse_owners_recycle(owners,index);
    return rf_object_registry_remove(registry,handle);
}
int rf_corpse_base_acquire(rf_corpse_owners *owners,rf_object_registry *registry,
    rf_corpse_list_link *head,uint32_t *object_count,const rf_corpse_physics_seed *seed,
    float elasticity,float friction,float density,uint32_t room,uint32_t *index)
{
    rf_corpse_physics_seed prepared;rf_corpse *c;rf_physics_body *body;uint32_t slot,handle;int status;
    if(!owners || !registry || !head || !object_count || !seed || !index || !isfinite(seed->radius) ||
       !head->next || !head->previous || head->next->previous!=head || head->previous->next!=head ||
       *object_count>=RF_OBJECT_CAPACITY)return RF_RANGE;
    if(!registry->count)return RF_NOT_FOUND;
    prepared=*seed;if(prepared.radius<0)prepared.radius=1;
    status=rf_corpse_owners_acquire(owners,&prepared,elasticity,friction,density,&slot);if(status)return status;
    c=&owners->slots[slot].corpse;body=&owners->slots[slot].body;owners->slots[slot].construction=RF_CORPSE_CONSTRUCT_BASE;
    status=rf_object_registry_insert(registry,c,&handle);
    if(status) {rf_corpse_owners_recycle(owners,slot);return status;}
    c->update.fade.health_34=100;c->update.fade.object_flags_7c=0x6400000u;c->update.model=0;
    memcpy(c->update.position,seed->position,12);memcpy(c->update.basis,seed->basis,36);
    c->model_radius=seed->radius<=0?1:seed->radius;c->physics_radius=body->state.bounds.radius;
    c->physics_flags=body->state.flags;c->word_1fc=0;c->attachment_index=UINT32_MAX;
    c->deletion.update=&c->update;c->deletion.handle=handle;c->deletion.registered_object=c;
    c->deletion.lifecycle=0;c->deletion.emitters=NULL;
    c->deletion.corpse_link.next=c->deletion.corpse_link.previous=NULL;
    c->deletion.object_link.next=head;c->deletion.object_link.previous=head->previous;
    head->previous->next=&c->deletion.object_link;head->previous=&c->deletion.object_link;
    /*48a160 copies object position into the query cache, then assigns room. */
    memcpy(owners->slots[slot].room.query_position,c->update.position,12);
    owners->slots[slot].room.room=room;owners->slots[slot].room.flags=c->update.fade.object_flags_7c;
    ++*object_count;*index=slot;return RF_OK;
}
static int corpse_owner_eligible(const rf_corpse *c)
{return !(c->update.fade.flags_29c&0x43u) && !(c->update.fade.object_flags_7c&0x4000u);}
static int corpse_create_with_name(rf_corpse_create_source *s,const rf_corpse_create_request *r,
    rf_corpse_list_link *head,uint32_t *count,const rf_corpse_create_backend *b,rf_corpse **result,
    int (*assign_name)(void *,rf_corpse *,const char *),void (*progress)(void *,uint32_t),
    int (*bind_model)(void *,rf_corpse_create_source *,rf_corpse *))
{
    rf_corpse_physics_seed seed;rf_corpse *c,*oldest,*candidate;rf_corpse_list_link *n,*previous;
    rf_corpse_delete_emitter *emitter;uint32_t visits=0,eligible=0;int32_t motion,offset=0;double delay;int status;
    if(!result)return RF_RANGE;
    *result=NULL;if(!s)return RF_NOT_FOUND;
    if(!r || !head || !count || !b || !b->allocate || !b->load_model || !b->motion || !b->effect || !b->emitter ||
       !r->death_name || !head->next || !head->previous || !isfinite(r->created_seconds) || *count>RF_CORPSE_CAPACITY ||
       s->sphere_count>r->sphere_capacity || s->sphere_count>UINT32_MAX/sizeof(*s->spheres) ||
       (s->sphere_count && (!s->spheres || !r->sphere_scratch)))return RF_RANGE;
    previous=head;
    for(n=head->next;n!=head;n=n->next) {
        if(!n || visits==RF_CORPSE_CAPACITY || n->previous!=previous || !isfinite(corpse_from_link(n)->created_seconds))return RF_RANGE;
        previous=n;++visits;
    }
    if(visits!=*count || head->previous!=previous)return RF_RANGE;
    if(s->emitter_kind>0) {
        delay=(double)s->emitter_lifetime*1000.0+0.5;
        if(!isfinite(delay) || delay<-(double)RF_TIMER_PERIOD || delay>(double)RF_TIMER_PERIOD)return RF_RANGE;
        offset=(int32_t)delay;status=rf_timer_set(&motion,r->now_ms,offset);if(status)return status;
    }
    if(!s->replacement_model || !s->replacement_model[0])s->object_flags|=0x400u;
    s->object_flags|=2u;
    if(*count==RF_CORPSE_CAPACITY)return RF_NOT_FOUND;
    memset(&seed,0,sizeof(seed));seed.word_0c=s->word_8c;seed.word_14=s->word_98;
    memcpy(seed.position,r->position,sizeof(seed.position));memcpy(seed.basis,r->basis,sizeof(seed.basis));
    seed.radius=s->physics_radius;seed.flags=(s->class_flags_724&0x80000u)?0x73u:0x33u;
    if(s->sphere_count)memmove(r->sphere_scratch,s->spheres,s->sphere_count*sizeof(*s->spheres));
    seed.spheres=r->sphere_scratch;seed.sphere_count=s->sphere_count;
    c=b->allocate(b->context,s,&seed);if(!c)return RF_NOT_FOUND;
    *result=c;
    c->deletion.update=&c->update;c->deletion.lifecycle=0;
    c->attachment_index=s->attachment_index;
    if(s->flags_814&2u)c->presentation[1]=0;
    else b->effect(b->context,RF_CORPSE_CREATE_SNAPSHOT,s,c,NULL);
    c->update.fade.flags_29c=(s->class_flags_724&0x20000u)?0x80u:0;
    if(s->class_flags_728&0x20u)c->update.fade.flags_29c|=0x400u;
    c->uid=s->uid;c->weapon=s->weapon;c->update.motion_2b8=-1;c->word_2d8=s->word_2d8;
    c->update.model=(s->replacement_model && s->replacement_model[0])?b->load_model(b->context,s->replacement_model):s->model;
    if(progress)progress(b->context,RF_CORPSE_CONSTRUCT_MODEL);
    if(bind_model){status=bind_model(b->context,s,c);if(status)return status;}
    if(s->model && s->model_kind==2) {
        motion=b->motion(b->context,s,r->death_name);
        if(motion<-1 || motion>=45)return RF_RANGE;
        if(motion!=-1 && !(s->class_flags_724&0x200000u)) {
            c->update.motion_2b8=s->motions[motion];if(r->seek_motion==1)c->update.fade.flags_29c|=8u;
            b->effect(b->context,RF_CORPSE_CREATE_POSE,s,c,NULL);
        }
    }
    if((s->class_flags_724&0x200000u) && s->motion_a44!=-1) {
        b->effect(b->context,RF_CORPSE_CREATE_PLAY,s,c,NULL);c->update.fade.flags_29c|=4u;
    }
    c->model_radius=c->physics_radius;c->word_1fc=s->word_1fc;
    c->created_seconds=r->created_seconds;
    c->class_index=s->class_index;c->update.fade.health_34=s->class_health;
    c->deletion.burn=0;c->word_2d4=-1;memset(c->velocity,0,sizeof(c->velocity));memset(c->vector_150,0,sizeof(c->vector_150));
    if(progress)progress(b->context,RF_CORPSE_CONSTRUCT_TAIL);
    if(assign_name) {status=assign_name(b->context,c,r->death_name);if(status)return status;}
    else b->effect(b->context,RF_CORPSE_CREATE_NAME,s,c,r->death_name);
    rf_timer_clear(&c->update.emitter_deadline_2ac);
    if(s->emitter_kind>=0) {
        emitter=b->emitter(b->context,s,c);
        if(emitter){emitter->next=c->deletion.emitters;c->deletion.emitters=emitter;}
        if(s->emitter_kind>0){status=rf_timer_set(&c->update.emitter_deadline_2ac,r->now_ms,offset);if(status)return status;}
    }
    if(s->flags_814&8u)c->update.fade.flags_29c|=2u;
    if(r->protected_body==1)c->update.fade.flags_29c|=0x40u;
    c->deletion.corpse_link.previous=head->previous;c->deletion.corpse_link.next=head;
    head->previous->next=&c->deletion.corpse_link;head->previous=&c->deletion.corpse_link;++*count;
    if(progress)progress(b->context,RF_CORPSE_CONSTRUCT_LINKED);
    for(n=head->next;n!=head;n=n->next)if(corpse_owner_eligible(corpse_from_link(n)))++eligible;
    while(eligible>5) {
        oldest=NULL;
        for(n=head->next;n!=head;n=n->next) {
            candidate=corpse_from_link(n);
            if(corpse_owner_eligible(candidate) && (!oldest || candidate->created_seconds<oldest->created_seconds))oldest=candidate;
        }
        oldest->update.fade.fade_298=1;oldest->update.fade.flags_29c|=1u;--eligible;
    }
    c->update.value_2b0=s->class_value;c->update.class_value=s->class_value;
    motion=b->motion(b->context,s,"corpse_drop");if(motion<-1 || motion>=45)return RF_RANGE;
    c->drop_motion=motion==-1?-1:s->motions[motion];
    motion=b->motion(b->context,s,"corpse_carry");if(motion<-1 || motion>=45)return RF_RANGE;
    c->carry_motion=motion==-1?-1:s->motions[motion];
    c->direction=(strstr(r->death_name,"forward") || strstr(r->death_name,"front"))?0:strstr(r->death_name,"back")?1:2;
    c->extra_model=0;if((s->flags_810&0x200000u) && s->extra_model){c->extra_model=s->extra_model;s->extra_model=0;}
    if((c->physics_flags&0x20u) && !(c->update.fade.object_flags_7c&0x8000u))b->effect(b->context,RF_CORPSE_CREATE_COLLISION,s,c,NULL);
    b->effect(b->context,RF_CORPSE_CREATE_SOURCE_EFFECTS,s,c,NULL);
    if(progress)progress(b->context,RF_CORPSE_CONSTRUCT_COMPLETE);
    *result=c;return RF_OK;
}

int rf_corpse_create(rf_corpse_create_source *s,const rf_corpse_create_request *r,
    rf_corpse_list_link *head,uint32_t *count,const rf_corpse_create_backend *backend,rf_corpse **result)
{return corpse_create_with_name(s,r,head,count,backend,result,NULL,NULL,NULL);}

typedef struct corpse_owned_create_context {
    const rf_corpse_create_ownership *ownership;const rf_corpse_create_backend *backend;
    uint32_t index;int allocation_status;
    int (*bind_model)(void *,rf_corpse_create_source *,rf_corpse *);void *model_context;
} corpse_owned_create_context;
static rf_corpse *corpse_owned_allocate(void *context,rf_corpse_create_source *source,const rf_corpse_physics_seed *seed)
{
    corpse_owned_create_context *c=context;const rf_corpse_create_ownership *o=c->ownership;(void)source;
    c->allocation_status=rf_corpse_base_acquire(o->owners,o->registry,o->object_head,o->object_count,seed,
        o->elasticity,o->friction,o->density,o->room,&c->index);
    if(c->allocation_status)return NULL;
    o->owners->slots[c->index].construction=RF_CORPSE_CONSTRUCT_ALLOCATED;
    return &o->owners->slots[c->index].corpse;
}
static uint32_t corpse_owned_load_model(void *context,const char *name)
{corpse_owned_create_context *c=context;return c->backend->load_model(c->backend->context,name);}
static int32_t corpse_owned_motion(void *context,rf_corpse_create_source *source,const char *name)
{corpse_owned_create_context *c=context;return c->backend->motion(c->backend->context,source,name);}
static void corpse_owned_create_effect(void *context,uint32_t operation,rf_corpse_create_source *source,rf_corpse *corpse,const char *name)
{corpse_owned_create_context *c=context;c->backend->effect(c->backend->context,operation,source,corpse,name);}
static rf_corpse_delete_emitter *corpse_owned_emitter(void *context,rf_corpse_create_source *source,rf_corpse *corpse)
{corpse_owned_create_context *c=context;return c->backend->emitter(c->backend->context,source,corpse);}
static void corpse_owned_progress(void *context,uint32_t stage)
{corpse_owned_create_context *c=context;c->ownership->owners->slots[c->index].construction=stage;}
static int corpse_owned_assign_name(void *context,rf_corpse *corpse,const char *name)
{
    corpse_owned_create_context *c=context;(void)corpse;
    return rf_corpse_name_assign(c->ownership->owners,c->index,RF_CORPSE_DEATH_NAME,name);
}
static int corpse_owned_bind_model(void *context,rf_corpse_create_source *source,rf_corpse *corpse)
{
    corpse_owned_create_context *c=context;
    return c->bind_model?c->bind_model(c->model_context,source,corpse):RF_OK;
}
int rf_corpse_owned_create_bound(const rf_corpse_create_ownership *o,rf_corpse_create_source *source,
    const rf_corpse_create_request *request,rf_corpse_list_link *head,uint32_t *count,
    const rf_corpse_create_backend *backend,rf_corpse **result,
    int (*bind_model)(void *,rf_corpse_create_source *,rf_corpse *),void *model_context)
{
    corpse_owned_create_context context;rf_corpse_create_backend bridge;int status;
    if(!result)return RF_RANGE;*result=NULL;if(!source)return RF_NOT_FOUND;
    if(!o || !o->owners || !o->registry || !o->object_head || !o->object_count ||
       !backend || !backend->load_model || !backend->motion || !backend->effect || !backend->emitter ||
       o->object_head==head || o->object_count==count)return RF_RANGE;
    context.bind_model=bind_model;context.model_context=model_context;
    context.ownership=o;context.backend=backend;context.index=0;context.allocation_status=0;
    bridge.allocate=corpse_owned_allocate;bridge.load_model=corpse_owned_load_model;bridge.motion=corpse_owned_motion;
    bridge.effect=corpse_owned_create_effect;bridge.emitter=corpse_owned_emitter;bridge.context=&context;
    status=corpse_create_with_name(source,request,head,count,&bridge,result,corpse_owned_assign_name,corpse_owned_progress,corpse_owned_bind_model);
    return context.allocation_status?context.allocation_status:status;
}

int rf_corpse_owned_create(const rf_corpse_create_ownership *o,rf_corpse_create_source *source,
    const rf_corpse_create_request *request,rf_corpse_list_link *head,uint32_t *count,
    const rf_corpse_create_backend *backend,rf_corpse **result)
{return rf_corpse_owned_create_bound(o,source,request,head,count,backend,result,NULL,NULL);}

int32_t rf_entity_action_name_lookup(uint32_t model,uint32_t model_kind,const char *const names[45],const char *query)
{
    uint32_t i;
    if(!query || !model || model_kind!=2 || !names)return -1;
    for(i=0;i<45;++i)if(names[i]) {
        const unsigned char *a=(const unsigned char*)names[i],*b=(const unsigned char*)query;
        for(;;++a,++b) {
            unsigned char x=*a,y=*b;
            if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;
            if(x!=y)break;if(!x)return (int32_t)i;
        }
    }
    return -1;
}

int rf_entity_dying_update(rf_entity_dying_state *s,const rf_entity_dying_backend *b)
{
    uint32_t finish=0,token,i,gain;float end[3],offset,radius;
    if(!s || !b || !b->call || !b->segment)return RF_RANGE;
    if(s->flags_810&0x80u) {
        finish=1; /* Original42e3c0 is a bare return. */
        if(s->burn_13d8) {
            b->call(b->context,RF_DYING_RELEASE_BURN,s->burn_13d8,0);s->burn_13d8=0;
        }
    } else if(s->action_824==-1 || !(b->call(b->context,RF_DYING_ACTION_ACTIVE,(uint32_t)s->action_824,0)&255u))finish=1;
    if((b->call(b->context,RF_DYING_WEAPON_ACTIVE,s->handle,(uint32_t)s->primary_weapon)&255u)==1)
        b->call(b->context,RF_DYING_RESET_WEAPON,s->handle,(uint32_t)s->primary_weapon);
    if((s->class_flags_728&0x20u) && b->player && (b->call(b->context,RF_DYING_TIMER,0,0)&255u)==1) {
        if(!isfinite(s->model_radius_78))return RF_RANGE;
        for(i=0;i<3;i++) {
            if(!isfinite(s->position[i]) || !isfinite(s->forward[i]) || !isfinite(b->player->position[i]))return RF_RANGE;
            offset=(float)((double)s->forward[i]*s->model_radius_78);
            end[i]=(float)((double)s->position[i]+offset);if(!isfinite(end[i]))return RF_RANGE;
        }
        radius=s->model_radius_78>6.0f?2.5f:1.5f;
        gain=s->model_radius_78>6.0f?0x3fa00000u:0x3f800000u;
        if((b->segment(b->context,s->position,end,b->player->position,radius)&255u)==1)
            b->call(b->context,RF_DYING_DAMAGE,b->player->handle,s->handle);
        b->call(b->context,RF_DYING_SHAKE,b->player->camera,gain);
    }
    if(finish) {
        b->call(b->context,RF_DYING_FINALIZE,0,0);
        if(b->call(b->context,RF_DYING_ENDGAME_NAME,0,0)&255u) {
            token=b->call(b->context,RF_DYING_LOOKUP_A,0x118a,0);
            if(token)b->call(b->context,RF_DYING_ACTIVATE_A,token,0);
            token=b->call(b->context,RF_DYING_LOOKUP_B,0x47c3,0);
            if(token)b->call(b->context,RF_DYING_ACTIVATE_B,token,0);
        }
    }
    return RF_OK;
}

rf_corpse *rf_entity_finalize_create_owned_bound(void *context,rf_entity_finalize_state *source,const char *name,
    int (*bind_model)(void *,rf_corpse_create_source *,rf_corpse *),void *model_context)
{
    rf_entity_finalize_corpse_binding *b=context;rf_corpse_create_request request;rf_corpse *result=NULL;uint32_t i;
    if(!b)return NULL;
    if(b->partial){b->status=RF_RANGE;return NULL;}
    b->status=RF_RANGE;b->cleanup_status=0;
    if(!source || !b->source || b->source->handle!=source->handle || !b->ownership ||
       !b->ownership->owners || !b->ownership->registry || !b->ownership->object_count ||
       !b->head || !b->count || !b->create || !b->destroy || !b->destroy->effect || !b->destroy->item_flags)return NULL;
    request=b->request;request.death_name=name;memcpy(request.position,source->position,12);memcpy(request.basis,source->basis,36);
    request.protected_body=0;request.seek_motion=0;
    b->source->object_flags=source->object_flags;b->source->flags_810=source->flags_810;b->source->replacement_model=source->replacement_model;
    b->status=rf_corpse_owned_create_bound(b->ownership,b->source,&request,b->head,b->count,b->create,&result,bind_model,model_context);
    source->object_flags=b->source->object_flags;source->flags_810=b->source->flags_810;
    if(!b->status)return result;
    if(result) {
        b->partial=result;b->cleanup_status=RF_RANGE;
        for(i=0;i<RF_CORPSE_CAPACITY;++i)if(&b->ownership->owners->slots[i].corpse==result) {
            b->cleanup_status=rf_corpse_owned_abort(b->ownership->owners,i,b->ownership->registry,b->count,
                b->ownership->object_count,b->visit_limit,b->destroy);break;
        }
        if(!b->cleanup_status)b->partial=NULL;
    }
    return NULL;
}

rf_corpse *rf_entity_finalize_create_owned(void *context,rf_entity_finalize_state *source,const char *name)
{return rf_entity_finalize_create_owned_bound(context,source,name,NULL,NULL);}

static void finalize_cross(const float a[3],const float b[3],float result[3])
{
    result[0]=(float)((double)a[1]*b[2]-(double)a[2]*b[1]);
    result[1]=(float)((double)a[2]*b[0]-(double)a[0]*b[2]);
    result[2]=(float)((double)a[0]*b[1]-(double)a[1]*b[0]);
}
int rf_entity_finalize_sp(rf_entity_finalize_state *s,const rf_entity_finalize_backend *b)
{
    uint32_t i,h,token,transferred=0,allow=0;rf_entity_finalize_link *link;rf_corpse *corpse;
    rf_entity_finalize_hit hit={0};char name[64]={0};const char *selected;float start[3],end[3],basis[9];double length,area;
    if(!s || !b || !b->call || !b->actor || !b->action_name || !b->region_flags || !b->probe || !b->face_area || !b->create)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(s->position[i]))return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(s->basis[i]))return RF_RANGE;
    s->object_flags|=2;
    if(s->class_kind==1)for(i=0;(int32_t)i<(int32_t)b->call(b->context,RF_FINAL_PLAYER_COUNT,0,0);++i) {
        h=b->call(b->context,RF_FINAL_PLAYER_HANDLE,i,0);link=b->actor(b->context,h);
        if(link && link->parent==s->handle)b->call(b->context,RF_FINAL_DAMAGE_CHILD,link->handle,0);
    }
    if(s->flags_7d0&0x100000u) {
        link=b->actor(b->context,s->parent);if(link)b->call(b->context,RF_FINAL_DAMAGE_PARENT,link->handle,0);
    }
    if(b->actor(b->context,s->parent))b->call(b->context,RF_FINAL_DETACH_PARENT,s->handle,0);
    for(i=0;(int32_t)i<(int32_t)b->call(b->context,RF_FINAL_CHILD_COUNT,0,0);++i) {
        h=b->call(b->context,RF_FINAL_CHILD_HANDLE,i,0);link=b->actor(b->context,h);
        if(link && (link->flags_7d0&0x100000u))link->health=0;
        h=b->call(b->context,RF_FINAL_CHILD_HANDLE,i,0);b->call(b->context,RF_FINAL_DETACH_CHILD,h,1);
    }
    token=b->call(b->context,RF_FINAL_PLAYER_LOOKUP,s->handle,0);
    if(token)b->call(b->context,RF_FINAL_PLAYER_DETACH,token,0);
    if(s->death_effect!=-1){s->action=-1;b->call(b->context,RF_FINAL_EXPLODE,s->handle,0);}
    if(!(s->flags_810&0x80u) && (s->action!=-1 || (s->replacement_model && *s->replacement_model))) {
        if(s->action!=-1) {
            selected=b->action_name(b->context,s->action);
            if(selected) {for(i=0;i<sizeof(name) && selected[i];++i)name[i]=selected[i];if(i==sizeof(name))return RF_RANGE;}
        }
        if(!(b->region_flags(b->context,s->position)&2u)) {
            if(s->movement_kind==10)allow=1;
            else {
                memcpy(start,s->position,12);memcpy(end,s->position,12);
                start[1]=(float)((double)start[1]+.5);end[1]=(float)((double)end[1]-1.5);
                if(!isfinite(start[1]) || !isfinite(end[1]))return RF_RANGE;
                hit.fraction=1;hit.handle=UINT32_MAX;hit.word_38=0;b->probe(b->context,start,end,&hit);
                if(!isfinite(hit.fraction))return RF_RANGE;
                if(hit.fraction>=1)allow=1;
                else if(!b->call(b->context,RF_FINAL_OBJECT_LOOKUP,hit.handle,0)) {
                    allow=1;if(!isfinite(hit.normal[1]))return RF_RANGE;
                    if(hit.normal[1]>.5f && hit.face) {
                        area=b->face_area(b->context,hit.face);if(!isfinite(area))return RF_RANGE;
                        if(area>1) {
                            for(i=0;i<3;++i)if(!isfinite(hit.normal[i]))return RF_RANGE;
                            finalize_cross(hit.normal,s->basis+6,basis);
                            length=sqrt(((double)basis[0]*basis[0]+(double)basis[1]*basis[1])+(double)basis[2]*basis[2]);
                            if(!isfinite(length))return RF_RANGE;
                            if(length<=0){basis[0]=1;basis[1]=basis[2]=0;}
                            else {length=1.0/length;for(i=0;i<3;++i)basis[i]=(float)(length*basis[i]);}
                            finalize_cross(basis,hit.normal,basis+6);finalize_cross(basis+6,basis,basis+3);
                            for(i=0;i<9;++i)if(!isfinite(basis[i]))return RF_RANGE;
                            memcpy(s->basis,basis,sizeof(basis));s->support=hit;
                        }
                    }
                }
            }
        }
        if(allow) {
            corpse=b->create(b->context,s,name);
            if(corpse && (s->flags_810&0x4000000u)) {
                b->call(b->context,RF_FINAL_DROP,corpse->deletion.handle,0);s->flags_810&=~0x200u;
            }
            if(s->burn && corpse) {
                b->call(b->context,RF_FINAL_RETARGET_BURN,s->burn,corpse->deletion.handle);
                corpse->deletion.burn=s->burn;transferred=1;
            }
        }
    }
    b->call(b->context,RF_FINAL_TAIL_PREDICATE,s->handle,0);
    if(!transferred && s->burn){b->call(b->context,RF_FINAL_RELEASE_BURN,s->burn,0);s->burn=0;}
    return RF_OK;
}

int rf_entity_death_select(const rf_entity_death_selection *state,
    uint32_t (*clearance)(void *context,uint32_t direction),void *context,
    rf_random_state *random,int32_t *result)
{
    int32_t action;uint32_t draw;
    if(!state || !clearance || !random || !result ||
       state->action_824 < -1 || state->action_824>=45)return RF_RANGE;
    action=state->action_824;
    if((state->flags_810&0x400u) || state->current_138c==13 || state->next_1390==13)action=16;
    else {
        if(action==6 || action==8 || action==11) {
            if(!(clearance(context,1)&255u))action=-1;
        } else if(action==7 || action==9 || action==12) {
            if(!(clearance(context,0)&255u))action=-1;
        }
        if(action==12) {
            rf_random_next(random,&draw);
            if(!(draw%2u))action=-1;
        }
        if(action==-1) {
            static const int32_t choices[3]={5,14,15};
            rf_random_next(random,&draw);action=choices[draw%3u];
        }
    }
    if(state->motions[action]==-1)action=state->motions[5]!=-1?5:-1;
    *result=action;
    return RF_OK;
}

uint32_t rf_entity_death_clearance(const rf_entity_death_clearance_state *s,
    uint32_t direction,const rf_entity_death_obstacle *actors,uint32_t count,
    uint32_t (*ray)(void *context,const float start[3],const float end[3]),void *context)
{
    float length,offset[3],start[3],end[3],delta[3],reach,local_x,local_z;
    uint32_t i,j;double distance;
    direction&=255u;
    length=(float)((double)s->extent_180*(direction==1?3.0:-3.0));
    for(j=0;j<3;j++) {offset[j]=(float)((double)s->matrix[2][j]*length);end[j]=(float)((double)s->position[j]+offset[j]);}
    if((ray(context,s->position,end)&255u)==1)return 0;
    memcpy(start,s->position,sizeof(start));start[1]=(float)((double)start[1]-(double)s->model_radius_78*.5);
    for(j=0;j<3;j++)end[j]=(float)((double)start[j]+offset[j]);
    if((ray(context,start,end)&255u)==1)return 0;
    for(j=0;j<3;j++) {float half=(float)((double)offset[j]*.5);start[j]=(float)((double)s->position[j]+half);}
    memcpy(end,start,sizeof(end));end[1]=(float)((double)end[1]-((double)s->model_radius_78+1.0));
    if(!(ray(context,start,end)&255u))return 0;
    for(j=0;j<3;j++)start[j]=(float)((double)s->position[j]+offset[j]);
    memcpy(end,start,sizeof(end));end[1]=(float)((double)end[1]-((double)s->model_radius_78+1.0));
    if(!(ray(context,start,end)&255u))return 0;
    for(i=0;i<count;i++) {
        const rf_entity_death_obstacle *a=actors+i;
        if(!(a->class_word_74&4u))continue;
        reach=(float)(fabs((double)length)+(double)a->extent_180);
        for(j=0;j<3;j++)delta[j]=(float)((double)a->position[j]-s->position[j]);
        distance=((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2];
        if(distance>(double)reach*reach)continue;
        local_z=(float)(((double)delta[2]*s->matrix[2][2]+(double)delta[1]*s->matrix[2][1])+(double)delta[0]*s->matrix[2][0]);
        if((direction==1 && local_z<0) || (direction==0 && local_z>0))continue;
        local_x=(float)(((double)delta[2]*s->matrix[0][2]+(double)delta[1]*s->matrix[0][1])+(double)delta[0]*s->matrix[0][0]);
        if(fabs((double)local_x)<fabs((double)local_z))return 0;
    }
    return 1;
}

int rf_entity_death_link_sp(const rf_entity_death_link_source *s,const rf_entity_death_link_backend *b)
{
    rf_entity_death_link_actor *actor,*player,*parent;uint32_t visited=0;
    if(!s || !b || !b->resolve || !b->call || !b->local_player || !b->head)return RF_RANGE;
    if(s->linked_146c==UINT32_MAX)return RF_OK;
    actor=b->resolve(b->context,s->linked_146c);if(!actor)return RF_OK;
    if(b->call(b->context,RF_DEATH_LINK_SKIP,actor,0)&255u)return RF_OK;
    memcpy(actor->base_position,s->position,12);memcpy(actor->base_basis,s->basis,36);
    memcpy(actor->position,s->position,12);memcpy(actor->basis,s->basis,36);
    b->call(b->context,RF_DEATH_LINK_UNLINK,actor,0);
    b->call(b->context,RF_DEATH_LINK_QUERY,actor,0);
    b->call(b->context,RF_DEATH_LINK_REFRESH,actor,0);
    player=*b->local_player;
    if(player) {
        b->call(b->context,RF_DEATH_LINK_OWNER,actor,player->handle);
        b->call(b->context,RF_DEATH_LINK_INVENTORY,actor,0);
    }
    if(b->call(b->context,RF_DEATH_LINK_PLAYER,*b->local_player,0)&255u) {
        player=*b->local_player;if(!player)return RF_RANGE;
        parent=b->resolve(b->context,player->parent_200);
        if(parent) {
            parent->flags_814|=0x800u;
            b->call(b->context,RF_DEATH_LINK_DETACH,*b->local_player,0);
            parent->word_34=0;
        }
    }
    for(actor=*b->head;actor;actor=actor->next) {
        if(visited++>=b->capacity)return RF_RANGE;
        if(b->call(b->context,RF_DEATH_LINK_LIST_PREDICATE,actor,0)&255u)actor->word_34=0;
    }
    return RF_OK;
}

int rf_entity_death_tail_sp(rf_entity_death_tail_state *s,const rf_entity_death_tail_backend *b)
{
    int status;
    if(!s || !b || !b->call || !b->now_ms)return RF_RANGE;
    if(s->action_520==13)b->call(b->context,RF_DEATH_TAIL_INVENTORY,0);
    if(s->class_flags_728&0x20u) {
        status=rf_timer_set(&s->deadline_4b8,*b->now_ms,s->radius_78>6.0f?2000:1600);
        if(status)return status;
    }
    b->call(b->context,RF_DEATH_TAIL_RESET,0);
    if(s->flags_810&0x400000u)b->call(b->context,RF_DEATH_TAIL_EVENT,s->name);
    if(s->model_148c) {
        b->call(b->context,RF_DEATH_TAIL_RELEASE,s->model_148c);
        s->model_148c=0;
    }
    return RF_OK;
}

int rf_entity_death_early_sp(rf_entity_death_early_state *s,const rf_entity_death_early_backend *b)
{
    rf_entity_death_player_view *player;uint32_t active;int status;
    if(!s || !b || !b->call)return RF_RANGE;
    b->call(b->context,RF_DEATH_EARLY_COLLISION,s->actor);
    if(s->actor==s->local_actor) {
        player=s->player;
        if(player) {
            active=b->call(b->context,RF_DEATH_EARLY_MODE,player->token);
            player=s->player;
            if(active&255u) {
                b->call(b->context,RF_DEATH_EARLY_STOP,player?player->token:0);
                player=s->player;
            }
        }
        if(s->actor==s->local_actor && player)player->field_fb0=0;
    }
    status=rf_timer_set(&s->deadline_62fd48,s->now_ms,1500);if(status)return status;
    return rf_timer_set(&s->deadline_62fd44,s->now_ms,750);
}

_Static_assert(sizeof(rf_entity_navigation_route)==0x174,"navigation route prefix");
_Static_assert(offsetof(rf_entity_navigation_route,timer_11c)==0x11c,"route timer");
_Static_assert(offsetof(rf_entity_navigation_route,flag_170)==0x170,"route flag");
int rf_entity_navigation_reset(rf_entity_navigation_route *route,int32_t now_ms)
{
    int32_t deadline;int status;if(!route)return RF_RANGE;
    status=rf_timer_set(&deadline,now_ms,0);if(status)return status;
    route->word_000=0;route->word_128=0;route->word_018=-1;route->word_014=-1;
    route->word_12c=0;route->word_144=-1;route->timer_11c=deadline;route->timer_134=deadline;
    memset(route->vector_138,0,sizeof(route->vector_138));route->flag_15c=1;route->flag_170=0;route->word_160=0;
    return RF_OK;
}

uint32_t rf_entity_navigation_candidate_allowed(float radius,float height,uint32_t mode,
    float candidate_radius,float candidate_height,uint32_t word_40)
{
    return !(radius>candidate_radius) && !(height>candidate_height) &&
        ((mode&255u)!=1u || word_40==0);
}

_Static_assert(sizeof(rf_entity_navigation_candidate)==0x44,"navigation candidate");
_Static_assert(offsetof(rf_entity_navigation_candidate,distance_squared)==0x38,"navigation score");
static double navigation_distance_squared(const float a[3],const float b[3])
{
    volatile float x=(float)((double)a[0]-b[0]),y=(float)((double)a[1]-b[1]),z=(float)((double)a[2]-b[2]);
    return ((double)x*x+(double)y*y)+(double)z*z;
}
int rf_entity_navigation_single(const float position[3],float radius,float height,
    uint32_t mode,rf_entity_navigation_candidate *candidate,uint32_t *classification)
{
    uint32_t i,result=2;volatile float half,cr2,r2;float flat[3];double distance;
    if(!position || !candidate || !classification)return RF_RANGE;
    if(!isfinite(radius) || !isfinite(height) || !isfinite(candidate->radius) || !isfinite(candidate->height))return RF_FORMAT;
    for(i=0;i<3;++i)if(!isfinite(position[i]) || !isfinite(candidate->position[i]))return RF_FORMAT;
    half=(float)((double)candidate->height*.5);
    memcpy(candidate->query_point,candidate->position,12);
    if(mode&255u)candidate->query_point[1]=(float)(((double)candidate->position[1]-half)+(double)height*.5);
    candidate->distance_squared=(float)navigation_distance_squared(position,candidate->query_point);
    if((double)half+candidate->position[1]>=position[1] &&
        (double)candidate->position[1]-half<=position[1]) {
        cr2=(float)((double)candidate->radius*candidate->radius);r2=(float)((double)radius*radius);
        memcpy(flat,candidate->position,12);flat[1]=position[1];distance=navigation_distance_squared(flat,position);
        if(!(distance>=(double)cr2-r2))result=0;
        else if(!(distance>=(double)r2+cr2))result=1;
    }
    *classification=result;return RF_OK;
}

int rf_entity_navigation_closest_point(const float point[3],const float start[3],
    const float end[3],float closest[3],float *distance_along)
{
    float delta[3],direction[3],relative[3],value[3];volatile float length,inverse,along,scaled;uint32_t i;
    if(!point || !start || !end || !closest || !distance_along)return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(point[i]) || !isfinite(start[i]) || !isfinite(end[i]))return RF_FORMAT;
        delta[i]=(float)((double)end[i]-start[i]);if(!isfinite(delta[i]))return RF_FORMAT;
    }
    length=(float)sqrt(((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2]);
    if(!isfinite(length))return RF_FORMAT;
    if(length==0){memcpy(closest,start,12);*distance_along=0;return RF_OK;}
    inverse=(float)(1.0/length);if(!isfinite(inverse))return RF_FORMAT;
    for(i=0;i<3;++i){direction[i]=(float)((double)delta[i]*inverse);relative[i]=(float)((double)point[i]-start[i]);if(!isfinite(relative[i]))return RF_FORMAT;}
    along=(float)(((double)relative[2]*direction[2]+(double)relative[1]*direction[1])+(double)relative[0]*direction[0]);
    if(!isfinite(along))return RF_FORMAT;if(along<0)along=0;if(along>length)along=length;
    for(i=0;i<3;++i){scaled=(float)((double)direction[i]*along);value[i]=(float)((double)start[i]+scaled);if(!isfinite(value[i]))return RF_FORMAT;}
    memcpy(closest,value,12);*distance_along=along;return RF_OK;
}

int rf_entity_ai_destination_prepare(rf_entity_ai_destination_actor *a,rf_entity_ai_destination_query *q,
    uint32_t world,int (*has_weapon)(void *,rf_entity_ai_destination_actor *,uint32_t *),void *context)
{
    uint32_t weapon,special;int32_t kind;int status;
    if(!a || !q || !has_weapon)return RF_RANGE;
    memcpy(&q->radius_630,&a->radius_7c0,4);memcpy(&q->offset_634,&a->height_7c4,4);
    status=has_weapon(context,a,&weapon);if(status)return status;q->weapon_639=weapon&255u;
    kind=a->movement_kind;special=kind==12 || kind==15 || kind==13 || kind==11 || kind==9 || kind==4 || kind==7;
    a->word_5e4=special;q->mode_638=!special;
    memcpy(a->begin_5a4,a->position_3c,12);memcpy(a->next_5b0,a->position_3c,12);
    q->route_slot_650=&a->first_58c;q->start_618=a->begin_5a4;q->limit_640=0;
    q->token_61c=a->token_69c;q->token_620=a->token_6a0;q->word_644=0;q->world_63c=world;
    return RF_OK;
}
static double ai_destination_distance(const float a[3],const float b[3])
{
    float d[3];uint32_t i;for(i=0;i<3;++i)d[i]=(float)((double)a[i]-b[i]);
    return sqrt(((double)d[0]*d[0]+(double)d[1]*d[1])+(double)d[2]*d[2]);
}
static int ai_destination_finish(rf_entity_ai_destination_actor *a,int32_t now,const float previous[3],uint32_t *result)
{
    int status;a->word_59c=0;a->word_5a0=1;
    status=rf_timer_set(&a->timer_6bc,now,0);if(status)return status;
    memcpy(a->previous_6d4,previous,12);*result=1;return RF_OK;
}
int rf_entity_ai_destination(uint32_t handle,const float point[3],int32_t now,
    rf_entity_ai_destination_query *q,const rf_entity_ai_destination_backend *b,uint32_t *result)
{
    rf_entity_ai_destination_actor *a,*target=NULL;double limit,length,ratio,inverse;float stored_limit,along,closest[3],d[3],scaled,radius,h1,h2;
    uint32_t value,i;int status;
    if(!point || !q || !b || !result || !b->lookup || !b->reset || !b->prepare || !b->limit || !b->select ||
       !b->clear || !b->add || !b->direct || !b->search || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
#define DEST_CALL(expr) do {status=(expr);if(status)return status;} while(0)
    DEST_CALL(b->lookup(b->context,handle,&a));if(!a){*result=0;return RF_OK;}
    DEST_CALL(b->reset(b->context,a));
    for(i=0;i<3;++i)if(!isfinite(point[i]))return RF_FORMAT;
    memmove(a->requested_620,point,12);memmove(a->adjusted_62c,point,12);q->destination_owner=a;
    if(a->action_520==3 && a->state_554==3) {
        a->count_588=2;a->first_58c=a->begin_5a4;a->last_590=a->requested_620;
        memcpy(a->begin_5a4,a->position_3c,12);memcpy(a->next_5b0,a->position_3c,12);
        return ai_destination_finish(a,now,a->position_3c,result);
    }
    DEST_CALL(b->prepare(b->context,a,q));if(!q->destination_owner)return RF_FORMAT;
    q->destination_owner->word_660=(q->mode_638&255u)!=1;
    DEST_CALL(b->limit(b->context,a,&limit));stored_limit=(float)limit;
    DEST_CALL(b->select(b->context,point,a->radius_7c0,a->height_7c4,q,&value));
    if((q->mode_638&255u)==1 && q->first) {
        if((value&255u)==1) {
            if(q->second) {
                length=ai_destination_distance(q->first->query_point,q->second->query_point);
                if(!isfinite(length))return RF_FORMAT;
                if(length>0) {
                    h1=(float)((double)q->first->query_point[1]-(double)q->first->height*.5);
                    h2=(float)((double)q->second->query_point[1]-(double)q->second->height*.5);
                    DEST_CALL(rf_entity_navigation_closest_point(a->position_3c,q->first->query_point,q->second->query_point,closest,&along));
                    ratio=(double)along/(float)length;a->adjusted_62c[1]=(float)(ratio*h1+(1.0-ratio)*h2+q->offset_634);
                }
            } else a->adjusted_62c[1]=(float)(((double)q->first->position[1]-(double)q->first->height*.5)+q->offset_634);
        } else {
            for(i=0;i<3;++i)d[i]=(float)((double)a->requested_620[i]-q->first->query_point[i]);d[1]=0;
            length=sqrt(((double)d[0]*d[0]+(double)d[1]*d[1])+(double)d[2]*d[2]);
            if(!(length>0) || !isfinite(length))return RF_FORMAT;inverse=1.0/length;
            for(i=0;i<3;++i)d[i]=(float)(inverse*d[i]);radius=(float)((double)q->first->radius-a->radius_7c0);
            for(i=0;i<3;++i){scaled=(float)((double)d[i]*radius);a->adjusted_62c[i]=(float)((double)q->first->query_point[i]+scaled);}
        }
    }
    for(i=0;i<3;++i)if(!isfinite(a->adjusted_62c[i]))return RF_FORMAT;
    if(a->action_520!=2 && a->action_520!=16) {
        length=ai_destination_distance(a->adjusted_62c,a->requested_620);if(!isfinite(length))return RF_FORMAT;
        if(length>stored_limit){*result=0;return RF_OK;}
    }
    DEST_CALL(b->clear(b->context,a));
    if(q->first)DEST_CALL(b->add(b->context,a,q->first));
    if(q->second)DEST_CALL(b->add(b->context,a,q->second));
    if(a->action_520==3)DEST_CALL(b->lookup(b->context,a->target_560,&target));
    DEST_CALL(b->direct(b->context,a,target,&value));
    if((value&255u)==1) {
        a->count_588=2;a->first_58c=a->begin_5a4;a->last_590=a->requested_620;
        return ai_destination_finish(a,now,a->vector_7d4,result);
    }
    q->search_63a=1;
    if(a->action_520==3){DEST_CALL(b->limit(b->context,a,&limit));q->limit_640=(float)limit;}
    else q->limit_640=0;
    DEST_CALL(b->search(b->context,q,&value));
    if(value&255u){a->count_588=q->count_64c;if(a->count_588>=2)return ai_destination_finish(a,now,a->vector_7d4,result);}
    *result=0;return RF_OK;
#undef DEST_CALL
}

int rf_entity_navigation_basis(const float direction[3],float matrix[3][3])
{
    float value[3][3]={{0}};double length,inverse;uint32_t i;
    if(!direction || !matrix)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(direction[i]))return RF_FORMAT;
    length=sqrt(((double)direction[0]*direction[0]+(double)direction[1]*direction[1])+(double)direction[2]*direction[2]);
    if(!(length>0) || !isfinite(length))return RF_FORMAT;inverse=1.0/length;
    for(i=0;i<3;++i)value[2][i]=(float)(inverse*direction[i]);
    if(value[2][0]<.0001f && value[2][0]>-.0001f && value[2][2]<.0001f && value[2][2]>-.0001f) {
        value[0][0]=1;value[1][2]=value[2][1]<0?1:-1;value[2][1]=value[2][1]<0?-1:1;value[2][0]=value[2][2]=0;
    } else {
        value[0][0]=value[2][2];value[0][2]=-value[2][0];
        length=sqrt(((double)value[0][0]*value[0][0]+(double)value[0][1]*value[0][1])+(double)value[0][2]*value[0][2]);inverse=1.0/length;
        for(i=0;i<3;++i)value[0][i]=(float)(inverse*value[0][i]);
        value[1][0]=(float)((double)value[2][1]*value[0][2]-(double)value[2][2]*value[0][1]);
        value[1][1]=(float)((double)value[2][2]*value[0][0]-(double)value[2][0]*value[0][2]);
        value[1][2]=(float)((double)value[2][0]*value[0][1]-(double)value[2][1]*value[0][0]);
    }
    memcpy(matrix,value,sizeof(value));return RF_OK;
}

int rf_entity_navigation_pair(const float position[3],float radius,
    const rf_entity_navigation_candidate *first,const rf_entity_navigation_candidate *second,
    float *squared_distance,uint32_t *classification)
{
    float center[3],direction[3],matrix[3][3],size[3],closest[3],along,score;volatile float sum;
    float minimum;uint32_t i,inside=0;int status;
    if(!position || !first || !second || !squared_distance || !classification)return RF_RANGE;
    if(!isfinite(radius) || !isfinite(first->radius) || !isfinite(second->radius) || !isfinite(first->height) || !isfinite(second->height))return RF_FORMAT;
    for(i=0;i<3;++i) {
        if(!isfinite(position[i]) || !isfinite(first->position[i]) || !isfinite(second->position[i]))return RF_FORMAT;
        sum=(float)((double)first->position[i]+second->position[i]);center[i]=(float)((double)sum*.5);
        direction[i]=(float)((double)first->position[i]-second->position[i]);
        if(!isfinite(center[i]) || !isfinite(direction[i]))return RF_FORMAT;
    }
    if(direction[0]==0 && direction[1]==0 && direction[2]==0){*classification=2;return RF_OK;}
    status=rf_entity_navigation_basis(direction,matrix);if(status)return status;
    minimum=first->radius<second->radius?first->radius:second->radius;
    size[0]=(float)(((double)minimum+minimum)-((double)radius+radius));
    size[1]=first->height<second->height?first->height:second->height;
    size[2]=(float)sqrt(navigation_distance_squared(first->position,second->position));
    for(i=0;i<3;++i)if(!isfinite(size[i]))return RF_FORMAT;
    if(size[0]>=0 && size[1]>=0) {
        status=rf_collision_point_oriented_box(position,center,matrix,size,&inside);if(status)return status;
        if(inside){*classification=0;return RF_OK;}
    }
    size[0]=(float)((double)radius*4+size[0]);if(!isfinite(size[0]))return RF_FORMAT;
    if(size[0]>=0 && size[1]>=0) {
        status=rf_collision_point_oriented_box(position,center,matrix,size,&inside);if(status)return status;
        if(inside) {
            status=rf_entity_navigation_closest_point(position,first->position,second->position,closest,&along);if(status)return status;
            score=(float)navigation_distance_squared(position,closest);if(!isfinite(score))return RF_FORMAT;
            *squared_distance=score;*classification=1;return RF_OK;
        }
    }
    *classification=2;return RF_OK;
}

int rf_entity_ai_direct_route(rf_entity_ai_destination_actor *a,const rf_entity_ai_destination_actor *target,
    const float start[3],const float end[3],rf_entity_navigation_reference *refs,uint32_t count,uint32_t *result)
{
    uint32_t i,j,k,classification,mode;int32_t kind;int status;float score=0,first_distance;
    float radius=target?target->radius_7c0:0,height=target?target->height_7c4:0;double second_distance;
    if(!a || !start || !end || !result)return RF_RANGE;
    if(a->state_554==3){*result=1;return RF_OK;}
    if((count && !refs) || count>0x7fffffffu)return RF_RANGE;
    for(i=0;i<count;++i) {
        if(!refs[i].candidate || (refs[i].neighbor_count && !refs[i].neighbors))return RF_RANGE;
        for(k=0;k<refs[i].neighbor_count;++k)if(refs[i].neighbors[k]>=count)return RF_RANGE;
    }
    kind=a->movement_kind;mode=!(kind==12 || kind==15 || kind==13 || kind==11 || kind==9 || kind==4 || kind==7);
    for(i=0;i<count;++i) {
        status=rf_entity_navigation_single(start,a->radius_7c0,a->height_7c4,mode,refs[i].candidate,&classification);if(status)return status;
        if(classification==2)continue;
        status=rf_entity_navigation_single(end,radius,height,mode,refs[i].candidate,&classification);if(status)return status;
        if(classification!=2){a->token_69c=refs[i].order_key;*result=1;return RF_OK;}
    }
    for(i=0;i<count;++i)for(k=0;k<refs[i].neighbor_count;++k) {
        j=refs[i].neighbors[k];if(refs[i].order_key>refs[j].order_key)continue;
        status=rf_entity_navigation_pair(start,a->radius_7c0,refs[i].candidate,refs[j].candidate,&score,&classification);if(status)return status;
        if(classification!=0)continue;
        status=rf_entity_navigation_pair(end,radius,refs[i].candidate,refs[j].candidate,&score,&classification);if(status)return status;
        if(classification==2)continue;
        first_distance=(float)navigation_distance_squared(a->position_3c,refs[i].candidate->position);
        second_distance=navigation_distance_squared(a->position_3c,refs[j].candidate->position);
        if(!isfinite(first_distance) || !isfinite(second_distance))return RF_FORMAT;
        if(second_distance>first_distance){a->token_69c=refs[i].order_key;a->token_6a0=refs[j].order_key;}
        else {a->token_69c=refs[j].order_key;a->token_6a0=refs[i].order_key;}
        *result=1;return RF_OK;
    }
    *result=0;return RF_OK;
}

int rf_entity_navigation_select(rf_entity_navigation_reference *references,uint32_t count,
    const float position[3],float radius,float height,uint32_t mode,uint32_t allow_far,
    int (*visibility)(void *,const float[3],const float[3],float,uint32_t *),void *context,
    rf_entity_navigation_selection *selection)
{
    uint32_t i,j,k,result,best,blocked;float score,minimum=FLT_MAX;int status;
    if(!position || !selection || !visibility || (count && !references) || count>0x7fffffffu)return RF_RANGE;
    if(!isfinite(radius) || !isfinite(height))return RF_FORMAT;
    for(i=0;i<3;++i)if(!isfinite(position[i]))return RF_FORMAT;
    for(i=0;i<count;++i) {
        if(!references[i].candidate || (references[i].neighbor_count && !references[i].neighbors))return RF_RANGE;
        for(k=0;k<references[i].neighbor_count;++k)if(references[i].neighbors[k]>=count)return RF_RANGE;
    }
    selection->first=selection->second=UINT32_MAX;selection->contained=0;
    for(i=0;i<count;++i) {
        rf_entity_navigation_candidate *c=references[i].candidate;
        c->rejected_035=!rf_entity_navigation_candidate_allowed(radius,height,mode,c->radius,c->height,c->word_040);
    }
    for(i=0;i<count;++i) {
        rf_entity_navigation_candidate *c=references[i].candidate;if(c->rejected_035==1)continue;
        status=rf_entity_navigation_single(position,radius,height,mode,c,&result);if(status)return status;
        if(result==0){selection->first=i;selection->contained=1;return RF_OK;}
        if(result==1 && c->distance_squared<minimum){minimum=c->distance_squared;selection->first=i;}
    }
    if(selection->first!=UINT32_MAX){selection->contained=1;return RF_OK;}
    for(i=0;i<count;++i)if(references[i].candidate->rejected_035!=1)
        for(k=0;k<references[i].neighbor_count;++k) {
            j=references[i].neighbors[k];if(references[i].order_key>references[j].order_key || references[j].candidate->rejected_035==1)continue;
            status=rf_entity_navigation_pair(position,radius,references[i].candidate,references[j].candidate,&score,&result);if(status)return status;
            if(result==0){selection->first=i;selection->second=j;selection->contained=1;return RF_OK;}
            if(result==1 && score<minimum){minimum=score;selection->first=i;selection->second=j;}
        }
    if(selection->first!=UINT32_MAX && selection->second!=UINT32_MAX){selection->contained=1;return RF_OK;}
    for(;;) {
        minimum=FLT_MAX;best=UINT32_MAX;
        for(i=0;i<count;++i) {
            rf_entity_navigation_candidate *c=references[i].candidate;
            if(c->rejected_035!=1 && ((allow_far&255u) || c->distance_squared<=625.0f) && c->distance_squared<minimum){minimum=c->distance_squared;best=i;}
        }
        if(best==UINT32_MAX)return RF_OK;
        status=visibility(context,references[best].candidate->query_point,position,2.5f,&blocked);if(status)return status;
        if(!(blocked&255u)){selection->first=best;return RF_OK;}
        references[best].candidate->rejected_035=1;
    }
}

int rf_entity_navigation_search(rf_entity_navigation_reference *refs,uint32_t count,
    rf_entity_navigation_search_query *q,uint32_t *scratch,uint32_t capacity,
    const rf_entity_navigation_search_backend *b,uint32_t *result)
{
    uint32_t i,j,k,n,best,current,value,found,depth,parent;float cost_sum=0;double cost,distance;int status;
    rf_entity_navigation_candidate *node,*next;
    if(!refs || !q || !scratch || !b || !b->visible || !b->append || (q->alternate && !b->edge) || !result ||
       !count || count>65536 || capacity<count || q->start>=count || q->goal>=count)return RF_RANGE;
    for(i=0;i<count;++i) {
        if(!refs[i].candidate || !refs[i].order_key || (refs[i].neighbor_count && !refs[i].neighbors))return RF_RANGE;
        for(j=0;j<i;++j)if(refs[j].order_key==refs[i].order_key || refs[j].candidate==refs[i].candidate)return RF_FORMAT;
        for(k=0;k<refs[i].neighbor_count;++k)if(refs[i].neighbors[k]>=count)return RF_RANGE;
        for(k=0;k<3;++k)if(!isfinite(refs[i].candidate->query_point[k]))return RF_FORMAT;
    }
    for(k=0;k<3;++k)if(!isfinite(refs[q->goal].candidate->position[k]))return RF_FORMAT;
    for(i=0;i<count;++i) {
        node=refs[i].candidate;if(!node->rejected_035){node->distance_squared=FLT_MAX;node->retained_03c=0;node->flag_034=0;node->flag_036=0;}
    }
    node=refs[q->start].candidate;node->distance_squared=0;node->flag_034=1;scratch[0]=q->start;n=1;
    while(n) {
        best=0;for(i=1;i<n;++i)if(refs[scratch[i]].candidate->distance_squared<refs[scratch[best]].candidate->distance_squared)best=i;
        current=scratch[best];node=refs[current].candidate;--n;if(best<n)memmove(scratch+best,scratch+best+1,(n-best)*sizeof(*scratch));found=0;
        if(!q->alternate) {
            if(q->limit>0) {
                distance=ai_destination_distance(node->query_point,refs[q->goal].candidate->position);if(!isfinite(distance))return RF_FORMAT;
                if(distance<q->limit){status=b->visible(b->context,node,refs[q->goal].order_key,.1f,q->height,&value);if(status)return status;found=(value&255u)==1;}
            }
            if(current==q->goal)found=1;
        } else if(current!=q->start) {
            status=b->visible(b->context,node,q->alternate,0,q->height,&value);if(status)return status;found=!(value&255u);
        }
        if(found) {
            if(current==q->start){*result=1;return RF_OK;}
            depth=0;i=current;
            while(i!=q->start) {
                if(depth>=count)return RF_FORMAT;scratch[depth++]=i;parent=refs[i].candidate->retained_03c;
                for(j=0;j<count && refs[j].order_key!=parent;++j){}if(j==count)return RF_FORMAT;i=j;
            }
            if(depth>=count)return RF_FORMAT;scratch[depth++]=q->start;
            while(depth) {
                i=scratch[--depth];status=b->append(b->context,refs[i].candidate);if(status)return status;
                if(i!=q->start)cost_sum=(float)((double)cost_sum+refs[i].candidate->distance_squared);
            }
            q->cost=cost_sum;*result=1;return RF_OK;
        }
        for(k=0;k<refs[current].neighbor_count;++k) {
            j=refs[current].neighbors[k];next=refs[j].candidate;
            if(q->alternate){status=b->edge(b->context,q->alternate,node->query_point,next->query_point,q->edge_parameter,&value);if(status)return status;if(!(value&255u))continue;}
            if(next->rejected_035 || next->flag_034)continue;
            if(!next->flag_036){if(n>=capacity)return RF_RANGE;next->flag_036=1;scratch[n++]=j;}
            cost=navigation_distance_squared(next->query_point,node->query_point)+(double)node->distance_squared;
            if(!isfinite(cost))return RF_FORMAT;
            if(cost<next->distance_squared){next->distance_squared=(float)cost;next->retained_03c=refs[current].order_key;}
        }
    }
    *result=0;return RF_OK;
}
