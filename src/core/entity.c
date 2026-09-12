#include "rf/entity.h"
#include "rf/timer.h"
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
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
    rf_corpse_emitter_link *e;rf_corpse_sound_view *sound;uint32_t proceed,visits=0;
    int status,expired;double value,duration;float point[3],seconds;
    if(!s || !b || !b->reset || !b->play || !b->duration || !b->advance || !b->pose ||
       !b->sound || !b->follow_point || !b->move_sound)return RF_RANGE;
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
    if(s->sound_2cc!=-1) {
        sound=b->sound(b->context,s->sound_2cc);
        if(sound) {
            memset(point,0,sizeof(point));status=b->follow_point(b->context,point);if(status)return status;
            memcpy(sound->position,point,sizeof(point));b->move_sound(b->context,sound,point);
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
    rf_corpse_delete_emitter *e,*next;uint32_t visits=0,*sound,handle;int status;
    if(!s || !s->update || !b || !b->effect || !b->sound_flags || !corpse_count || !object_count ||
       corpse_count==object_count || !*corpse_count || !*object_count || s->lifecycle || !s->registered_object ||
       !corpse_link_valid(&s->corpse_link) || !corpse_link_valid(&s->object_link))return RF_RANGE;
    if(rf_object_registry_lookup(registry,s->handle)!=s->registered_object)return RF_NOT_FOUND;
    for(e=s->emitters;e;e=e->next) {if(visits==limit)return RF_RANGE;++visits;}
    handle=s->handle;s->lifecycle=1;
    b->effect(b->context,RF_CORPSE_DELETE_PAIRS,handle);
    b->effect(b->context,RF_CORPSE_DELETE_STRING,handle);
    sound=b->sound_flags(b->context,s->update->sound_2cc);
    if(sound) {*sound|=2u;s->update->sound_2cc=-1;}
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
    rf_physics_body_parameters parameters={0};rf_physics_mass_tensor initial={0},prepared;
    rf_physics_sphere fallback={0};rf_physics_fallback values;
    const rf_physics_sphere *spheres;uint32_t count;int status;
    if(!seed || !result || (seed->flags!=0x33u && seed->flags!=0x73u) ||
       (seed->sphere_count && !seed->spheres))return RF_RANGE;
    if(result->allocated_bytes || result->spheres.items || result->spheres.count || result->spheres.allocated_bytes ||
       sizeof(*result)+(uint64_t)(seed->sphere_count?seed->sphere_count:1)*sizeof(*spheres)>budget)return RF_RANGE;
    memcpy(&parameters.coefficients[1],&seed->word_0c,4);memcpy(&parameters.mass,&seed->word_14,4);
    parameters.coefficients[0]=elasticity;parameters.coefficients[2]=friction;parameters.flags=seed->flags;
    memcpy(parameters.position,seed->position,12);memcpy(parameters.orientation,seed->basis,36);
    spheres=seed->spheres;count=seed->sphere_count;
    if(!count) {
        status=rf_physics_fallback_prepare(density,seed->radius,parameters.mass,&values);if(status)return status;
        parameters.mass=values.mass;fallback.radius=values.radius;fallback.parameter_10=values.parameter_10;
        parameters.local_tensor[0]=parameters.local_tensor[4]=parameters.local_tensor[8]=1;
        spheres=&fallback;count=1;
    } else if(parameters.mass<=0) {
        initial.mass=parameters.mass;
        status=rf_physics_spheres_prepare(spheres,count,density,&initial,&prepared);if(status)return status;
        parameters.mass=prepared.mass;memcpy(parameters.local_tensor,prepared.tensor,36);
    }
    return rf_physics_body_open(&parameters,spheres,count,budget,result);
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
static uint32_t *corpse_owned_delete_sound(void *context,int32_t id)
{
    corpse_owned_delete_context *c=context;return c->backend->sound_flags(c->backend->context,id);
}
int rf_corpse_owned_delete(rf_corpse_owners *owners,uint32_t index,rf_object_registry *registry,
    uint32_t *corpse_count,uint32_t *object_count,uint32_t limit,const rf_corpse_delete_backend *backend)
{
    rf_corpse *corpse;corpse_owned_delete_context context;rf_corpse_delete_backend bridge;
    if(!owners || index>=RF_CORPSE_CAPACITY || !(owners->pool.active_mask&(1u<<index)) ||
       owners->allocated_bytes<sizeof(*owners) || owners->allocated_bytes>owners->budget ||
       !backend || !backend->effect || !backend->sound_flags)return RF_RANGE;
    corpse=&owners->slots[index].corpse;
    if(corpse->deletion.registered_object!=corpse || corpse->deletion.update!=&corpse->update)return RF_RANGE;
    context.owners=owners;context.index=index;context.backend=backend;
    bridge.effect=corpse_owned_delete_effect;bridge.sound_flags=corpse_owned_delete_sound;bridge.context=&context;
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

rf_corpse *rf_entity_finalize_create_owned(void *context,rf_entity_finalize_state *source,const char *name)
{
    rf_entity_finalize_corpse_binding *b=context;rf_corpse_create_request request;rf_corpse *result=NULL;uint32_t i;
    if(!b)return NULL;
    if(b->partial){b->status=RF_RANGE;return NULL;}
    b->status=RF_RANGE;b->cleanup_status=0;
    if(!source || !b->source || b->source->handle!=source->handle || !b->ownership ||
       !b->ownership->owners || !b->ownership->registry || !b->ownership->object_count ||
       !b->head || !b->count || !b->create || !b->destroy || !b->destroy->effect || !b->destroy->sound_flags)return NULL;
    request=b->request;request.death_name=name;memcpy(request.position,source->position,12);memcpy(request.basis,source->basis,36);
    request.protected_body=0;request.seek_motion=0;
    b->source->object_flags=source->object_flags;b->source->flags_810=source->flags_810;b->source->replacement_model=source->replacement_model;
    b->status=rf_corpse_owned_create(b->ownership,b->source,&request,b->head,b->count,b->create,&result);
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
    if((state->flags_810&0x400u) || state->damage_138c==13 || state->damage_1390==13)action=16;
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
