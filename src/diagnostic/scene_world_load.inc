#ifndef RF_SCENE_WORLD_LOAD_INCLUDED
#define RF_SCENE_WORLD_LOAD_INCLUDED
/* Ordinary loader: pristine world or the admitted L1S2 authored terrain cut,
 * standing/seated player and settled NPCs.
 * Called once after frame-zero initialization, before the next input tick.
 * All gameplay owners validate before assignment; cache demand may remain on
 * an aborted load and is owned by normal scene cleanup. */
uint32_t rf_scene_world_load_reject[3]; /* phase FNV1a, status, staged bytes */
static int scene_world_load(scene_stream *stream,const rf_level *level,const char *tables_path)
{
    extern rf_entity_room_state rf_scene_actor_room_state;
#ifndef RF_IMAGE_XBOX_NATIVE
    const char *path=scene_live_load_active?"redfaction-save":getenv("RF_REPLAY_WORLD_SNAPSHOT_IN");
#else
    rf_xbox_checkpoint_storage storage={0};uint32_t enabled=0,fixture=0;
    const char *path=RF_XBOX_WORLD_CHECKPOINT_BASE;
#endif
    scene_world_snapshot_identity identity;rf_checkpoint_file_selection selection={0};rf_world_checkpoint envelope;
    unsigned char *buffer=NULL;uint32_t bytes=0,i,rows_count=0,added=0,flags=0,selected=0,cost=0;int status=RF_OK,found=0;
    const uint32_t budget=2u*1024u*1024u;const char *phase="identity";
    if(!scene_live_load_active && scene_world_startup_snapshot_consumed)return RF_OK;
    if(scene_vehicle_switch_has_parked())return RF_NOT_FOUND;
    rf_npc_checkpoint_catalog catalog={0};rf_npc_checkpoint_record *rows=NULL;
    scene_world_restore_stage *world=NULL;scene_world_event_stage *events=NULL;scene_world_mission_stage *mission=NULL;
    scene_authored_checkpoint_stage *terrain_stage=NULL;
    scene_authored_collection_stage *collection_stage=NULL;
    scene_world_environment_stage *environment=NULL;scene_campaign_history_checkpoint_stage *history=NULL;
    scene_campaign_history_rebind_stage *bindings=NULL;scene_world_player_stage player;
    scene_ai_projectile_checkpoint_stage *projectiles=NULL;
    scene_passive_vehicle_checkpoint_stage *passive=NULL;
    scene_vehicle_physics_checkpoint_stage *vehicle_physics=NULL;
    scene_vehicle_switch_checkpoint_stage *vehicle_switch=NULL;
    scene_turret_checkpoint_stage *turrets=NULL;
    scene_turret_generated_checkpoint_stage *generated=NULL;
    scene_npc_seat_checkpoint_stage *seats=NULL;
    scene_jeep_gunner_checkpoint_stage *gunner=NULL;
    scene_corpse_lifetime_stage *corpses=NULL;
    scene_turret_player_checkpoint_stage *turret_player=NULL;
    scene_vehicle_checkpoint_record vehicle={0};scene_campaign_vehicle_route vehicle_route={0};
    uint32_t vehicle_present=0,vehicle_base=0,vehicle_bytes=0,authored_terrain=0,authored_cuts=0;
    const unsigned char *vehicle_data=NULL;
    scene_checkpoint_world_context context={0};rf_geometry_materials mapping={0};rf_geometry_body_surfaces surfaces;
    float modified_min[3]={0},modified_max[3]={0};
    const rf_world_checkpoint_slice *v;
#ifdef RF_IMAGE_XBOX_NATIVE
    if(!scene_live_load_active){
        status=scene_world_storage_flag("D:\\world-fixture-load.flag",&fixture);if(status)return status;
        if(!fixture){status=scene_world_storage_flag("D:\\world-hdd-load.flag",&enabled);if(status||!enabled)return status;}
    }
#endif
    if(!path||!*path)return RF_OK;
    rf_scene_world_checkpoint_state[8]=1;
    if(scene_turret_player_active()||scene_player_jeep_gunner_active(stream)){status=RF_RANGE;goto done;}
    if(!stream||!level||!tables_path||!stream->collision||
       (stream->terrain&&!scene_world_l1s2_authored_terrain(stream))||
       (!stream->terrain&&(stream->terrain_authored||stream->terrain_publication))||
       stream->terrain_history_count||stream->terrain_publication_serial){status=RF_RANGE;goto done;}
    status=scene_world_checkpoint_source_identity(level,tables_path,rf_scene_weapon_supply[3],identity.bytes);if(status)goto done;
    if(strlen(campaign_current_level)>=64){status=RF_FORMAT;goto done;}
    memset(identity.level,0,64);strcpy(identity.level,campaign_current_level);
    buffer=malloc(RF_CHECKPOINT_FILE_MAX);if(!buffer){status=RF_IO;goto done;}
    phase="file";
#ifndef RF_IMAGE_XBOX_NATIVE
    status=rf_checkpoint_file_load(path,buffer,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity,&selection);
#else
    (void)path;
    if(fixture)status=rf_checkpoint_file_load(SCENE_WORLD_FIXTURE_BASE,buffer,RF_CHECKPOINT_FILE_MAX,&bytes,
        scene_world_snapshot_validate,&identity,&selection);
    else {status=scene_live_load_active&&scene_live_load_auto_source?
        rf_xbox_checkpoint_storage_open_world_auto(&storage,0):rf_xbox_checkpoint_storage_open_world(&storage,0);
        if(!status)status=rf_xbox_checkpoint_storage_load(&storage,buffer,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity);}
#endif
    if(status)goto done;
    status=rf_world_checkpoint_decode(buffer,bytes,RF_WORLD_CHECKPOINT_MASK(RF_WORLD_ENVIRONMENT),&envelope);if(status)goto done;
    if(envelope.sections[RF_WORLD_SWITCHES-1].bytes){status=RF_NOT_FOUND;goto done;}
    phase="vehicle_switch";v=envelope.sections+RF_WORLD_VEHICLE-1;
    status=scene_vehicle_switch_checkpoint_prepare(stream,v->data,v->bytes,budget-cost,&vehicle_switch,&vehicle_data,&vehicle_bytes);if(status)goto done;
    cost+=scene_vehicle_switch_checkpoint_bytes(vehicle_switch);
    phase="corpse_lifetime";
    status=scene_corpse_lifetime_checkpoint_prepare(vehicle_data,vehicle_bytes,budget-cost,&corpses,&vehicle_data,&vehicle_bytes);if(status)goto done;
    if(corpses)cost+=corpses->allocated_bytes;
    if(cost>budget){status=RF_RANGE;goto done;}
    phase="turret_player";
    status=scene_turret_player_checkpoint_prepare(vehicle_data,vehicle_bytes,budget-cost,&turret_player,&vehicle_data,&vehicle_bytes);if(status)goto done;
    if(turret_player)cost+=turret_player->allocated_bytes;
    if(cost>budget){status=RF_RANGE;goto done;}
    phase="npc_resources";catalog.hash=rf_scene_weapon_supply[3];catalog.count=campaign_weapon_supply.names.count;
    if(!catalog.count||catalog.count>64){status=RF_RANGE;goto done;}
    memcpy(catalog.weapons,campaign_weapon_supply.definitions,sizeof(catalog.weapons));
    for(i=0;i<catalog.count;i++){const rf_weapon_acquire_definition *d=catalog.weapons+i;
        catalog.supported[i]=(uint8_t)(d->ammo_type>=-1&&d->ammo_type<32&&d->capacity>=0&&d->magazine>=0&&(d->ammo_type!=-1||d->magazine==0));}
    v=envelope.sections+RF_WORLD_NPC-1;status=rf_npc_checkpoint_preflight(v->data,v->bytes,identity.bytes,&catalog,&rows_count);if(status)goto done;
    if(rows_count){rows=malloc(rows_count*sizeof(*rows));if(!rows){status=RF_IO;goto done;}}
    status=rf_npc_checkpoint_decode(v->data,v->bytes,identity.bytes,&catalog,rows,rows_count,&rows_count);if(status)goto done;
    status=scene_corpse_lifetime_checkpoint_admit(corpses,rows,rows_count);if(status)goto done;
    status=scene_npc_checkpoint_resources_require(rows,rows_count,budget-cost,&added);if(status)goto done;free(rows);rows=NULL;
    v=envelope.sections+RF_WORLD_DESTRUCTION-1;
    if(v->bytes>=4 && !memcmp(v->data,"RFDS",4)){
        rf_geomod_terrain_view edited;
        if(!scene_world_l1s2_authored_terrain(stream)){status=RF_FORMAT;goto done;}
        phase="authored_terrain";
        if(stream->terrain_source_count>1) {
            status=scene_authored_collection_stage_prepare(stream,v->data,v->bytes,&collection_stage);
            if(status)goto done;
            authored_cuts=collection_stage->shared.cuts;
            status=scene_authored_collection_stage_commit(collection_stage,NULL);if(status)goto done;
            scene_authored_collection_stage_discard(&collection_stage);
        } else {
            status=scene_authored_checkpoint_stage_prepare(stream,v->data,v->bytes,NULL,&terrain_stage);
            if(status)goto done;
            authored_cuts=terrain_stage->cuts;
            status=scene_authored_checkpoint_stage_commit(terrain_stage);if(status)goto done;
            scene_authored_checkpoint_stage_discard(&terrain_stage);
        }
        authored_terrain=1;
        status=scene_terrain_publication_view(stream,&edited);if(status)goto done;
        for(i=0;i<edited.mesh.vertex_count;i++)for(uint32_t axis=0;axis<3;axis++){
            float coordinate=edited.mesh.vertices[i].position[axis];
            if(!i){modified_min[axis]=modified_max[axis]=coordinate;}
            else {if(coordinate<modified_min[axis])modified_min[axis]=coordinate;
                  if(coordinate>modified_max[axis])modified_max[axis]=coordinate;}
        }
        if(authored_cuts && !edited.mesh.vertex_count){status=RF_FORMAT;goto done;}
    }else if(v->bytes){phase="destruction";
        if(stream->terrain){status=RF_FORMAT;goto done;}
        status=scene_campaign_geomod_checkpoint_restore(stream,v->data,v->bytes);if(status)goto done;
        {const rf_geomod_mesh_view *edited[3]={
            stream->campaign_geomod && stream->campaign_geomod->active?&stream->campaign_geomod->mesh:NULL,
            stream->campaign_wall && stream->campaign_wall->active?&stream->campaign_wall->rooms[0].mesh:NULL,
            stream->campaign_wall && stream->campaign_wall->active?&stream->campaign_wall->rooms[1].mesh:NULL};
         uint32_t side,found_vertex=0;
         for(side=0;side<3;side++)if(edited[side])for(uint32_t vertex=0;vertex<edited[side]->vertex_count;vertex++)for(i=0;i<3;i++){
             float coordinate=edited[side]->vertices[vertex].position[i];
             if(!found_vertex){modified_min[i]=modified_max[i]=coordinate;}
             else {if(coordinate<modified_min[i])modified_min[i]=coordinate;
                   if(coordinate>modified_max[i])modified_max[i]=coordinate;}
             if(i==2)found_vertex=1;
         }
         if(!found_vertex){status=RF_FORMAT;goto done;}
        }
    }
    mapping.offsets=actor_follow_world->offsets;mapping.slots=actor_follow_world->slots;mapping.count=actor_follow_world->geometry_count;mapping.textures.count=actor_follow_world->material_count;
    surfaces.geometries=campaign_surface_sources;surfaces.count=mapping.count;surfaces.mapping=&mapping;surfaces.palette=campaign_surface_palette;
    context.world=stream->collision;context.movers=&campaign_movers;context.metadata=campaign_body_surface;context.metadata_context=&surfaces;
    context.dt=1.0f/60;context.authored_static_unchanged=!v->bytes||(authored_terrain&&!authored_cuts);context.fresh_boot=1;
    if(v->bytes&&(!authored_terrain||authored_cuts)){context.modified_min=modified_min;context.modified_max=modified_max;}
    if(authored_terrain&&authored_cuts)context.modified_rooms[context.modified_room_count++]=stream->terrain_authored->asset_view.room;
    if(stream->campaign_geomod&&stream->campaign_geomod->active)context.modified_rooms[context.modified_room_count++]=28;
    if(stream->campaign_wall&&stream->campaign_wall->active){
        context.modified_rooms[context.modified_room_count++]=stream->campaign_wall->overlay[0].room;
        context.modified_rooms[context.modified_room_count++]=stream->campaign_wall->overlay[1].room;
    }
    if(cost>budget){status=RF_RANGE;goto done;}
    status=scene_turret_retirement_checkpoint_prepare(vehicle_data,vehicle_bytes,0,budget-cost,&generated,&vehicle_data,&vehicle_bytes);if(status)goto done;
    if(generated)cost+=generated->allocated_bytes;
    if(cost>budget){status=RF_RANGE;goto done;}
    status=scene_turret_checkpoint_prepare(vehicle_data,vehicle_bytes,0,budget-cost,&turrets,&vehicle_data,&vehicle_bytes);if(status)goto done;
    if(turrets)cost+=turrets->allocated_bytes;
    status=scene_npc_seat_checkpoint_prepare(vehicle_data,vehicle_bytes,budget-cost,&seats,&vehicle_data,&vehicle_bytes);if(status)goto done;
    cost+=scene_npc_seat_checkpoint_bytes(seats);
    if(cost>budget){status=RF_RANGE;goto done;}
    phase="vehicle_physics";
    status=scene_vehicle_physics_checkpoint_prepare(stream,vehicle_data,vehicle_bytes,budget-cost,&vehicle_physics,&vehicle_data,&vehicle_bytes);if(status)goto done;
    cost+=scene_vehicle_physics_checkpoint_bytes(vehicle_physics);
    /* Old passive-only saves cannot silently become a different active owner. */
    if(campaign_vehicle_selection_fallback && (!vehicle_bytes ||
       (vehicle_bytes>=SCENE_PASSIVE_VEHICLE_HEADER && !memcmp(vehicle_data,"RFVA",4) && !scene_history_word(vehicle_data+8)))){
        phase="vehicle_selection_legacy";status=RF_NOT_FOUND;goto done;
    }
    if(vehicle_bytes>=4&&!memcmp(vehicle_data,"RFVA",4)){
        phase="passive_vehicle";
        status=scene_passive_vehicle_checkpoint_prepare(vehicle_data,vehicle_bytes,budget-cost,&passive);
        if(status)goto done;cost+=passive->allocated_bytes;
        vehicle_data+=SCENE_PASSIVE_VEHICLE_HEADER;vehicle_bytes=passive->host_bytes;
    }else if(campaign_passive_vehicle_count){status=RF_FORMAT;goto done;}
    vehicle_present=vehicle_bytes!=0;
    if(vehicle_present){
        vehicle_base=(rf_scene_vehicle_enabled==3||rf_scene_vehicle_enabled==5)?160u:128u;
        if(vehicle_bytes<vehicle_base){status=RF_FORMAT;goto done;}
        phase="vehicle_read";status=scene_vehicle_checkpoint_read(stream,vehicle_data,vehicle_base,&vehicle);if(status)goto done;
        phase="vehicle_route";status=scene_world_vehicle_route_read(vehicle_data,vehicle_bytes,vehicle_base,&vehicle_route);if(status)goto done;
        if(vehicle_route.active&&!vehicle.value.vehicle.alive){status=RF_FORMAT;goto done;}
    }
    status=scene_vehicle_physics_checkpoint_admit(stream,vehicle_physics,vehicle_present?&vehicle:NULL,passive);if(status)goto done;
    status=scene_vehicle_switch_checkpoint_admit(stream,vehicle_switch,vehicle_present?&vehicle:NULL,passive,vehicle_physics);if(status)goto done;
    status=scene_npc_seat_checkpoint_hosts(seats,turrets,vehicle_present?&vehicle:NULL,
        scene_vehicle_physics_checkpoint_active_hidden(vehicle_physics),vehicle_switch,passive);if(status)goto done;
    phase="world";status=scene_world_restore_prepare_with_passive(buffer,bytes,identity.bytes,0,&catalog,0,&context,budget-cost,&world,stream,passive,seats);if(status)goto done;cost+=world->allocated_bytes;
    phase="environment";v=envelope.sections+RF_WORLD_ENVIRONMENT-1;
    status=scene_world_environment_prepare(identity.bytes,16,v->data,v->bytes,budget-cost,&environment);if(status)goto done;cost+=environment->bytes;
    phase="player";status=scene_world_player_prepare(stream,world,environment->support_uid,
        environment->support_point_present?environment->support_point_velocity:NULL,
        vehicle_present?&vehicle:NULL,passive,&player);if(status)goto done;
    if(vehicle_present){phase="vehicle";status=scene_world_vehicle_prepare(stream,world,&player,&vehicle,seats,
        scene_vehicle_physics_checkpoint_active_hidden(vehicle_physics));if(status)goto done;}
    if(player.player.player.weapon==UINT32_MAX)found=1;
    else for(i=0;i<scene_weapon_slots();i++)if(scene_weapon_available(i)&&player.player.player.weapon==(uint32_t)campaign_slot_weapon(i)){
        selected=i;found=1;break;}
    if(!found||(player.player.player.weapon!=UINT32_MAX&&!stream->player_weapon[selected])){status=RF_NOT_FOUND;goto done;}
    phase="form";
    if(environment->form_next.active &&
       (campaign_extra_ids[3]<0||campaign_extra_ids[3]>=64||!scene_weapon_available(16)||
        !stream->player_weapon[16]||!player.player.player.inventory.owned[campaign_extra_ids[3]])){
        status=RF_NOT_FOUND;goto done;
    }
    phase="mission";status=scene_world_mission_prepare(stream,&envelope,0,budget-cost,&mission);if(status)goto done;cost+=mission->bytes;
    phase="history";v=envelope.sections+RF_WORLD_CAMPAIGN_HISTORY-1;
    status=scene_campaign_history_checkpoint_prepare(identity.bytes,v->data,v->bytes,budget-cost,&history);if(status)goto done;cost+=(uint32_t)sizeof(*history);
    status=scene_campaign_history_checkpoint_rebind_prepare(history,budget-cost,&bindings);if(status)goto done;cost+=bindings->bytes;
    phase="events";status=scene_world_event_prepare(stream,&envelope,0,world,budget-cost,&events,&flags);if(status)goto done;cost+=events->bytes;
    if(flags&(SCENE_WORLD_EVENT_PUBLICATION_BLOCKERS&~(SCENE_WORLD_EVENT_MISSING_GRAVITY|SCENE_WORLD_EVENT_MISSING_FORCES|SCENE_WORLD_EVENT_NEEDS_PLAYER_MISSION))){status=RF_NOT_FOUND;goto done;}
    phase="projectiles";v=envelope.sections+RF_WORLD_PROJECTILES-1;
    if(v->bytes){status=scene_ai_projectile_checkpoint_prepare(stream,v->data,v->bytes,budget-cost,&projectiles);
        if(status)goto done;cost+=scene_ai_projectile_checkpoint_stage_bytes();
        if(scene_ai_projectile_checkpoint_trigger_pending(projectiles)&&
           player.player.player.weapon!=(uint32_t)campaign_rocket_id){status=RF_FORMAT;goto done;}}
    status=scene_ai_projectile_checkpoint_fusion_admit(projectiles,&player.player.player);if(status)goto done;
    status=scene_ai_projectile_checkpoint_flame_admit(projectiles,&player.player.player);if(status)goto done;
    status=scene_ai_projectile_checkpoint_burning_admit(projectiles,world->npcs);if(status)goto done;
    phase="remote";v=envelope.sections+RF_WORLD_REMOTE-1;
    status=scene_remote_checkpoint_world_preflight(stream,v->data,v->bytes,world);if(status)goto done;
    phase="validate";
    status=scene_npc_checkpoint_restore_validate(world->npcs);if(status)goto done;
    status=scene_mover_checkpoint_validate(&campaign_group_runtime,&campaign_group_registration,campaign_controller_views,&campaign_movers,&world->movers);if(status)goto done;
    status=scene_passive_vehicle_checkpoint_validate(passive);if(status)goto done;
    status=scene_turret_checkpoint_validate(turrets);if(status)goto done;
    status=scene_turret_attack_checkpoint_admit(world->npcs,turrets);if(status)goto done;
    status=scene_npc_seat_checkpoint_admit(seats,world->npcs);if(status)goto done;
    phase="passive_seat";
    status=scene_passive_jeep_seat_restore_admit(stream,world,&player,vehicle_present?&vehicle:NULL,
        seats,vehicle_switch,passive,scene_vehicle_physics_checkpoint_active_hidden(vehicle_physics));if(status)goto done;
    status=scene_passive_jeep_seat_restore_audit(seats,vehicle_switch,passive);if(status)goto done;
    phase="turret_player";status=scene_turret_player_checkpoint_admit(turret_player,turrets,&player,0);if(status)goto done;
    phase="gunner";if(cost>budget){status=RF_RANGE;goto done;}
    status=scene_jeep_gunner_checkpoint_prepare(stream,seats,vehicle_present?&vehicle:NULL,&player,budget-cost,&gunner);
    if(status)goto done;cost+=scene_jeep_gunner_checkpoint_bytes(gunner);
    status=scene_turret_generated_world_admit(generated,world->npcs,&player,
        vehicle_present?&vehicle:NULL,(uint32_t)campaign_authored_vehicle_uid,passive,turrets);if(status)goto done;
    status=scene_turret_generated_attack_admit(world->npcs,generated);if(status)goto done;
    status=scene_clutter_checkpoint_targets_valid(world->clutter);if(status)goto done;
    status=campaign_group_control_children_validate(world->controls);if(status)goto done;
    status=scene_event_checkpoint_validate(&events->events);if(status)goto done;
    status=scene_world_mission_validate(mission);if(status)goto done;
    status=scene_campaign_history_checkpoint_rebind_validate(bindings);if(status)goto done;
    status=scene_world_environment_validate(environment);if(status)goto done;
    /* Parked publication has no callbacks or mutation on rejection. Admit it
     * before the assignment-only world transaction below. */
    if(vehicle_present && !vehicle.value.vehicle.player_occupied){phase="vehicle_publish";stream->vehicle_record=vehicle;
        status=scene_vehicle_checkpoint_publish(stream);if(status)goto done;}
    /* Publish validated world owners before restoring the occupied vehicle:
     * driver possession needs the saved player body and health in place. */
    scene_npc_seat_checkpoint_detach(seats);
    scene_jeep_gunner_checkpoint_host_assign(stream,gunner);
    scene_mover_checkpoint_assign(&campaign_group_runtime,&campaign_movers,&world->movers);
    scene_passive_vehicle_checkpoint_assign(passive);
    scene_clutter_checkpoint_assign(world->clutter);scene_npc_checkpoint_restore_assign(world->npcs);
    scene_corpse_lifetime_checkpoint_assign(corpses);
    scene_turret_checkpoint_assign(turrets);
    scene_turret_retirement_checkpoint_assign(generated);
    scene_turret_generated_activation_reset();
    scene_npc_seat_checkpoint_assign(seats);
    scene_turret_death_effects_reset_pending();
    scene_event_checkpoint_assign(&events->events);scene_world_mission_assign(mission);
    campaign_group_control_children_assign(world->controls);
    if(vehicle_present){scene_world_vehicle_route_assign(&vehicle_route);campaign_vehicle_mode_publish(stream);}
    scene_campaign_history_checkpoint_assign_rebound(history,bindings);scene_world_environment_assign(environment);
    scene_remote_checkpoint_publish();
    scene_actor_body.state=player.body;rf_scene_actor_room_state=player.room;
    memcpy(actor_look.state.body_angles,player.player.body_angles,12);memcpy(actor_look.state.eye_angles,player.player.eye_angles,12);
    memset(actor_look.state.command,0,12);memset(actor_look.state.angular_velocity,0,12);actor_look.state.pending_pitch=actor_look.state.pending_yaw=0;
    memcpy(campaign_position,player.player.position,12);memcpy(campaign_orientation,player.eye,36);
    campaign_player_inventory=player.player.player.inventory;campaign_player_damage.state.effects.health=player.player.player.health;
    campaign_player_damage.state.effects.armor=player.player.player.armor;campaign_select_primary(selected);
    campaign_explicit_unarmed=player.player.player.weapon==UINT32_MAX;
    scene_remote_checkpoint_world_selection();
    scene_weapon_modes_checkpoint_assign(stream,&player.modes);
    scene_ai_projectile_checkpoint_assign(stream,projectiles);
    campaign_player_form=campaign_player_form_carry=environment->form_next;
    memset(rf_scene_player_form,0,sizeof(rf_scene_player_form));
    if(campaign_player_form.active){
        campaign_player_damage.state.effects.class_armor=0;
        campaign_player_view.flags_810&=~0x40u;
        campaign_player_view.flags_7d0|=0x10000u;
        campaign_player_damage.state.effects.flags_810=campaign_player_view.flags_810;
        rf_scene_player_form[0]=1;rf_scene_player_form[1]=campaign_player_form.variant;
        rf_scene_player_form[2]=campaign_player_form.compromised;
        rf_scene_player_form[5]=(uint32_t)campaign_extra_ids[3];
        memcpy(rf_scene_player_form+6,&campaign_player_damage.state.effects.armor,4);
    }
    campaign_ammo_publish();
    if(gunner)scene_jeep_gunner_checkpoint_player_assign(stream,gunner);
    else if(vehicle_present && vehicle.value.vehicle.player_occupied){
        phase="vehicle_publish";stream->vehicle_record=vehicle;stream->player_checkpoint_value=player.player;
        status=scene_vehicle_checkpoint_publish(stream);if(status)goto done;
    }
    scene_vehicle_physics_checkpoint_assign(stream,vehicle_physics);
    scene_vehicle_switch_checkpoint_assign(stream,vehicle_switch);
    campaign_support_handle=player.support.handle;memcpy(campaign_support_velocity,player.support_velocity,12);
    campaign_player_support_epoch=0; /* No pre-load chassis interval may authorize a new capture. */
    rf_scene_actor_landing[1]=1;rf_scene_actor_landing[2]=0;
    scene_actor_body.state.flags&=~(0x200000u|0x400000u);
    if(campaign_support_handle)scene_actor_body.state.flags|=0x400000u;
    if(campaign_support_handle)printf("WORLD_PLAYER_SUPPORT_RESTORED uid%u handle%u velocity%.9g,%.9g,%.9g\n",
        environment->support_uid,campaign_support_handle,(double)campaign_support_velocity[0],
        (double)campaign_support_velocity[1],(double)campaign_support_velocity[2]);
    scene_turret_player_checkpoint_assign(turret_player);
    scene_player_impact_relocated();
    printf("WORLD_SNAPSHOT_LOADED bytes%u staged%u npc%u events%u added_motion%u health%.9g armor%.9g weapon%u\n",bytes,cost,world->npcs->count,events->events.count,added,
        (double)player.player.player.health,(double)player.player.player.armor,player.player.player.weapon);
    scene_world_startup_snapshot_consumed=1;
 done:
    if(status){const unsigned char *name=(const unsigned char *)phase;uint32_t hash=2166136261u;
        while(*name)hash=(hash^*name++)*16777619u;
        rf_scene_world_load_reject[0]=hash;rf_scene_world_load_reject[1]=(uint32_t)status;
        rf_scene_world_load_reject[2]=cost;}
    rf_scene_world_checkpoint_state[0]=(uint32_t)status;rf_scene_world_checkpoint_state[1]=status?0:bytes;
    rf_scene_world_checkpoint_state[2]=status?0:cost;
#ifdef RF_IMAGE_XBOX_NATIVE
    {int closed=scene_world_storage_close(&storage);if(!status)status=closed;rf_scene_world_checkpoint_state[0]=(uint32_t)status;}
#endif
    if(status)printf("WORLD_SNAPSHOT_LOAD_REJECT phase%s status%d\n",phase,status);
    scene_authored_checkpoint_stage_discard(&terrain_stage);
    scene_authored_collection_stage_discard(&collection_stage);
    scene_remote_checkpoint_discard();scene_world_event_close(&events);scene_world_environment_close(&environment);
    scene_campaign_history_checkpoint_rebind_close(&bindings);scene_campaign_history_checkpoint_close(&history);
    scene_ai_projectile_checkpoint_discard(&projectiles);
    scene_jeep_gunner_checkpoint_close(&gunner);
    scene_corpse_lifetime_checkpoint_close(&corpses);
    scene_turret_player_checkpoint_close(&turret_player);
    scene_vehicle_physics_checkpoint_close(&vehicle_physics);
    scene_vehicle_switch_checkpoint_close(&vehicle_switch);
    scene_passive_vehicle_checkpoint_close(&passive);scene_turret_checkpoint_close(&turrets);scene_turret_generated_checkpoint_close(&generated);scene_npc_seat_checkpoint_close(&seats);
    scene_world_mission_close(&mission);scene_world_restore_discard(&world);free(rows);free(buffer);return status;
}
#endif
