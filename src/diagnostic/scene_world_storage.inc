#ifndef RF_SCENE_WORLD_STORAGE_INCLUDED
#define RF_SCENE_WORLD_STORAGE_INCLUDED
/* Read-only optical flags opt in. Native saves use their own protected HDD
 * profile; existing DEV destruction slots are never reused. */
uint32_t rf_scene_world_checkpoint_state[10]; /* load status/bytes/staged,save status/bytes,generation,slot,hash,load attempted,save attempted */
unsigned char *rf_scene_world_checkpoint_data; /* Retained native endpoint capture; released at next capture. */
#ifdef RF_IMAGE_XBOX_NATIVE
static int scene_world_storage_flag(const char *path,uint32_t *found)
{
    FILE *file;*found=0;errno=0;file=fopen(path,"rb");
    if(!file)return errno==ENOENT?RF_OK:RF_IO;
    *found=1;return fclose(file)?RF_IO:RF_OK;
}
static int scene_world_storage_close(rf_xbox_checkpoint_storage *storage)
{
    uint32_t state[8];int status;if(!storage->mounted)return RF_OK;
    memcpy(state,rf_xbox_checkpoint_storage_state,sizeof(state));status=rf_xbox_checkpoint_storage_close(storage);
    if(!status){state[7]&=~3u;memcpy(rf_xbox_checkpoint_storage_state,state,sizeof(state));}return status;
}
#endif
#endif
