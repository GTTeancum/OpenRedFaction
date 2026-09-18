/* Typed RFCP3 dispatch. All physical candidates are admitted before publication. */
static int scene_vehicle_checkpoint_capture_record(const scene_stream *s,scene_vehicle_checkpoint_record *out)
{
    scene_vehicle_checkpoint_record r={0};int status;
    r.profile=rf_scene_vehicle_enabled;
    if(r.profile==1)status=scene_driller_checkpoint_capture(s,&r.value.vehicle);
    else if(r.profile==2)status=scene_apc_checkpoint_capture(s,&r.value.apc);
    else if(r.profile==3)status=scene_jeep_checkpoint_capture(s,&r.value.jeep);
    else if(r.profile==5)status=scene_fighter_checkpoint_capture_live(s,&r.value.fighter);
    else return RF_RANGE;
    if(status)return status;*out=r;return RF_OK;
}
static int scene_vehicle_checkpoint_capture(scene_stream *s,void *blob,uint32_t *bytes)
{
    scene_vehicle_checkpoint_record record;int status;
    if(!s||!blob||!bytes)return RF_RANGE;
    *bytes=0;if(!s->driller)return RF_OK;
    if(!s->terrain_authored)return RF_RANGE;
    status=scene_vehicle_checkpoint_capture_record(s,&record);if(status)return status;
    if(record.value.vehicle.player_occupied && !record.value.vehicle.alive)return RF_RANGE;
    if(record.profile==1)status=rf_vehicle_checkpoint_encode(&record.value.vehicle,blob,128);
    else if(record.profile==2)status=rf_apc_checkpoint_encode(&record.value.apc,blob,128);
    else if(record.profile==5)status=rf_fighter_checkpoint_encode(&record.value.fighter,blob,160);
    else status=rf_jeep_checkpoint_encode(&record.value.jeep,blob,160);
    if(!status)*bytes=(record.profile==3 || record.profile==5)?160:128;
    return status;
}
static int scene_vehicle_checkpoint_physics(const scene_stream *s,const rf_vehicle_checkpoint *r,scene_driller_physics *out)
{
    if(rf_scene_vehicle_enabled==1)return scene_driller_physics_initialize(s->driller,r->position,r->orientation,scene_gravity.acceleration,out);
    if(rf_scene_vehicle_enabled==5)return scene_vehicle_body_physics_initialize("Fighter01",
        &s->driller->physics,&s->driller->movement,&s->driller->rotation,&s->driller->render,r->position,r->orientation,0,out);
    return scene_ground_vehicle_physics_initialize(rf_scene_vehicle_enabled==2?"APC":"Jeep01",
        &s->driller->physics,&s->driller->movement,&s->driller->rotation,&s->driller->render,r->position,r->orientation,scene_gravity.acceleration,out);
}
static int scene_vehicle_checkpoint_read(scene_stream *s,const void *blob,uint32_t bytes,scene_vehicle_checkpoint_record *out)
{
    scene_driller_physics physics;scene_driller_checkpoint_candidate staged;scene_vehicle_combat_candidate combat;
    scene_vehicle_checkpoint_record record={0};int status;
    if(!s||!out)return RF_RANGE;
    if(!bytes){if(s->driller)return RF_FORMAT;*out=record;return RF_OK;}
    if(!s->driller||!s->terrain_authored||strcmp(campaign_current_level,"ctf06.rfl"))return RF_FORMAT;
    if(s->driller_runtime && !scene_driller_active(s)){const scene_driller_runtime *r=s->driller_runtime;
        if(!scene_driller_damage_runtime_valid(&r->damage)||r->damage.entry!=&r->entry||r->entry.physics!=&r->physics||
           r->entry.host.driver!=UINT32_MAX||r->entry.occupant!=-1||r->player.saved||r->damage.damage.state.effects.burn||
           r->entry.player_view->linked_handle==(int32_t)r->entry.host.handle)return RF_FORMAT;
    }
    record.profile=rf_scene_vehicle_enabled;
    if(record.profile==1)status=rf_vehicle_checkpoint_decode(blob,bytes,&record.value.vehicle);
    else if(record.profile==2)status=rf_apc_checkpoint_decode(blob,bytes,&record.value.apc);
    else if(record.profile==3)status=rf_jeep_checkpoint_decode(blob,bytes,&record.value.jeep);
    else if(record.profile==5)status=rf_fighter_checkpoint_decode(blob,bytes,&record.value.fighter);
    else return RF_FORMAT;
    if(status)return status;
    if(record.value.vehicle.player_occupied && !record.value.vehicle.alive)return RF_FORMAT;
    status=scene_vehicle_checkpoint_physics(s,&record.value.vehicle,&physics);if(status)return status;
    if(record.profile==1)status=scene_driller_checkpoint_stage_rigid(&physics.state,&record.value.vehicle,&staged);
    else if(record.profile==2)status=scene_apc_checkpoint_stage_rigid(&physics.state,&record.value.apc,&combat);
    else if(record.profile==5)status=scene_vehicle_combat_stage_validated(&physics.state,&record.value.vehicle,&staged);
    else status=scene_jeep_checkpoint_stage_rigid(&physics.state,&record.value.jeep,&combat);
    if(status)return status;
    /* Loading while on foot must reject transient live combat BEFORE the
     * terrain/player transaction publishes anything. Match final admission. */
    if(record.profile>=2 && s->driller_runtime && !scene_driller_active(s)){
        if(record.profile==5)status=scene_fighter_checkpoint_admit_parked(s);
        else {combat.vehicle.player_occupied=0;status=scene_vehicle_combat_parked_admit(s,&combat);}
        if(status)return status;
    }
    *out=record;return RF_OK;
}
static int scene_vehicle_checkpoint_fit(scene_stream *s,scene_authored_collection_stage *stage,scene_authored_checkpoint_stage *single,
    const scene_vehicle_checkpoint_record *saved,const rf_checkpoint_placement *player)
{
    scene_driller_physics physics;uint32_t i,j,k,q;int status;
    const rf_vehicle_checkpoint *record;
    rf_collision_composition_view composition;rf_geomod_terrain_view full;rf_checkpoint_placement host={0};
    const scene_stream *world=stage?&stage->candidate:single?single->scene:NULL;
    const scene_authored_checkpoint_stage *shared=stage?&stage->shared:single;
    if(!s||!world||!shared||!saved||!player||!s->driller)return RF_RANGE;
    record=&saved->value.vehicle;
    status=scene_vehicle_checkpoint_valid(saved);if(status)return status;
    status=scene_vehicle_checkpoint_physics(s,record,&physics);if(status)return status;
    status=rf_collision_composition_pending(world->terrain_publication->composition,&composition);if(status)return status;
    full=shared->publication;full.faces=composition.tree->faces;full.tree=composition.tree;full.mesh.face_count=composition.count;
    host.world=world->collision;host.replaced_room=world->terrain_collision.room;host.query_flags=4;
    host.spheres=physics.spheres;host.count=physics.sphere_count;
    memcpy(host.position,record->position,12);memcpy(host.basis,record->orientation,36);
    if(saved->profile==1)status=stage?scene_driller_checkpoint_placement(stage,&physics,record):scene_driller_checkpoint_placement_single(single,&physics,record);
    else {
        status=rf_checkpoint_placement_check(&full,&host,NULL);if(status)return status;
        status=stage?scene_collection_player_placement(world,&host):rf_geomod_piece_registry_placement_check(single->pieces,&host);
    }
    if(status)return status;
    if(record->player_occupied){
        status=rf_checkpoint_placement_check(&full,player,NULL);if(status)return status;
        return stage?scene_collection_player_placement(world,player):rf_geomod_piece_registry_placement_check(single->pieces,player);
    }
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
static int scene_vehicle_checkpoint_player_placement(const scene_stream *s,const scene_vehicle_checkpoint_record *r,
    const rf_player_checkpoint *player,rf_checkpoint_placement *out,rf_physics_sphere *scratch)
{
    if(r->profile==1)return scene_driller_checkpoint_seat_prepare(s,&r->value.vehicle,player,out);
    if(r->profile==2)return scene_apc_checkpoint_seat_prepare(s,&r->value.apc,player,out,scratch);
    if(r->profile==3)return scene_jeep_checkpoint_seat_prepare(s,&r->value.jeep,player,out,scratch);
    if(r->profile==5)return scene_vehicle_combat_seat_prepare(s,&r->value.fighter.vehicle,player,"Fighter01.v3m","interface_1",NULL,0,out,scratch);
    return RF_FORMAT;
}
static int scene_vehicle_checkpoint_publish(scene_stream *s)
{
    const scene_vehicle_checkpoint_record *saved=&s->vehicle_record;
    const rf_vehicle_checkpoint *v=&saved->value.vehicle;
    scene_vehicle_combat_candidate combat={0};int status;
    status=scene_vehicle_checkpoint_valid(saved);if(status)return status;
    if(saved->profile==1){rf_vehicle_checkpoint parked=*v;parked.player_occupied=0;
        status=scene_driller_checkpoint_apply_parked(s,&parked);
    }else if(saved->profile==5){status=scene_fighter_checkpoint_apply_live(s,&saved->value.fighter);
    }else{
        if(saved->profile==2)status=scene_apc_checkpoint_stage_rigid(&s->driller_runtime->physics.state,&saved->value.apc,&combat);
        else status=scene_jeep_checkpoint_stage_rigid(&s->driller_runtime->physics.state,&saved->value.jeep,&combat);
        if(status)return status;
        combat.vehicle.player_occupied=0;
        status=scene_vehicle_combat_apply_parked(s,&combat);
    }
    if(status)return status;
    if(v->player_occupied){
        memset(&actor_look,0,sizeof(actor_look));
        memcpy(actor_look.state.body_angles,s->player_checkpoint_value.body_angles,12);
        memcpy(actor_look.state.eye_angles,s->player_checkpoint_value.eye_angles,12);
        status=rf_look_update_pose(&actor_look.state,1.0f,scene_step_seconds,&actor_look);if(status)return status;
        s->player_checkpoint_look=0;
        if(saved->profile==3){int32_t seat;
            status=scene_jeep_seat_index(s->driller,saved->value.jeep.role,&seat);if(status)return status;
            s->driller->seat=seat;s->driller_runtime->jeep_seat.role=saved->value.jeep.role;
            s->driller_runtime->jeep_seat.held=1;
            if(saved->value.jeep.role)memcpy(scene_actor_body.state.orientation,saved->value.jeep.aim_reference,36);
        }
        status=saved->profile==1?scene_driller_checkpoint_restore_occupancy(s,v):scene_driller_checkpoint_restore_occupancy_validated(s,v);if(status)return status;
        if(saved->profile==2 || saved->profile==3){s->apc_aim.angles[0]=combat.aim_pitch;s->apc_aim.angles[1]=combat.aim_yaw;
            s->apc_aim.angles[2]=0;s->apc_aim_active=1;}
    }
    if(saved->profile==5)printf("FIGHTER_SAVE_RESTORE ammo%d/%d cooldown%.9g/%.9g shots%u/%u\n",
        s->fighter_weapon.primary_reserve,s->fighter_weapon.rocket_reserve,s->fighter_weapon.primary_fire.cooldown,
        s->fighter_weapon.rocket_fire.cooldown,s->fighter_weapon.primary_fire.shots,s->fighter_weapon.rocket_fire.shots);
    printf("VEHICLE_SAVE_RESTORE profile%u ammo%d/%d role%u aim%.9g,%.9g\n",saved->profile,
        s->apc_primary.reserve,s->apc_secondary.reserve,s->driller_runtime->jeep_seat.role,
        s->apc_aim.angles[0],s->apc_aim.angles[1]);
    return RF_OK;
}
static int scene_vehicle_checkpoint_player_capture(scene_stream *s,rf_player_checkpoint *player,rf_player_checkpoint_catalog *catalog)
{
    scene_vehicle_checkpoint_record vehicle;rf_checkpoint_placement placement;rf_physics_sphere scratch;rf_geomod_terrain_view view;
    rf_collision_composition_view composed;scene_driller_runtime *r=s->driller_runtime;int status;
    status=scene_checkpoint_player_scope(s,0);if(status)return status;
    if(!r || !r->entry.session.active || !r->player.saved || !r->entry.host.alive ||
       campaign_player_damage.state.effects.health<=0 || campaign_crouched || campaign_climb.region ||
       combat_trigger.cooldown || combat_trigger.remaining || combat_trigger.delay || combat_trigger.held || rf_scene_combat[6] ||
       riot_charge_remainder || rf_scene_rockets[3] || scene_apc_primary_pending(s) || scene_apc_secondary_pending(s) || scene_ai_grenade_pending() || scene_ai_rocket_pending() ||
       scene_fusion_projectile_pending() || scene_burning_save_pending() || scene_flame_input_pending() || scene_flame_canister_save_pending())return RF_RANGE;
    status=scene_vehicle_checkpoint_capture_record(s,&vehicle);if(status)return status;
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
    status=rf_player_checkpoint_seated_profile_validate(player,catalog,vehicle.profile,scene_vehicle_checkpoint_typed(&vehicle));if(status)return status;
    status=scene_vehicle_checkpoint_player_placement(s,&vehicle,player,&placement,&scratch);if(status)return status;
    status=scene_terrain_publication_view(s,&view);if(status)return status;
    status=rf_collision_composition_get(s->terrain_publication->composition,&composed);if(status)return status;
    view.faces=composed.tree->faces;view.tree=composed.tree;view.mesh.face_count=composed.count;
    status=rf_checkpoint_placement_check(&view,&placement,NULL);if(status)return status;
    status=s->terrain_source_count>1?scene_collection_player_placement(s,&placement):rf_geomod_piece_registry_placement_check(s->detached_pieces,&placement);
    if(!status){rf_scene_player_checkpoint_state[6]=scene_actor_body.spheres.count;rf_scene_player_checkpoint_state[7]=1;}
    return status;
}
