#include "rf/entity.h"
#include "rf/timer.h"
#include <math.h>
#include <string.h>
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
