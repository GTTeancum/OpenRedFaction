#include "rf/entity.h"
#include <math.h>
#include <string.h>
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
