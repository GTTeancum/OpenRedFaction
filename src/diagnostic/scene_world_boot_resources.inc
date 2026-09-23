/* Read-only ordinary-save resource demand. This does not grant inventory or
 * publish gameplay; the normal world transaction still validates every owner. */
static int scene_world_boot_weapon_mask(const rf_level *level,const char *tables_path,uint32_t *mask)
{
#ifndef RF_IMAGE_XBOX_NATIVE
    const char *path=scene_live_load_active?"redfaction-save":getenv("RF_REPLAY_WORLD_SNAPSHOT_IN");
#else
    rf_xbox_checkpoint_storage storage={0};uint32_t enabled=0;
#endif
    scene_world_snapshot_identity identity;rf_world_checkpoint world;
    rf_player_checkpoint_catalog catalog;rf_player_checkpoint player;
    rf_checkpoint_file_selection selection={0};unsigned char *data=NULL;
    uint32_t bytes=0,i;int status=RF_OK;
    *mask=0;
#ifdef RF_IMAGE_XBOX_NATIVE
    if(!scene_live_load_active){status=scene_world_storage_flag("D:\\world-hdd-load.flag",&enabled);if(status||!enabled)return status;}
#else
    if(!path||!*path)return RF_OK;
#endif
    status=scene_world_checkpoint_source_identity(level,tables_path,rf_scene_weapon_supply[3],identity.bytes);if(status)goto done;
    if(strlen(campaign_current_level)>=sizeof(identity.level)){status=RF_RANGE;goto done;}
    memset(identity.level,0,sizeof(identity.level));strcpy(identity.level,campaign_current_level);
    data=malloc(RF_CHECKPOINT_FILE_MAX);if(!data){status=RF_IO;goto done;}
#ifdef RF_IMAGE_XBOX_NATIVE
    (void)selection;status=rf_xbox_checkpoint_storage_open_world(&storage,0);
    if(!status)status=rf_xbox_checkpoint_storage_load(&storage,data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity);
#else
    status=rf_checkpoint_file_load(path,data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity,&selection);
#endif
    if(status)goto done;
    status=rf_world_checkpoint_decode(data,bytes,RF_WORLD_CHECKPOINT_MASK(RF_WORLD_PLAYER),&world);if(status)goto done;
    status=scene_checkpoint_player_catalog_mode(&catalog,1);if(status)goto done;
    status=rf_player_checkpoint_decode(world.sections[RF_WORLD_PLAYER-1].data,world.sections[RF_WORLD_PLAYER-1].bytes,&catalog,&player);if(status)goto done;
    for(i=0;i<SCENE_WEAPON_SLOTS;i++){
        int32_t id=rf_weapon_name_find(&campaign_weapon_supply.names,campaign_weapon_names[i]);
        if(id>=0&&player.player.inventory.owned[id])*mask|=1u<<i;
    }
    printf("WORLD_BOOT_WEAPONS mask%u\n",*mask);
 done:
#ifdef RF_IMAGE_XBOX_NATIVE
    {int closed=scene_world_storage_close(&storage);if(!status)status=closed;}
#endif
    free(data);if(status)*mask=0;return status;
}
