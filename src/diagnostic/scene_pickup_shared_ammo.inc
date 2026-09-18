#ifndef RF_SCENE_PICKUP_SHARED_AMMO_INCLUDED
#define RF_SCENE_PICKUP_SHARED_AMMO_INCLUDED
/* Scene policy: a shared reserve uses the greatest capacity of its owned
 * weapons plus this pickup's target. Never lower an existing shared pool to
 * the capacity of the particular ammo-box weapon. No ownership is inferred
 * from reserve contents; the ordinary finite grant still owns all mutation. */
static int scene_pickup_shared_ammo_grant(rf_weapon_inventory *inventory,
    const rf_weapon_supply_catalog *supply,int32_t weapon,int32_t quantity,
    uint32_t gives_weapon,rf_weapon_pickup_grant *grant)
{
    rf_weapon_acquire_definition definition;uint32_t i;
    if(!inventory || !supply || !grant || supply->names.count>64 || weapon<0 ||
       (uint32_t)weapon>=supply->names.count)return RF_RANGE;
    definition=supply->definitions[weapon];
    if(definition.ammo_type<0 || definition.ammo_type>=32 || definition.capacity<0)return RF_RANGE;
    for(i=0;i<supply->names.count;i++)if(inventory->owned[i] &&
        supply->definitions[i].ammo_type==definition.ammo_type){
        if(supply->definitions[i].capacity<0)return RF_RANGE;
        if(supply->definitions[i].capacity>definition.capacity)definition.capacity=supply->definitions[i].capacity;
    }
    return rf_weapon_pickup_grant_sp(inventory,&definition,weapon,quantity,gives_weapon,grant);
}
#endif
