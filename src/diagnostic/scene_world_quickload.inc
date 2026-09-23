/* Resolve the saved level and verify its source before discarding the current scene.
 * Full component/resource admission remains in the fresh-scene loader. */
static int scene_world_quickload_request(scene_stream *stream)
{
    scene_world_snapshot_identity identity;rf_checkpoint_file_selection selection={0};
    unsigned char *data=NULL;uint32_t bytes=0,i,prefix=0;int status=RF_OK;
    rf_world_checkpoint world;rf_level target;rf_vpp target_archive={0};char directory[1024];
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_xbox_checkpoint_storage storage={0};
#endif
    if(!stream||!campaign_spawn||!rf_scene_follow_level_exits||rf_scene_dev_room_enabled||
       rf_scene_level_transition.pending||!stream->world_checkpoint_level||!stream->world_checkpoint_tables)return RF_RANGE;
    data=malloc(RF_CHECKPOINT_FILE_MAX);if(!data){status=RF_IO;goto done;}
#ifdef RF_IMAGE_XBOX_NATIVE
    (void)selection;status=rf_xbox_checkpoint_storage_open_world(&storage,0);
    if(!status)status=rf_xbox_checkpoint_storage_load(&storage,data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_prior_validate,NULL);
#else
    status=rf_checkpoint_file_load("redfaction-save",data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_prior_validate,NULL,&selection);
#endif
    if(status)goto done;
    status=rf_world_checkpoint_decode(data,bytes,RF_WORLD_CHECKPOINT_MASK(RF_WORLD_ENVIRONMENT),&world);if(status)goto done;
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
    status=scene_world_snapshot_validate(data,bytes,&identity);
 done:
    rf_vpp_close(&target_archive);
#ifdef RF_IMAGE_XBOX_NATIVE
    {int closed=scene_world_storage_close(&storage);if(!status)status=closed;}
#endif
    free(data);if(status)return status;
    memset(&rf_scene_level_transition,0,sizeof(rf_scene_level_transition));
    strcpy(rf_scene_level_transition.level,identity.level);
    rf_scene_level_transition.uid=UINT32_MAX-2u;rf_scene_level_transition.pending=1;
    scene_live_load_active=1;return RF_OK;
}
