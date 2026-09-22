#ifndef RF_SCENE_EXTRA_PICKUPS_GAMEPLAY_INCLUDED
#define RF_SCENE_EXTRA_PICKUPS_GAMEPLAY_INCLUDED
/* Include once after scene_weapon_resource_demand.inc and scene globals.
 * Forward-declare helpers used by earlier availability/collection code.
 * This owns only the optional campaign firearm resource mask, never inventory
 * selection. Existing scene failure/close paths own opened view resources. */
static uint32_t scene_extra_pickups_resource_mask;
static void scene_extra_pickups_resources_reset(void)
{scene_extra_pickups_resource_mask=0;}
static uint32_t scene_extra_pickups_available(uint32_t slot)
{return slot<SCENE_WEAPON_SLOTS && !!(scene_extra_pickups_resource_mask&(1u<<slot));}
static uint32_t scene_extra_pickups_resource_limit(void)
{
    uint32_t mask=scene_extra_pickups_resource_mask,n=0;
    while(mask){++n;mask>>=1;}return n;
}
static uint32_t scene_extra_pickups_selection_limit(void)
{
    /* Alternate MP resource17 remains one weapon at cycling slot13. */
    uint32_t n=scene_extra_pickups_resource_limit();return n>17?17:n;
}
static int scene_extra_pickups_resolve_item(void *context,const char *name,rf_item_definition *item)
{return rf_item_definition_load((rf_vpp *)context,name,128*1024,item);}
/* Replaces the current planner call, preserving shield/base demand. Publish
 * only after all needed primary definitions load; no partial new mask. Call
 * reset on scene entry BEFORE earlier availability-sensitive setup. */
static int scene_extra_pickups_resources_prepare(scene_stream *stream,rf_vpp *tables,
    uint32_t base_mask,uint32_t other_optional,scene_weapon_resource_demand *result)
{
    scene_weapon_resource_demand demand;rf_weapon_primary_definition primary[8];
    uint32_t optional,pair,expanded,slots[8],count=0,i;int status;
    if(!stream || !tables || !result)return RF_RANGE;
    status=scene_extra_pickup_demand_slots(campaign_weapon_names,SCENE_WEAPON_SLOTS,&optional,&pair);if(status)return status;
    status=scene_weapon_resources_plan(base_mask,other_optional|optional,campaign_weapon_names,
        SCENE_WEAPON_SLOTS,&campaign_weapon_supply.names,
        campaign_import_pending?&campaign_player_import.inventory:NULL,
        &stream->pickups,&campaign_events.decoded,scene_extra_pickups_resolve_item,tables,&demand);if(status)return status;
    status=scene_extra_pickup_demand_expand(campaign_weapon_names,SCENE_WEAPON_SLOTS,demand.mask,&expanded);if(status)return status;
    for(i=0;i<SCENE_WEAPON_SLOTS;i++)if((expanded&optional)&(1u<<i)){
        if(count>=8)return RF_RANGE;
        slots[count]=i;status=rf_weapon_primary_load(tables,campaign_weapon_names[i],128*1024,primary+count);if(status)return status;
        ++count;
    }
    for(i=0;i<count;i++)campaign_primary[slots[i]]=primary[i];
    demand.mask=expanded;scene_extra_pickups_resource_mask=expanded&optional;*result=demand;return RF_OK;
}
/* After the existing sparse view-open loop. Replaces the DEV-only MP custom
 * resource block; current scene close already releases machine_custom[]. */
static int scene_extra_pickups_machine_resources(scene_stream *stream,rf_vpp *motions)
{
    int status;uint32_t i;
    if(!stream || !motions)return RF_RANGE;
    if(!stream->player_weapon[13] && !stream->player_weapon[17])return RF_OK;
    if(!stream->player_weapon[13] || !stream->player_weapon[17])return RF_NOT_FOUND;
    status=scene_machine_pistol_mode_timing(motions,stream->machine_transition_ticks);if(status)return status;
    for(i=0;i<2;i++)if(!stream->machine_custom[i]){
        status=scene_weapon_custom_actions_open(stream->player_weapon[i?17:13],motions,i,256*1024,stream->machine_custom+i);
        if(status)return status;
    }
    return RF_OK;
}
/* Collection branch before legacy slot mapping; handled=1 suppresses that
 * mapping even on a full inventory. Caller retains proximity/cover, records
 * retirement only on acquired/rounds, and calls normal ammo publication. */
static int scene_extra_pickups_collect(const rf_level_item *item,const rf_item_definition *definition,
    rf_weapon_pickup_grant *grant,uint32_t *handled)
{
    if(!item || !definition || !grant || !handled)return RF_RANGE;
    *handled=scene_extra_pickup_index(item->class_name)>=0;
    if(!*handled)return RF_OK;
    return scene_extra_pickup_grant(item->class_name,definition,&campaign_weapon_supply,
        &campaign_player_inventory,item->quantity,grant);
}
/* Resource loader validation before its legacy slot/name comparison; no
 * indexing an absent player slot. Existing dynamic resource owner stays. */
static int scene_extra_pickups_resource_validate(const char *name,const rf_item_definition *definition,uint32_t *handled)
{
    int index=scene_extra_pickup_index(name);int32_t weapon;
    if(!definition || !handled)return RF_RANGE;*handled=index>=0;if(index<0)return RF_OK;
    if(definition->mesh_kind!=1 || !scene_extra_pickup_equal(definition->weapon,scene_extra_pickup_weapons[index]) ||
       definition->gives_weapon!=scene_extra_pickup_gives_weapon[index])return RF_FORMAT;
    weapon=rf_weapon_name_find(&campaign_weapon_supply.names,definition->weapon);
    return weapon>=0 && weapon<64?RF_OK:RF_NOT_FOUND;
}
#endif
