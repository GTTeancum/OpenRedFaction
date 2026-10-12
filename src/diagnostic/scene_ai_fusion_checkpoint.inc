#ifndef RF_SCENE_AI_FUSION_CHECKPOINT_INCLUDED
#define RF_SCENE_AI_FUSION_CHECKPOINT_INCLUDED
/* RFAP6 kind12 is a paid NPC Fusion flight, never a player/controller row.
 * The source may have switched weapon or become a registered corpse since
 * launch. Removed sources still need a historical-source owner and refuse;
 * neither the current gun nor a constructor-only actor supplies that proof. */
static int scene_ai_fusion_checkpoint_source(uint32_t handle,uint32_t *uid)
{
    uint32_t i,found=0,value=0;
    if(!uid||!handle||handle==UINT32_MAX||!campaign_npc_bodies||
       !campaign_seeds.records.items||campaign_seeds.records.count<campaign_npc_body_count||
       campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS)return RF_FORMAT;
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==handle){
        const campaign_npc_body *owner=scene_ai_melee_npc_identity(i,handle);
        const rf_campaign_object_record *key;
        if(found++||!owner||!owner->persistence_registered||
           owner->persistence_slot>=rf_scene_defeated_actors.count||
           owner->persistence_slot>=RF_CAMPAIGN_ACTOR_SLOTS)return RF_FORMAT;
        key=rf_scene_defeated_actors.items+owner->persistence_slot;
        value=(uint32_t)campaign_seeds.records.items[i].record.uid;
        if(!value||value>(uint32_t)INT32_MAX||key->uid!=value||key->retired||
           key->level>=rf_scene_defeated_actors.level_count||key->level>=RF_CAMPAIGN_PICKUP_LEVELS||
           !scene_npc_checkpoint_level_equal(rf_scene_defeated_actors.levels[key->level],campaign_current_level))return RF_FORMAT;
    }
    if(found!=1)return RF_NOT_FOUND;
    *uid=value;return RF_OK;
}
static int scene_ai_fusion_checkpoint_shape(const scene_fusion_projectile *shot)
{
    campaign_enemy_weapon_selection selected={0};uint32_t i,j,k;int status;
    if(!shot)return RF_RANGE;
    status=campaign_enemy_fusion_definition(campaign_fusion_id,&campaign_weapon_supply,&selected);if(status)return status;
    if(shot->npc_source!=1||shot->flight.active!=1||
       shot->flight.radius!=campaign_fusion.collision_radius||
       !isfinite(shot->flight.remaining)||shot->flight.remaining<=0||shot->flight.remaining>campaign_fusion.lifetime||
       shot->damage!=combat_enemy_primary_damage(selected.primary)||
       (shot->liquid.query_flags!=4u&&shot->liquid.query_flags!=0x1004u)||
       memcmp(&shot->definition,&campaign_fusion,sizeof(campaign_fusion)))return RF_FORMAT;
    for(i=0;i<3;i++){
        if(!isfinite(shot->flight.position[i])||fabsf(shot->flight.position[i])>100000||
           !isfinite(shot->flight.velocity[i]))return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(shot->basis[i*3+j]))return RF_FORMAT;
    }
    /* Straight flight never changes speed or orientation, including water.
     * Keep actual bits; these tolerances only admit normalized float roundoff. */
    for(i=0;i<3;i++){
        if(fabsf(shot->flight.velocity[i]/campaign_fusion.speed-shot->basis[6+i])>.001f)return RF_FORMAT;
        for(j=i;j<3;j++){
            double dot=0;for(k=0;k<3;k++)dot+=(double)shot->basis[i*3+k]*shot->basis[j*3+k];
            if(fabs(dot-(i==j?1.0:0.0))>.001)return RF_FORMAT;
        }
        j=(i+1)%3;k=(i+2)%3;
        if(fabsf(shot->basis[j]*shot->basis[3+k]-shot->basis[k]*shot->basis[3+j]-shot->basis[6+i])>.001f)return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_ai_projectile_checkpoint_npc_fusion_admit(const scene_ai_projectile_checkpoint_stage *stage,
    const scene_npc_checkpoint_restore_stage *npcs)
{
    if(!stage)return RF_OK;
    for(uint32_t i=0;i<SCENE_FUSION_CAPACITY;i++)if(stage->fusion[i].flight.active&&stage->fusion[i].npc_source){
        const scene_npc_checkpoint_restore_entry *source=NULL;
        if(!npcs||!stage->fusion_source_uid[i])return RF_FORMAT;
        for(uint32_t j=0;j<npcs->count;j++)if(npcs->entries[j].saved.uid==stage->fusion_source_uid[i]){
            if(source)return RF_FORMAT;source=npcs->entries+j;
        }
        /* RFNC owns retirement; a pristine startup registration must never
         * resurrect or stand in for an absent/retired candidate source. */
        if(!source||!source->registered||source->saved.retired||
           !source->handle||source->handle==UINT32_MAX||source->handle!=stage->fusion[i].source||
           source->slot>=campaign_npc_body_count||
           (uint32_t)campaign_seeds.records.items[source->slot].record.uid!=stage->fusion_source_uid[i])return RF_FORMAT;
    }
    return RF_OK;
}
#endif
