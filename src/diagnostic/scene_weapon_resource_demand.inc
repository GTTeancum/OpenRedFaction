/* Sparse first-person resource planning, not resource loading or item grants.
 * Preserve explicit base_mask; only optional_mask bits can be added. For the
 * current shield extension use optional_mask=1u<<11, avoiding the prefix12
 * mistake that would load every intervening campaign weapon. No allocation.
 * Resolver may reuse rf_item_definition_load with bounded temporary scratch.
 */
#include "rf/entity_assets.h"
#include <string.h>
typedef struct scene_weapon_resource_demand {
    uint32_t mask,owned_mask,pickup_mask,event_mask,unresolved_items;
} scene_weapon_resource_demand;
typedef int (*scene_weapon_demand_item_resolver)(void *,const char *,rf_item_definition *);
static int scene_weapon_demand_equal(const char *a,const char *b)
{
    unsigned char x,y;do{x=(unsigned char)*a++;y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;
        if(x!=y)return 0;
    }while(x);return 1;
}
static int scene_weapon_demand_item(const char *name,uint32_t optional_mask,
    const char *const *slots,uint32_t count,scene_weapon_demand_item_resolver resolve,
    void *context,uint32_t *mask,uint32_t *unresolved)
{
    rf_item_definition definition;uint32_t i;int status;
    if(!name[0])return RF_OK;
    status=resolve(context,name,&definition);
    if(status==RF_NOT_FOUND){++*unresolved;return RF_OK;}if(status)return status;
    if(!memchr(definition.weapon,0,sizeof(definition.weapon)))return RF_FORMAT;
    /* Ammo-only pickups cannot make an unowned weapon selectable; its owning
     * inventory, actual weapon pickup or grant supplies the demand instead. */
    if(!definition.gives_weapon || !definition.weapon[0] || (definition.flags&1u))return RF_OK;
    for(i=0;i<count;++i)if((optional_mask&(1u<<i)) && scene_weapon_demand_equal(definition.weapon,slots[i]))*mask|=1u<<i;
    return RF_OK;
}
static int scene_weapon_resources_plan(uint32_t base_mask,uint32_t optional_mask,
    const char *const *slots,uint32_t slot_count,const rf_weapon_names *weapons,
    const rf_weapon_inventory *inventory,const rf_level_owned_items *pickups,
    const rf_level_owned_events *events,scene_weapon_demand_item_resolver resolve,
    void *context,scene_weapon_resource_demand *result)
{
    scene_weapon_resource_demand value={0};uint32_t i,valid_mask;int status;
    if(!slots || !slot_count || slot_count>32 || !weapons || weapons->count>64 || !result ||
       (pickups && pickups->count && !pickups->items) || (events && events->count && !events->items))return RF_RANGE;
    valid_mask=slot_count==32?UINT32_MAX:((1u<<slot_count)-1);
    if((base_mask|optional_mask)&~valid_mask)return RF_RANGE;
    for(i=0;i<slot_count;++i)if((optional_mask&(1u<<i)) && (!slots[i] || !slots[i][0]))return RF_RANGE;
    if(optional_mask && ((pickups && pickups->count) || (events && events->count)) && !resolve)return RF_RANGE;
    value.mask=base_mask;
    for(i=0;i<slot_count && optional_mask;++i)if(optional_mask&(1u<<i)){
        int32_t weapon=rf_weapon_name_find(weapons,slots[i]);if(weapon<0)return RF_NOT_FOUND;
        if(inventory && inventory->owned[weapon])value.owned_mask|=1u<<i;
    }
    for(i=0;optional_mask && pickups && i<pickups->count;++i){
        const char *name=pickups->items[i].class_name;
        if(!memchr(name,0,sizeof(pickups->items[i].class_name)))return RF_FORMAT;
        status=scene_weapon_demand_item(name,optional_mask,slots,slot_count,resolve,context,&value.pickup_mask,&value.unresolved_items);if(status)return status;
    }
    for(i=0;optional_mask && events && i<events->count;++i){
        const rf_level_event *event=&events->items[i].record;
        uint32_t slot;
        if(!memchr(event->type,0,sizeof(event->type)))return RF_FORMAT;
        /* Go_Undercover equips this weapon directly, without an item grant. */
        if(scene_weapon_demand_equal(event->type,"Go_Undercover")){
            for(slot=0;slot<slot_count;slot++)if((optional_mask&(1u<<slot))&&
                scene_weapon_demand_equal(slots[slot],"Undercover 12mm handgun"))value.event_mask|=1u<<slot;
            continue;
        }
        if(!scene_weapon_demand_equal(event->type,"Give_Item_To_Player"))continue;
        if(!memchr(event->texts[0],0,sizeof(event->texts[0])))return RF_FORMAT;
        status=scene_weapon_demand_item(event->texts[0],optional_mask,slots,slot_count,resolve,context,&value.event_mask,&value.unresolved_items);if(status)return status;
    }
    value.mask|=value.owned_mask|value.pickup_mask|value.event_mask;*result=value;return RF_OK;
}
