#ifndef RF_SCENE_WORLD_LOAD_INCLUDED
#define RF_SCENE_WORLD_LOAD_INCLUDED
/* First ordinary loader: pristine static world, standing player, settled NPCs.
 * Called once after frame-zero initialization, before the next input tick.
 * All gameplay owners validate before assignment; cache demand may remain on
 * an aborted load and is owned by normal scene cleanup. */
static int scene_world_load(scene_stream *stream,const rf_level *level,const char *tables_path)
{
    extern rf_entity_room_state rf_scene_actor_room_state;
#ifndef RF_IMAGE_XBOX_NATIVE
    const char *path=scene_live_load_active?"redfaction-save":getenv("RF_REPLAY_WORLD_SNAPSHOT_IN");
#else
    rf_xbox_checkpoint_storage storage={0};uint32_t enabled=0;
    const char *path=RF_XBOX_WORLD_CHECKPOINT_BASE;
#endif
    scene_world_snapshot_identity identity;rf_checkpoint_file_selection selection={0};rf_world_checkpoint envelope;
    unsigned char *buffer=NULL;uint32_t bytes=0,i,rows_count=0,added=0,flags=0,selected=0,cost=0;int status=RF_OK,found=0;
    const uint32_t budget=2u*1024u*1024u;const char *phase="identity";
    if(!scene_live_load_active && scene_world_startup_snapshot_consumed)return RF_OK;
    rf_npc_checkpoint_catalog catalog={0};rf_npc_checkpoint_record *rows=NULL;
    scene_world_restore_stage *world=NULL;scene_world_event_stage *events=NULL;scene_world_mission_stage *mission=NULL;
    scene_world_environment_stage *environment=NULL;scene_campaign_history_checkpoint_stage *history=NULL;
    scene_campaign_history_rebind_stage *bindings=NULL;scene_world_player_stage player;
    scene_checkpoint_world_context context={0};rf_geometry_materials mapping={0};rf_geometry_body_surfaces surfaces;
    const rf_world_checkpoint_slice *v;
#ifdef RF_IMAGE_XBOX_NATIVE
    if(!scene_live_load_active){status=scene_world_storage_flag("D:\\world-hdd-load.flag",&enabled);if(status||!enabled)return status;}
#endif
    if(!path||!*path)return RF_OK;
    rf_scene_world_checkpoint_state[8]=1;
    if(!stream||!level||!tables_path||!stream->collision||stream->terrain||stream->terrain_authored||stream->terrain_publication||
       stream->terrain_history_count||stream->terrain_publication_serial){status=RF_RANGE;goto done;}
    status=scene_world_checkpoint_source_identity(level,tables_path,rf_scene_weapon_supply[3],identity.bytes);if(status)goto done;
    if(strlen(campaign_current_level)>=64){status=RF_FORMAT;goto done;}
    memset(identity.level,0,64);strcpy(identity.level,campaign_current_level);
    buffer=malloc(RF_CHECKPOINT_FILE_MAX);if(!buffer){status=RF_IO;goto done;}
    phase="file";
#ifndef RF_IMAGE_XBOX_NATIVE
    status=rf_checkpoint_file_load(path,buffer,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity,&selection);
#else
    (void)path;(void)selection;status=rf_xbox_checkpoint_storage_open_world(&storage,0);
    if(!status)status=rf_xbox_checkpoint_storage_load(&storage,buffer,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity);
#endif
    if(status)goto done;
    status=rf_world_checkpoint_decode(buffer,bytes,RF_WORLD_CHECKPOINT_MASK(RF_WORLD_ENVIRONMENT),&envelope);if(status)goto done;
    if(envelope.sections[RF_WORLD_DESTRUCTION-1].bytes||envelope.sections[RF_WORLD_VEHICLE-1].bytes||envelope.sections[RF_WORLD_SWITCHES-1].bytes){status=RF_NOT_FOUND;goto done;}
    phase="npc_resources";catalog.hash=rf_scene_weapon_supply[3];catalog.count=campaign_weapon_supply.names.count;
    if(!catalog.count||catalog.count>64){status=RF_RANGE;goto done;}
    memcpy(catalog.weapons,campaign_weapon_supply.definitions,sizeof(catalog.weapons));
    for(i=0;i<catalog.count;i++){const rf_weapon_acquire_definition *d=catalog.weapons+i;
        catalog.supported[i]=(uint8_t)(d->ammo_type>=-1&&d->ammo_type<32&&d->capacity>=0&&d->magazine>=0&&(d->ammo_type!=-1||d->magazine==0));}
    v=envelope.sections+RF_WORLD_NPC-1;status=rf_npc_checkpoint_preflight(v->data,v->bytes,identity.bytes,&catalog,&rows_count);if(status)goto done;
    if(rows_count){rows=malloc(rows_count*sizeof(*rows));if(!rows){status=RF_IO;goto done;}}
    status=rf_npc_checkpoint_decode(v->data,v->bytes,identity.bytes,&catalog,rows,rows_count,&rows_count);if(status)goto done;
    status=scene_npc_checkpoint_resources_require(rows,rows_count,budget,&added);if(status)goto done;free(rows);rows=NULL;
    mapping.offsets=actor_follow_world->offsets;mapping.slots=actor_follow_world->slots;mapping.count=actor_follow_world->geometry_count;mapping.textures.count=actor_follow_world->material_count;
    surfaces.geometries=campaign_surface_sources;surfaces.count=mapping.count;surfaces.mapping=&mapping;surfaces.palette=campaign_surface_palette;
    context.world=stream->collision;context.movers=&campaign_movers;context.metadata=campaign_body_surface;context.metadata_context=&surfaces;
    context.dt=1.0f/60;context.authored_static_unchanged=1;context.fresh_boot=1;
    phase="world";status=scene_world_restore_prepare(buffer,bytes,identity.bytes,0,&catalog,0,&context,budget,&world);if(status)goto done;cost=world->allocated_bytes;
    phase="player";status=scene_world_player_prepare(stream,world,&player);if(status)goto done;
    if(player.player.player.weapon==UINT32_MAX)found=1;
    else for(i=0;i<scene_weapon_slots();i++)if(scene_weapon_available(i)&&player.player.player.weapon==(uint32_t)campaign_slot_weapon(i)){
        selected=i;found=1;break;}
    if(!found||(player.player.player.weapon!=UINT32_MAX&&!stream->player_weapon[selected])){status=RF_NOT_FOUND;goto done;}
    phase="mission";status=scene_world_mission_prepare(stream,&envelope,0,budget-cost,&mission);if(status)goto done;cost+=mission->bytes;
    phase="history";v=envelope.sections+RF_WORLD_CAMPAIGN_HISTORY-1;
    status=scene_campaign_history_checkpoint_prepare(identity.bytes,v->data,v->bytes,budget-cost,&history);if(status)goto done;cost+=(uint32_t)sizeof(*history);
    status=scene_campaign_history_checkpoint_rebind_prepare(history,budget-cost,&bindings);if(status)goto done;cost+=bindings->bytes;
    phase="environment";v=envelope.sections+RF_WORLD_ENVIRONMENT-1;
    status=scene_world_environment_prepare(identity.bytes,16,v->data,v->bytes,budget-cost,&environment);if(status)goto done;cost+=environment->bytes;
    phase="events";status=scene_world_event_prepare(stream,&envelope,0,world,budget-cost,&events,&flags);if(status)goto done;cost+=events->bytes;
    if(flags&(SCENE_WORLD_EVENT_PUBLICATION_BLOCKERS&~(SCENE_WORLD_EVENT_MISSING_GRAVITY|SCENE_WORLD_EVENT_MISSING_FORCES|SCENE_WORLD_EVENT_NEEDS_PLAYER_MISSION))){status=RF_NOT_FOUND;goto done;}
    phase="remote";v=envelope.sections+RF_WORLD_REMOTE-1;
    /* Active remote charges need candidate-world attachment admission. */
    if(v->bytes!=RF_REMOTE_CHECKPOINT_HEADER){status=RF_NOT_FOUND;goto done;}
    status=scene_remote_checkpoint_preflight(stream,v->data,v->bytes);if(status)goto done;
    phase="validate";
    status=scene_npc_checkpoint_restore_validate(world->npcs);if(status)goto done;
    status=scene_mover_checkpoint_validate(&campaign_group_runtime,&campaign_group_registration,campaign_controller_views,&campaign_movers,&world->movers);if(status)goto done;
    status=scene_clutter_checkpoint_targets_valid(world->clutter);if(status)goto done;
    status=scene_event_checkpoint_validate(&events->events);if(status)goto done;
    status=scene_world_mission_validate(mission);if(status)goto done;
    status=scene_campaign_history_checkpoint_rebind_validate(bindings);if(status)goto done;
    status=scene_world_environment_validate(environment);if(status)goto done;
    /* No fallible operations below. Stable scene owns every validated target. */
    scene_mover_checkpoint_assign(&campaign_group_runtime,&campaign_movers,&world->movers);
    scene_clutter_checkpoint_assign(world->clutter);scene_npc_checkpoint_restore_assign(world->npcs);
    scene_event_checkpoint_assign(&events->events);scene_world_mission_assign(mission);
    scene_campaign_history_checkpoint_assign_rebound(history,bindings);scene_world_environment_assign(environment);
    scene_remote_checkpoint_publish();
    scene_actor_body.state=player.body;rf_scene_actor_room_state=player.room;
    memcpy(actor_look.state.body_angles,player.player.body_angles,12);memcpy(actor_look.state.eye_angles,player.player.eye_angles,12);
    memset(actor_look.state.command,0,12);memset(actor_look.state.angular_velocity,0,12);actor_look.state.pending_pitch=actor_look.state.pending_yaw=0;
    memcpy(campaign_position,player.player.position,12);memcpy(campaign_orientation,player.eye,36);
    campaign_player_inventory=player.player.player.inventory;campaign_player_damage.state.effects.health=player.player.player.health;
    campaign_player_damage.state.effects.armor=player.player.player.armor;campaign_select_primary(selected);
    campaign_explicit_unarmed=player.player.player.weapon==UINT32_MAX;
    scene_weapon_modes_checkpoint_assign(stream,&player.modes);campaign_ammo_publish();
    campaign_support_handle=0;memset(campaign_support_velocity,0,12);rf_scene_actor_landing[1]=1;rf_scene_actor_landing[2]=0;
    scene_actor_body.state.flags&=~(0x200000u|0x400000u);scene_player_impact_relocated();
    printf("WORLD_SNAPSHOT_LOADED bytes%u staged%u npc%u events%u added_motion%u health%.9g armor%.9g weapon%u\n",bytes,cost,world->npcs->count,events->events.count,added,
        (double)player.player.player.health,(double)player.player.player.armor,player.player.player.weapon);
    scene_world_startup_snapshot_consumed=1;
 done:
    rf_scene_world_checkpoint_state[0]=(uint32_t)status;rf_scene_world_checkpoint_state[1]=status?0:bytes;
    rf_scene_world_checkpoint_state[2]=status?0:cost;
#ifdef RF_IMAGE_XBOX_NATIVE
    {int closed=scene_world_storage_close(&storage);if(!status)status=closed;rf_scene_world_checkpoint_state[0]=(uint32_t)status;}
#endif
    if(status)printf("WORLD_SNAPSHOT_LOAD_REJECT phase%s status%d\n",phase,status);
    scene_remote_checkpoint_discard();scene_world_event_close(&events);scene_world_environment_close(&environment);
    scene_campaign_history_checkpoint_rebind_close(&bindings);scene_campaign_history_checkpoint_close(&history);
    scene_world_mission_close(&mission);scene_world_restore_discard(&world);free(rows);free(buffer);return status;
}
#endif
