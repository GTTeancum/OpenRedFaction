#ifndef RF_SCENE_PRECISION_DROP_DEMAND_INC
#define RF_SCENE_PRECISION_DROP_DEMAND_INC
/* Resource demand only, before ammo-reset resolves the global weapon IDs.
 * Include current-level owners only: other levels' persistent drops must not
 * grow this scene's FP residency. Empty guns still supply ownership. */
/* Returns1 only for a valid matching persistent slot, otherwise0; output is
 * unchanged on0. Does not register actors or mutate the persistence store. */
static uint32_t scene_precision_owner_persistence_slot(const campaign_npc_body *owner,uint32_t *out)
{
    uint32_t i,j;
    if(!owner || !out || !campaign_npc_bodies || campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS ||
       rf_scene_defeated_actors.count>RF_CAMPAIGN_ACTOR_SLOTS || rf_scene_defeated_actors.level_count>RF_CAMPAIGN_PICKUP_LEVELS)return 0;
    if(owner->persistence_registered){
        if(owner->persistence_slot>=rf_scene_defeated_actors.count)return 0;
        *out=owner->persistence_slot;return 1;
    }
    for(i=0;i<campaign_npc_body_count && campaign_npc_bodies+i!=owner;i++){}
    if(i==campaign_npc_body_count || !campaign_seeds.records.items || i>=campaign_seeds.records.count)return 0;
    for(j=0;j<rf_scene_defeated_actors.count;j++){
        const rf_campaign_object_record *r=rf_scene_defeated_actors.items+j;
        if(r->uid!=(uint32_t)campaign_seeds.records.items[i].record.uid || r->level>=rf_scene_defeated_actors.level_count)continue;
        if(!memchr(campaign_current_level,0,sizeof(campaign_current_level)) || !memchr(rf_scene_defeated_actors.levels[r->level],0,64))return 0;
        if(scene_weapon_demand_equal(campaign_current_level,rf_scene_defeated_actors.levels[r->level])){*out=j;return 1;}
    }
    return 0;
}
static uint32_t scene_precision_drop_resource_mask(void)
{
    int32_t ids[2];uint32_t i,j,mask=0;
    if(!campaign_npc_bodies || campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS ||
       campaign_weapon_supply.names.count>64 || rf_scene_defeated_actors.count>RF_CAMPAIGN_ACTOR_SLOTS ||
       rf_scene_defeated_actors.level_count>RF_CAMPAIGN_PICKUP_LEVELS)return 0;
    ids[0]=rf_weapon_name_find(&campaign_weapon_supply.names,"Sniper Rifle");
    ids[1]=rf_weapon_name_find(&campaign_weapon_supply.names,"rail_gun");
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *owner=campaign_npc_bodies+i;const rf_campaign_weapon_drop *drop=NULL;int32_t weapon;uint32_t slot=UINT32_MAX;
        if(owner->persistence_registered && owner->persistence_slot>=rf_scene_defeated_actors.count)continue;
        if(!owner->persistence_registered && (!campaign_seeds.records.items || i>=campaign_seeds.records.count))continue;
        (void)scene_precision_owner_persistence_slot(owner,&slot);
        if(slot!=UINT32_MAX)drop=rf_scene_defeated_actors.drops+slot;
        if(drop && drop->state==1){if(drop->quantity<0)continue;weapon=drop->weapon;}
        else if(!drop || (drop->state==0 && !rf_scene_defeated_actors.items[slot].retired)){
            weapon=owner->view.weapons[0];
            if(weapon<0 || weapon>=64 || !owner->inventory.owned[weapon])continue;
        }else continue;
        for(j=0;j<2;j++)if(ids[j]>=0 && ids[j]<64 && weapon==ids[j])mask|=1u<<(6+j);
    }
    return mask;
}
#endif
