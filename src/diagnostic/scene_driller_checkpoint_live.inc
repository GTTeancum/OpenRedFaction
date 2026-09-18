/* Shared RFCP3 save dispatch: standing RFPL2 or explicitly paired seated
 * RFPL3. Host geometry and seat placement are admitted before publication. */
static int scene_vehicle_checkpoint_capture(scene_stream *s,void *blob,uint32_t *bytes)
{
    rf_vehicle_checkpoint record;int status;
    if(!s||!blob||!bytes)return RF_RANGE;
    *bytes=0;if(!s->driller)return RF_OK;
    if(!s->terrain_authored)return RF_RANGE;
    status=scene_driller_checkpoint_capture(s,&record);if(status)return status;
    if(record.player_occupied && !record.alive)return RF_RANGE;
    status=rf_vehicle_checkpoint_encode(&record,blob,RF_VEHICLE_CHECKPOINT_BYTES);
    if(!status)*bytes=RF_VEHICLE_CHECKPOINT_BYTES;
    return status;
}
static int scene_vehicle_checkpoint_read(scene_stream *s,const void *blob,uint32_t bytes,rf_vehicle_checkpoint *out)
{
    scene_driller_physics physics;scene_driller_checkpoint_candidate staged;rf_vehicle_checkpoint record;int status;
    if(!s||!out)return RF_RANGE;
    if(!bytes)return s->driller?RF_FORMAT:RF_OK;
    if(!s->driller||!s->terrain_authored||strcmp(campaign_current_level,"ctf06.rfl"))return RF_FORMAT;
    if(s->driller_runtime && !scene_driller_active(s)){const scene_driller_runtime *r=s->driller_runtime;
        if(!scene_driller_damage_runtime_valid(&r->damage)||r->damage.entry!=&r->entry||r->entry.physics!=&r->physics||
           r->entry.host.driver!=UINT32_MAX||r->entry.occupant!=-1||r->player.saved||r->damage.damage.state.effects.burn||
           r->entry.player_view->linked_handle==(int32_t)r->entry.host.handle)return RF_FORMAT;
    }
    status=rf_vehicle_checkpoint_decode(blob,bytes,&record);if(status)return status;
    if(record.player_occupied && !record.alive)return RF_FORMAT;
    status=scene_driller_physics_initialize(s->driller,record.position,record.orientation,scene_gravity.acceleration,&physics);if(status)return status;
    status=scene_driller_checkpoint_stage_rigid(&physics.state,&record,&staged);if(status)return status;
    *out=record;return RF_OK;
}
static int scene_vehicle_checkpoint_fit(scene_stream *s,scene_authored_collection_stage *stage,scene_authored_checkpoint_stage *single,
    const rf_vehicle_checkpoint *record,const rf_checkpoint_placement *player)
{
    scene_driller_physics physics;uint32_t i,j,k,q;int status;
    if(!s||(!stage&&!single)||!record||!player||!s->driller)return RF_RANGE;
    status=scene_driller_physics_initialize(s->driller,record->position,record->orientation,scene_gravity.acceleration,&physics);if(status)return status;
    status=stage?scene_driller_checkpoint_placement(stage,&physics,record):scene_driller_checkpoint_placement_single(single,&physics,record);if(status)return status;
    if(record->player_occupied){
        rf_collision_composition_view composition;rf_geomod_terrain_view full;
        const scene_stream *world=stage?&stage->candidate:single->scene;
        const scene_authored_checkpoint_stage *shared=stage?&stage->shared:single;
        status=rf_collision_composition_pending(world->terrain_publication->composition,&composition);if(status)return status;
        full=shared->publication;full.faces=composition.tree->faces;full.tree=composition.tree;full.mesh.face_count=composition.count;
        status=rf_checkpoint_placement_check(&full,player,NULL);if(status)return status;
        return stage?scene_collection_player_placement(world,player):rf_geomod_piece_registry_placement_check(single->pieces,player);
    }
    /* The saved on-foot body must also clear the restored chassis. Terrain
     * checks do not include this separately registered rigid vehicle. */
    for(i=0;i<physics.sphere_count;i++)for(j=0;j<player->count;j++){
        double distance=0,radius=physics.spheres[i].radius+player->spheres[j].radius-.002;
        for(k=0;k<3;k++){
            double d=(double)record->position[k]-player->position[k];
            for(q=0;q<3;q++)d+=(double)physics.spheres[i].center[q]*record->orientation[q*3+k]-(double)player->spheres[j].center[q]*player->basis[q*3+k];
            distance+=d*d;
        }
        if(distance<radius*radius)return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_vehicle_checkpoint_player_placement(const scene_stream *s,const rf_vehicle_checkpoint *vehicle,
    const rf_player_checkpoint *player,rf_checkpoint_placement *out)
{return scene_driller_checkpoint_seat_prepare(s,vehicle,player,out);}
static int scene_vehicle_checkpoint_player_capture(scene_stream *s,rf_player_checkpoint *player,rf_player_checkpoint_catalog *catalog)
{
    rf_vehicle_checkpoint vehicle;rf_checkpoint_placement placement;rf_geomod_terrain_view view;
    rf_collision_composition_view composed;scene_driller_runtime *r=s->driller_runtime;int status;
    status=scene_checkpoint_player_scope(s,0);if(status)return status;
    if(!r || !r->entry.session.active || !r->player.saved || !r->entry.host.alive ||
       campaign_player_damage.state.effects.health<=0 || campaign_crouched || campaign_climb.region ||
       combat_trigger.cooldown || combat_trigger.remaining || combat_trigger.delay || combat_trigger.held || rf_scene_combat[6] ||
       riot_charge_remainder || rf_scene_rockets[3] || scene_ai_grenade_pending() || scene_ai_rocket_pending() ||
       scene_fusion_projectile_pending() || scene_burning_save_pending() || scene_flame_input_pending() || scene_flame_canister_save_pending())return RF_RANGE;
    status=scene_driller_checkpoint_capture(s,&vehicle);if(status)return status;
    status=scene_checkpoint_player_catalog(catalog);if(status)return status;
    memset(player,0,sizeof(*player));player->player.inventory=campaign_player_inventory;
    player->player.health=campaign_player_damage.state.effects.health;player->player.armor=campaign_player_damage.state.effects.armor;
    player->player.weapon=campaign_explicit_unarmed?UINT32_MAX:(uint32_t)campaign_selected_weapon();
    if(!campaign_player_inventory.owned[campaign_selected_weapon()])player->player.weapon=UINT32_MAX;
    player->player.catalog_hash=rf_scene_weapon_supply[3];memcpy(player->position,scene_actor_body.state.position,12);
    /* Exit look belongs to the driver before boarding; the current body/camera
     * matrices belong to the moving seat. No registry handles enter RFPL. */
    memcpy(player->body_angles,r->player.saved_look.state.body_angles,12);
    memcpy(player->eye_angles,r->player.saved_look.state.eye_angles,12);
    status=rf_player_checkpoint_seated_validate(player,catalog,&vehicle);if(status)return status;
    status=scene_driller_checkpoint_seat_prepare(s,&vehicle,player,&placement);if(status)return status;
    status=scene_terrain_publication_view(s,&view);if(status)return status;
    status=rf_collision_composition_get(s->terrain_publication->composition,&composed);if(status)return status;
    view.faces=composed.tree->faces;view.tree=composed.tree;view.mesh.face_count=composed.count;
    status=rf_checkpoint_placement_check(&view,&placement,NULL);if(status)return status;
    status=s->terrain_source_count>1?scene_collection_player_placement(s,&placement):rf_geomod_piece_registry_placement_check(s->detached_pieces,&placement);
    if(!status){rf_scene_player_checkpoint_state[6]=scene_actor_body.spheres.count;rf_scene_player_checkpoint_state[7]=1;}
    return status;
}
