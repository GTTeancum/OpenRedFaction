#ifndef RF_SCENE_NPC_RELOAD_CHECKPOINT_INCLUDED
#define RF_SCENE_NPC_RELOAD_CHECKPOINT_INCLUDED
/* Residual ordinary fire/reload presentation after gameplay obligations end.
 * Original42c3c0/425648 start nonfreezing clips;419d8c completes ammunition
 * without stopping reload. Ordinary starts retain other nonloop slots, and
 * visibility/LOD may suspend the ordinary51ba80 model clock.
 * Callers separately prove candidate life and absent script/combat/reload
 * owners. No stale combat_reload_weapon, timer inference or mutable callback.
 * Exact serialized selection/playback is sufficient for this narrow profile. */
static int scene_npc_checkpoint_residual_weapon_clip(uint32_t index,uint32_t cls,int32_t weapon,
    const rf_weapon_inventory *inventory,const rf_entity_motion_selection *selection,
    const rf_motion_playback_state *playback,int32_t motion,uint32_t capturing)
{
    const int32_t ids[8]={campaign_pistol_id,campaign_rifle_id,campaign_riot_id,campaign_shotgun_id,
        campaign_rocket_id,campaign_grenade_id,campaign_sniper_id,campaign_rail_id};
    campaign_enemy_weapon_selection selected={0};rf_entity_motion_selection binding;
    const campaign_npc_body *owner;const campaign_model_owner *model_owner;
    const rf_entity_playback_model *model;const rf_motion_slot_state *active;
    uint32_t skeleton,i,j,ordinary,found=0;int status;
    if(capturing>1||!inventory||!selection||!playback||!campaign_npc_bodies||index>=campaign_npc_body_count||
       !campaign_seeds.items||index>=campaign_seeds.records.count||!campaign_seeds.classes||cls>=campaign_seeds.class_count||
       campaign_seeds.items[index].class_index!=cls||campaign_seeds.classes[cls].model_kind!=2||
       !campaign_poses.items||index>=campaign_poses.count||
       !campaign_model_owners||index>=campaign_model_owner_count||weapon<0||weapon>=64||
       inventory->owned[weapon]!=1||!campaign_weapon_supply.names.count||
       campaign_weapon_supply.names.count>64||(uint32_t)weapon>=campaign_weapon_supply.names.count)return 0;
    owner=campaign_npc_bodies+index;model_owner=campaign_model_owners+index;
    if(!owner->persistence_registered||owner->registration.view!=&owner->view||
       rf_object_registry_lookup(&campaign_registry,owner->registration.handle)!=&owner->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)owner->registration.handle)!=&owner->view||
       !model_owner->registration.loaded||model_owner->owned||
       model_owner->pose!=campaign_poses.items+index||
       (capturing&&playback!=&model_owner->pose->playback)||
       !model_owner->pose->matrices||!model_owner->pose->generations||
       !model_owner->pose->bone_count||model_owner->pose->bone_count>50||
       model_owner->registration.active!=&model_owner->pose->playback.completion.active||
       !model_owner->registration.next||!model_owner->registration.previous||
       model_owner->registration.next->previous!=&model_owner->registration||
       model_owner->registration.previous->next!=&model_owner->registration)return 0;
    status=campaign_enemy_weapon_select(weapon,ids,campaign_primary,campaign_weapon_supply.definitions,
        campaign_weapon_supply.names.count,&selected);
    if(status==RF_NOT_FOUND)status=campaign_enemy_extra_weapon_select(weapon,&campaign_weapon_supply,&selected);
    /* These two selectors admit only the nine synchronous ordinary firearms
     * once melee/reserve-fed profiles are rejected below. Rocket keeps the
     * existing action39-only exception; it never gains residual fire admission. */
    ordinary=status==RF_OK;
    if(status==RF_NOT_FOUND&&weapon==campaign_rocket_id&&scene_rocket_resources){
        selected.primary=campaign_primary+4;selected.ammo=campaign_weapon_supply.definitions+weapon;status=RF_OK;
    }
    if(status||!selected.primary||!selected.ammo||selected.melee||selected.reserve_fed||
       selected.ammo->capacity<0||selected.ammo->magazine<=0||
       selected.primary->magazine!=(uint32_t)selected.ammo->magazine||
       campaign_enemy_ammo_validate(inventory,selected.ammo,selected.primary,weapon))return 0;
    status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,cls,&binding);
    if(!status)status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,cls,weapon,&binding);
    /* Compare members, not struct padding; weapon aliases belong to42ab20. */
    if(status||memcmp(&binding.mapping,&selection->mapping,sizeof(binding.mapping))||
       memcmp(binding.action_sounds,selection->action_sounds,sizeof(binding.action_sounds))||
       memcmp(binding.action_declarations,selection->action_declarations,sizeof(binding.action_declarations))||
       motion<0||(motion!=binding.mapping.actions[39]&&
                  (!ordinary||motion!=binding.mapping.actions[2])))return 0;
    skeleton=binding.mapping.skeleton;
    if(skeleton!=model_owner->pose->skeleton||!campaign_skeletons.items||skeleton>=campaign_skeletons.count||
       !campaign_skeletons.items[skeleton].bones||
       campaign_skeletons.items[skeleton].count!=model_owner->pose->bone_count||!campaign_playback_resources.models||
       skeleton>=campaign_playback_resources.model_count||!campaign_motion_catalog.models||
       skeleton>=campaign_motion_catalog.model_count)return 0;
    model=campaign_playback_resources.models+skeleton;active=&playback->completion.active;
    if(!model->resources||!model->cache_ids||!campaign_motion_catalog.models[skeleton].items||
       !active->count||active->count>16||playback->completion.frozen||active->freeze_slot!=-1||
       !isfinite(playback->phase)||playback->phase<0||playback->phase>1)return 0;
    /* Prove only the requested mapped fire/reload slot. Other nonloops
     * still need their own capture owner or candidate restore/RFAP join;
     * their presence neither grants ownership nor vetoes this residual.
     * Keep all-slot resource and duplicate-motion checks below. */
    for(i=0;i<active->count;i++){
        const rf_motion_active_slot *slot=active->slots+i;const rf_motion_playback_resource *clip;
        const rf_entity_model_motion *authored;uint32_t cache;int64_t duration;
        if(slot->motion<0||(uint32_t)slot->motion>=model->count||
           (uint32_t)slot->motion>=campaign_motion_catalog.models[skeleton].count||
           !isfinite(slot->weight)||slot->weight<0)return 0;
        for(j=0;j<i;j++)if(active->slots[j].motion==slot->motion)return 0;
        clip=model->resources+slot->motion;authored=campaign_motion_catalog.models[skeleton].items+slot->motion;
        cache=model->cache_ids[slot->motion];duration=(int64_t)clip->comparison.end_tick-clip->comparison.start_tick;
        if(!campaign_npc_motion_data||cache>=campaign_npc_motion_count||!campaign_npc_motion_data[cache]||
           authored->file.resident!=campaign_npc_motion_data[cache]||clip->references<(capturing?1:0)||
           clip->looping!=authored->looping||memcmp(&clip->comparison,&authored->comparison,sizeof(clip->comparison))||
           duration<=0||duration>INT32_MAX)return 0;
        /* Newly applied loops can still have tick0 until the next advancing
         * frame; the requested residual needs an unfinished authored cursor. */
        if(!clip->looping&&slot->motion==motion){
            if(slot->weight<=0||slot->weight>1||
               slot->tick<clip->comparison.start_tick||slot->tick>=clip->comparison.end_tick)return 0;
            ++found;
        }
    }
    return found==1;
}
#endif
