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
/* The shared collectible policy lives in scene_ai_drop_supply.inc, included
 * earlier. Return only independent weapon slots; the normal resource planner
 * expands Remote Charge and Machine Pistol into their companion view pairs. */
static uint32_t scene_enemy_drop_resource_mask(void)
{
    uint32_t i,mask=0;
    if(!campaign_npc_bodies || campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS ||
       !campaign_weapon_supply.names.count || campaign_weapon_supply.names.count>64 ||
       rf_scene_defeated_actors.count>RF_CAMPAIGN_ACTOR_SLOTS ||
       rf_scene_defeated_actors.level_count>RF_CAMPAIGN_PICKUP_LEVELS)return 0;
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *owner=campaign_npc_bodies+i;const rf_campaign_weapon_drop *drop=NULL;
        int32_t weapon,view_slot;uint32_t slot=UINT32_MAX,j;
        if(owner->persistence_registered && owner->persistence_slot>=rf_scene_defeated_actors.count)continue;
        if(!owner->persistence_registered && (!campaign_seeds.records.items || i>=campaign_seeds.records.count))continue;
        (void)scene_precision_owner_persistence_slot(owner,&slot);
        if(slot!=UINT32_MAX)drop=rf_scene_defeated_actors.drops+slot;
        if(drop && drop->state==1){if(drop->quantity<0)continue;weapon=drop->weapon;}
        else if(!drop || (drop->state==0 && !rf_scene_defeated_actors.items[slot].retired)){
            /* A living actor can switch to an owned fallback before death.
             * Reserve its future collectible view without granting it. */
            for(j=0;j<campaign_weapon_supply.names.count;j++)if(owner->inventory.owned[j]){
                view_slot=scene_enemy_drop_weapon_slot(&campaign_weapon_supply.names,(int32_t)j);
                if(view_slot>=0 && view_slot<32)mask|=1u<<(uint32_t)view_slot;
            }
            continue;
        }else continue;
        view_slot=scene_enemy_drop_weapon_slot(&campaign_weapon_supply.names,weapon);
        if(view_slot>=0 && view_slot<32)mask|=1u<<(uint32_t)view_slot;
    }
    return mask;
}
#endif
