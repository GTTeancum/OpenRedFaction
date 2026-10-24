/* Diagnostics: [0] requests,[1] chosen family(1 auto,2 manual),[2] autosave
 * status,[3] FNV1a of saved level,[4] FNV1a of current level,[5] storage
 * generation,[6] slot,[7] death recovery. */
uint32_t rf_scene_quickload_choice[8];
static uint32_t scene_quickload_name_hash(const char *name,uint32_t limit)
{
    uint32_t h=2166136261u,i;
    for(i=0;i<limit && name[i];i++)h=(h^(unsigned char)name[i])*16777619u;
    return h;
}
/* Resolve the saved level and verify its source before discarding the current scene.
 * Full component/resource admission remains in the fresh-scene loader. */
static int scene_world_quickload_request(scene_stream *stream,uint32_t death_recovery)
{
    {int boundary=scene_pursuit_save_boundary();if(boundary)return boundary;}
    scene_world_snapshot_identity identity;rf_checkpoint_file_selection selection={0};
    unsigned char *data=NULL;uint32_t bytes=0,i,prefix=0;int status=RF_OK;
    rf_world_checkpoint world;rf_level target;rf_vpp target_archive={0};char directory[1024];
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_xbox_checkpoint_storage storage={0};uint32_t chosen_auto=0;
#else
    (void)death_recovery;
#endif
    if(scene_turret_player_active()||scene_player_jeep_gunner_active(stream)||!stream||!campaign_spawn||!rf_scene_follow_level_exits||rf_scene_dev_room_enabled||
       rf_scene_level_transition.pending||!stream->world_checkpoint_level||!stream->world_checkpoint_tables)return RF_RANGE;
    data=malloc(RF_CHECKPOINT_FILE_MAX);if(!data){status=RF_IO;goto done;}
#ifdef RF_IMAGE_XBOX_NATIVE
    (void)selection;
    if(death_recovery){
        /* Death reloads the player's most recent save, autosave or manual,
         * whichever level it belongs to; ties keep the autosave. */
        uint64_t auto_time=0,manual_time=0;int auto_status,closed;
        auto_status=rf_xbox_checkpoint_storage_open_world_auto(&storage,0);
        if(!auto_status)auto_status=rf_xbox_checkpoint_storage_load(&storage,data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_prior_validate,NULL);
        rf_scene_quickload_choice[2]=(uint32_t)auto_status;
        if(!auto_status && rf_xbox_checkpoint_storage_selected_time(&storage,&auto_time))auto_time=0;
        if(auto_status && auto_status!=RF_NOT_FOUND && auto_status!=RF_FORMAT && auto_status!=RF_RANGE){status=auto_status;goto done;}
        closed=scene_world_storage_close(&storage);if(closed){status=closed;goto done;}
        memset(&storage,0,sizeof(storage));
        status=rf_xbox_checkpoint_storage_open_world(&storage,0);
        if(!status)status=rf_xbox_checkpoint_storage_load(&storage,data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_prior_validate,NULL);
        if(!status && !auto_status && (rf_xbox_checkpoint_storage_selected_time(&storage,&manual_time) || manual_time<=auto_time))status=RF_NOT_FOUND;
        if(status){
            if(auto_status)goto done;
            closed=scene_world_storage_close(&storage);if(closed){status=closed;goto done;}
            memset(&storage,0,sizeof(storage));
            status=rf_xbox_checkpoint_storage_open_world_auto(&storage,0);
            if(!status)status=rf_xbox_checkpoint_storage_load(&storage,data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_prior_validate,NULL);
            chosen_auto=!status;
        }
    }else{
        status=rf_xbox_checkpoint_storage_open_world(&storage,0);
        if(!status)status=rf_xbox_checkpoint_storage_load(&storage,data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_prior_validate,NULL);
    }
#else
    status=rf_checkpoint_file_load("redfaction-save",data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_prior_validate,NULL,&selection);
#endif
    if(status)goto done;
    status=rf_world_checkpoint_decode(data,bytes,RF_WORLD_CHECKPOINT_MASK(RF_WORLD_ENVIRONMENT),&world);if(status)goto done;
    ++rf_scene_quickload_choice[0];
    rf_scene_quickload_choice[3]=scene_quickload_name_hash(world.level,sizeof(world.level));
    rf_scene_quickload_choice[4]=scene_quickload_name_hash(campaign_current_level,sizeof(campaign_current_level));
    rf_scene_quickload_choice[7]=death_recovery;
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_scene_quickload_choice[1]=chosen_auto?1u:2u;
    rf_scene_quickload_choice[5]=storage.selection.generation;rf_scene_quickload_choice[6]=storage.selection.slot;
#endif
    /* Prefer the already-open archive, including a process-local fixture. */
    status=rf_level_open(&target,stream->world_checkpoint_level->archive,world.level);
    if(status==RF_NOT_FOUND){
        for(i=0;stream->world_checkpoint_tables[i];i++){
            if(i>=sizeof(directory)-1){status=RF_RANGE;goto done;}
            if(stream->world_checkpoint_tables[i]=='/'||stream->world_checkpoint_tables[i]=='\\')prefix=i+1;
        }
        if(prefix){memcpy(directory,stream->world_checkpoint_tables,prefix);directory[prefix]=0;}
        else strcpy(directory,".");
        status=rf_level_campaign_open(&target,&target_archive,directory,world.level);
    }
    if(status)goto done;
    status=scene_world_checkpoint_source_identity(&target,stream->world_checkpoint_tables,rf_scene_weapon_supply[3],identity.bytes);if(status)goto done;
    memset(identity.level,0,sizeof(identity.level));strcpy(identity.level,target.entry.name);
    status=scene_world_snapshot_validate(data,bytes,&identity);if(status)goto done;
    {const rf_world_checkpoint_slice *charges=world.sections+RF_WORLD_PROJECTILES-1;
        status=scene_continuous_charge_checkpoint_preflight(charges->data,charges->bytes);}
 done:
    rf_vpp_close(&target_archive);
#ifdef RF_IMAGE_XBOX_NATIVE
    {int closed=scene_world_storage_close(&storage);if(!status)status=closed;}
#endif
    free(data);if(status)return status;
    memset(&rf_scene_level_transition,0,sizeof(rf_scene_level_transition));
    strcpy(rf_scene_level_transition.level,identity.level);
    rf_scene_level_transition.uid=UINT32_MAX-2u;rf_scene_level_transition.pending=1;
    scene_live_load_active=1;
#ifdef RF_IMAGE_XBOX_NATIVE
    scene_live_load_auto_source=chosen_auto;
#endif
    return RF_OK;
}
