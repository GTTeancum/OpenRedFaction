#ifndef RF_SCENE_NPC_RELOAD_PROFILE_INCLUDED
#define RF_SCENE_NPC_RELOAD_PROFILE_INCLUDED
/* Bounded ordinary guard1/12mm ownership. UID109 is the original evidence
 * anchor, never a runtime identity rule.425648..425692 uses the resolved
 * action39 span;419bc0 services inventory without requiring an AI target.
 * This helper reads metadata only. It never starts a clip or transfers ammo. */
static int scene_npc_reload_named(uint32_t slot,int32_t weapon)
{
    const char *name;
    if(weapon!=campaign_pistol_id||weapon<0||weapon>=64||!campaign_seeds.items||
       !campaign_seeds.records.items||slot>=campaign_seeds.records.count)return 0;
    name=campaign_seeds.records.items[slot].record.class_name;
    return name&&rf_emitter_name_lookup(&name,1,"guard1")==0;
}
static int scene_npc_reload_identity(uint32_t slot,const campaign_npc_body *o)
{
    if(!campaign_npc_bodies||slot>=campaign_npc_body_count||o!=campaign_npc_bodies+slot||
       o->registration.object_kind!=0||o->view.type!=0||o->registration.view!=&o->view||
       o->registration.handle==UINT32_MAX||(uint32_t)o->view.handle!=o->registration.handle||
       o->damage.effects.handle!=o->registration.handle||
       rf_object_registry_lookup(&campaign_registry,o->registration.handle)!=&o->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)!=&o->view)return RF_FORMAT;
    return RF_OK;
}
static int scene_npc_reload_standalone(const campaign_npc_body *o)
{
    return o&&!(o->object_flags&8u)&&o->view.linked_handle==-1&&!o->view.occupant_count&&
        o->attachment_75c==-1&&!(campaign_player_object.view==&campaign_player_view&&
        campaign_player_view.linked_handle==(int32_t)o->registration.handle);
}
static int scene_npc_reload_binding(uint32_t slot,uint32_t cls,int32_t weapon,
    const rf_weapon_inventory *inventory,const rf_entity_motion_selection *selection,
    uint32_t resident,uint32_t *frames)
{
    const campaign_npc_body *o;const campaign_model_owner *render;
    const rf_entity_pose *pose;const rf_entity_playback_model *model;
    const rf_entity_model_motion *authored;const rf_motion_playback_resource *clip;
    const rf_weapon_acquire_definition *ammo;rf_entity_motion_selection binding;
    uint32_t skeleton,cache,milliseconds,references;uint64_t projected;int32_t motion;int status;
    volatile float seconds;double ms;const char *name;
    if(!frames||resident>1||!inventory||!selection||!scene_npc_reload_named(slot,weapon)||
       !campaign_npc_bodies||slot>=campaign_npc_body_count||!campaign_seeds.classes||
       cls>=campaign_seeds.class_count||campaign_seeds.items[slot].class_index!=cls||
       campaign_seeds.classes[cls].model_kind!=2||(campaign_seeds.classes[cls].physics.flags2&0x100u)||
       !campaign_weapon_supply.names.count||campaign_weapon_supply.names.count>64||
       (uint32_t)weapon>=campaign_weapon_supply.names.count||
       rf_weapon_name_find(&campaign_weapon_supply.names,"12mm handgun")!=weapon)return RF_FORMAT;
    name=campaign_seeds.classes[cls].model;
    if(!memchr(name,0,sizeof(campaign_seeds.classes[cls].model))||
       rf_emitter_name_lookup(&name,1,"ult2_guard.vcm")!=0)return RF_FORMAT;
    o=campaign_npc_bodies+slot;status=scene_npc_reload_identity(slot,o);if(status)return status;
    if((o->object_flags&8u)||!o->body.allocated_bytes||!o->persistence_registered||
       o->persistence_slot>=rf_scene_defeated_actors.count||
       rf_scene_defeated_actors.items[o->persistence_slot].retired||
       rf_scene_defeated_actors.items[o->persistence_slot].uid!=(uint32_t)campaign_seeds.records.items[slot].record.uid||
       !campaign_model_owners||slot>=campaign_model_owner_count||!campaign_poses.items||slot>=campaign_poses.count)return RF_FORMAT;
    render=campaign_model_owners+slot;pose=campaign_poses.items+slot;
    if(!render->registration.loaded||render->owned||render->pose!=pose||
       render->registration.active!=&pose->playback.completion.active||!pose->matrices||!pose->generations||
       !pose->bone_count||pose->bone_count>50||!render->registration.next||!render->registration.previous||
       render->registration.next->previous!=&render->registration||
       render->registration.previous->next!=&render->registration)return RF_FORMAT;
    ammo=campaign_weapon_supply.definitions+weapon;
    if(inventory->owned[weapon]!=1||ammo->capacity<0||ammo->magazine<=0||
       campaign_primary[0].magazine!=(uint32_t)ammo->magazine||campaign_primary[0].piercing_enabled||
       campaign_primary[0].damage_kind!=1||campaign_enemy_ammo_validate(inventory,ammo,campaign_primary,weapon))return RF_FORMAT;
    status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,cls,&binding);
    if(!status)status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,cls,weapon,&binding);
    if(status)return status;
    if(memcmp(&binding.mapping,&selection->mapping,sizeof(binding.mapping))||
       memcmp(binding.action_sounds,selection->action_sounds,sizeof(binding.action_sounds))||
       memcmp(binding.action_declarations,selection->action_declarations,sizeof(binding.action_declarations)))return RF_FORMAT;
    skeleton=binding.mapping.skeleton;motion=binding.mapping.actions[39];
    if(skeleton!=pose->skeleton||!campaign_skeletons.items||skeleton>=campaign_skeletons.count||
       !campaign_skeletons.items[skeleton].bones||campaign_skeletons.items[skeleton].count!=pose->bone_count||
       !campaign_playback_resources.models||skeleton>=campaign_playback_resources.model_count||
       !campaign_motion_catalog.models||skeleton>=campaign_motion_catalog.model_count||motion<0)return RF_FORMAT;
    model=campaign_playback_resources.models+skeleton;
    if(!model->resources||!model->cache_ids||(uint32_t)motion>=model->count||
       !campaign_motion_catalog.models[skeleton].items||
       (uint32_t)motion>=campaign_motion_catalog.models[skeleton].count)return RF_FORMAT;
    clip=model->resources+motion;authored=campaign_motion_catalog.models[skeleton].items+motion;
    name=authored->file.entry.name;
    if(!memchr(authored->file.entry.name,0,sizeof(authored->file.entry.name))||
       rf_emitter_name_lookup(&name,1,"ult2_reload.rfa")!=0||!authored->file.archive||!authored->file.entry.size||
       authored->file.entry.size<80||!authored->file.header[6]||
       (uint32_t)authored->comparison.start_tick!=authored->file.header[4]||
       (uint32_t)authored->comparison.end_tick!=authored->file.header[5]||
       (uint32_t)authored->comparison.fade_in!=authored->file.header[9]||
       (uint32_t)authored->comparison.fade_out!=authored->file.header[10]||
       !isfinite(authored->comparison.weight)||
       authored->looping!=0||clip->looping!=0||clip->references<0||
       memcmp(&clip->comparison,&authored->comparison,sizeof(clip->comparison))||
       (int64_t)authored->comparison.end_tick-authored->comparison.start_tick<=0||
       (int64_t)authored->comparison.end_tick-authored->comparison.start_tick>INT32_MAX)return RF_FORMAT;
    cache=model->cache_ids[motion];
    if(!campaign_npc_motion_data||!campaign_npc_motion_sizes||cache>=campaign_npc_motion_count)return RF_FORMAT;
    if(!campaign_npc_motion_data[cache]){
        /* Ordinary eviction preserves the parsed catalog/header, and is legal
         * only at zero aggregate borrowers. Recognize the episode before
         * demand without inventing an active slot or requiring its payload. */
        if(resident||authored->file.resident||campaign_npc_motion_sizes[cache])return RF_FORMAT;
        status=rf_entity_playback_cache_references(&campaign_playback_resources,cache,&references);
        if(status)return status;if(references)return RF_FORMAT;
    }else if(campaign_npc_motion_sizes[cache]!=authored->file.entry.size||
       (authored->file.resident&&authored->file.resident!=campaign_npc_motion_data[cache])||
       ((resident||clip->references)&&authored->file.resident!=campaign_npc_motion_data[cache]))return RF_FORMAT;
    /* Match original fstp(float), multiply1000 in extended/double precision,
     * positive truncation, then integer-millisecond60Hz ceiling. No68 literal. */
    seconds=(float)rf_motion_duration(authored->comparison.start_tick,authored->comparison.end_tick);
    ms=(double)seconds*1000.0;
    if(!isfinite(seconds)||seconds<=0||!isfinite(ms)||ms<1.0||ms>(double)UINT32_MAX)return RF_RANGE;
    milliseconds=(uint32_t)ms;projected=((uint64_t)milliseconds*60u+999u)/1000u;
    if(!projected||projected>3600u)return RF_RANGE;
    *frames=(uint32_t)projected;return RF_OK;
}
/* Authenticate actual slot contributions before resource demand. Reload
 * presence does not authorize any other nonloop: capture/prepare discharge
 * that separate obligation with their script/pain/burn/residual owners. */
static int scene_npc_reload_playback(uint32_t slot)
{
    const rf_entity_pose *pose=campaign_poses.items+slot;
    const rf_entity_playback_model *model=campaign_playback_resources.models+pose->skeleton;
    const rf_motion_slot_state *active=&pose->playback.completion.active;uint32_t i,j;
    if(active->count>16||active->freeze_slot< -1||active->primary_slot< -1||active->dominant_slot< -1||
       active->freeze_slot>=(int32_t)active->count||active->primary_slot>=(int32_t)active->count||
       active->dominant_slot>=(int32_t)active->count||pose->playback.completion.frozen>1||
       !isfinite(pose->playback.phase)||pose->playback.phase<0||pose->playback.phase>1)return RF_FORMAT;
    for(i=0;i<active->count;i++){
        const rf_motion_active_slot *slot=active->slots+i;const rf_motion_playback_resource *clip;
        const rf_entity_model_motion *authored;uint32_t cache;
        if(slot->motion<0||(uint32_t)slot->motion>=model->count||
           (uint32_t)slot->motion>=campaign_motion_catalog.models[pose->skeleton].count||
           !isfinite(slot->weight)||slot->weight<0)return RF_FORMAT;
        for(j=0;j<i;j++)if(active->slots[j].motion==slot->motion)return RF_FORMAT;
        clip=model->resources+slot->motion;authored=campaign_motion_catalog.models[pose->skeleton].items+slot->motion;
        cache=model->cache_ids[slot->motion];
        if(cache>=campaign_npc_motion_count||!campaign_npc_motion_data[cache]||
           authored->file.resident!=campaign_npc_motion_data[cache]||
           campaign_npc_motion_sizes[cache]!=authored->file.entry.size||clip->references<=0||
           clip->looping!=authored->looping||memcmp(&clip->comparison,&authored->comparison,sizeof(clip->comparison)))return RF_FORMAT;
    }
    return RF_OK;
}
/* Authenticate the claimed live episode without assuming a positive timer or
 * a surviving action39 slot. Canonical absence has no new-state obligations;
 * a positive legacy deadline is intentionally left to its existing owner. */
static int scene_npc_reload_live(uint32_t slot,uint32_t frame,uint32_t *frames)
{
    const campaign_npc_body *o;uint32_t span;int status;
    if(!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;o=campaign_npc_bodies+slot;
    if(o->combat_reload_pending>1)return RF_FORMAT;
    if(!o->combat_reload_pending){if(frames)*frames=0;return RF_OK;}
    if(!scene_npc_reload_standalone(o)||!isfinite(o->damage.effects.health)||o->damage.effects.health<=0||
       (o->object_flags&(2u|0x4000u))||(o->view.flags_810&1u)||
       o->combat_reload_weapon!=o->view.weapons[0])return RF_FORMAT;
    status=scene_npc_reload_binding(slot,campaign_seeds.items&&slot<campaign_seeds.records.count?campaign_seeds.items[slot].class_index:UINT32_MAX,
        o->combat_reload_weapon,&o->inventory,&o->selection,0,&span);if(status)return status;
    status=scene_npc_reload_playback(slot);if(status)return status;
    if(o->combat_reload_due>frame&&o->combat_reload_due-frame>span)return RF_FORMAT;
    if(frames)*frames=span;return RF_OK;
}
static int scene_npc_reload_live_present(uint32_t frame,uint32_t *present)
{
    uint32_t i;int status;if(!present||campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&!campaign_npc_bodies))return RF_RANGE;
    *present=0;
    for(i=0;i<campaign_npc_body_count;i++){
        status=scene_npc_reload_live(i,frame,NULL);if(status)return status;
        *present|=campaign_npc_bodies[i].combat_reload_pending;
    }
    return RF_OK;
}
#endif
