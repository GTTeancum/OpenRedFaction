#include "rf/entity.h"
#include <math.h>

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
