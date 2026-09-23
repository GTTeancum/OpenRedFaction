/* Confirm a same-level save exists before discarding the current scene.
 * Full component/resource admission remains in the fresh-scene loader. */
static int scene_world_quickload_request(scene_stream *stream)
{
    scene_world_snapshot_identity identity;rf_checkpoint_file_selection selection={0};
    unsigned char *data=NULL;uint32_t bytes=0;int status=RF_OK;
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_xbox_checkpoint_storage storage={0};
#endif
    if(!stream||!campaign_spawn||!rf_scene_follow_level_exits||rf_scene_dev_room_enabled||
       rf_scene_level_transition.pending||!stream->world_checkpoint_level||!stream->world_checkpoint_tables)return RF_RANGE;
    status=scene_world_checkpoint_source_identity(stream->world_checkpoint_level,stream->world_checkpoint_tables,
        rf_scene_weapon_supply[3],identity.bytes);if(status)goto done;
    if(strlen(campaign_current_level)>=sizeof(identity.level)){status=RF_RANGE;goto done;}
    memset(identity.level,0,sizeof(identity.level));strcpy(identity.level,campaign_current_level);
    data=malloc(RF_CHECKPOINT_FILE_MAX);if(!data){status=RF_IO;goto done;}
#ifdef RF_IMAGE_XBOX_NATIVE
    (void)selection;status=rf_xbox_checkpoint_storage_open_world(&storage,0);
    if(!status)status=rf_xbox_checkpoint_storage_load(&storage,data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity);
#else
    status=rf_checkpoint_file_load("redfaction-save",data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity,&selection);
#endif
 done:
#ifdef RF_IMAGE_XBOX_NATIVE
    {int closed=scene_world_storage_close(&storage);if(!status)status=closed;}
#endif
    free(data);if(status)return status;
    memset(&rf_scene_level_transition,0,sizeof(rf_scene_level_transition));
    strcpy(rf_scene_level_transition.level,campaign_current_level);
    rf_scene_level_transition.uid=UINT32_MAX-2u;rf_scene_level_transition.pending=1;
    scene_live_load_active=1;return RF_OK;
}
