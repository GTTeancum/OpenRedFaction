#ifndef RF_SCENE_FRONTEND_LOAD_INCLUDED
#define RF_SCENE_FRONTEND_LOAD_INCLUDED
#include "rf/frontend_load.h"
/* Include after rf_scene_session_reset. Only explicit source acceptance calls
 * this path. Existing snapshot validators and boot/world loaders are unchanged. */
#ifdef RF_IMAGE_XBOX_NATIVE
/* A failed unmount retains this exact alias owner until restart. Do not lose
 * it on a stack return, steal an existing R: mount, or silently retry close. */
static rf_xbox_checkpoint_storage scene_frontend_load_storage;
#endif
int rf_scene_frontend_load_prepare(uint32_t source,char level[64],uint32_t *outcome)
{
#ifdef RF_IMAGE_XBOX_NATIVE
    struct scene_frontend_source {
        rf_level level;
        rf_weapon_supply_catalog supply;
    } *candidate=NULL;
    scene_world_snapshot_identity identity;
    rf_world_checkpoint world;
    rf_vpp target_archive={0},tables={0};
    unsigned char *data=NULL;
    uint32_t bytes=0,catalog_hash=0;
    int status=RF_OK,closed;
#endif
    if(outcome)*outcome=RF_SCENE_FRONTEND_LOAD_ERROR;
    if(level)level[0]=0;
    if(!level || !outcome || source>RF_SCENE_FRONTEND_LOAD_AUTO)return RF_RANGE;
#ifndef RF_IMAGE_XBOX_NATIVE
    return RF_NOT_FOUND;
#else
    if(scene_session_retire_status || scene_frontend_load_storage.mounted){
        *outcome=RF_SCENE_FRONTEND_LOAD_BLOCKED;
        return scene_session_retire_status?scene_session_retire_status:RF_IO;
    }
    /* No live scene and no previously accepted request may be replaced here.
     * Use the existing reset owner to check every retained callback/borrower. */
    if(campaign_spawn || rf_scene_follow_level_exits ||
       rf_scene_level_transition.pending || scene_live_load_active)return RF_RANGE;
    status=rf_scene_session_reset();
    if(status){*outcome=RF_SCENE_FRONTEND_LOAD_BLOCKED;return status;}
    memset(&scene_frontend_load_storage,0,sizeof(scene_frontend_load_storage));
    data=malloc(RF_CHECKPOINT_FILE_MAX);
    if(!data){status=RF_IO;goto done;}
    status=source==RF_SCENE_FRONTEND_LOAD_AUTO?
        rf_xbox_checkpoint_storage_open_world_auto(&scene_frontend_load_storage,0):
        rf_xbox_checkpoint_storage_open_world(&scene_frontend_load_storage,0);
    if(status)goto done;
    status=rf_xbox_checkpoint_storage_load(&scene_frontend_load_storage,data,
        RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_prior_validate,NULL);
    if(status){
        if(status==RF_NOT_FOUND)*outcome=RF_SCENE_FRONTEND_LOAD_MISSING;
        else if(status==RF_FORMAT || status==RF_RANGE)
            *outcome=RF_SCENE_FRONTEND_LOAD_INCOMPATIBLE;
        goto done;
    }
    status=rf_world_checkpoint_decode(data,bytes,
        RF_WORLD_CHECKPOINT_MASK(RF_WORLD_ENVIRONMENT),&world);
    if(status){*outcome=RF_SCENE_FRONTEND_LOAD_INCOMPATIBLE;goto done;}
    candidate=calloc(1,sizeof(*candidate));
    if(!candidate){status=RF_IO;goto done;}
    status=rf_level_campaign_open(&candidate->level,&target_archive,"D:\\",world.level);
    if(status){
        if(status==RF_NOT_FOUND || status==RF_FORMAT)
            *outcome=RF_SCENE_FRONTEND_LOAD_INCOMPATIBLE;
        goto done;
    }
    /* Exact campaign_clutter_open identity semantics: a zeroed catalogue,
     * rf_weapon_supply_load with the same128 KiB scratch cap, then FNV over
     * the complete catalogue object. Its intervening NPC loaders take const
     * supply pointers and do not alter this hash input. No live catalogue,
     * NPC resources, reset policy or inventory is initialized here. */
    status=rf_vpp_open(&tables,"D:\\tables.vpp");if(status)goto done;
    status=rf_weapon_supply_load(&tables,128*1024,&candidate->supply);
    if(status)goto done;
    catalog_hash=npc_hash_bytes(2166136261u,&candidate->supply,sizeof(candidate->supply));
    rf_vpp_close(&tables);
    status=scene_world_checkpoint_source_identity(&candidate->level,
        "D:\\tables.vpp",catalog_hash,identity.bytes);
    if(status)goto done;
    memset(identity.level,0,sizeof(identity.level));
    strcpy(identity.level,candidate->level.entry.name);
    status=scene_world_snapshot_validate(data,bytes,&identity);
    if(status){*outcome=RF_SCENE_FRONTEND_LOAD_INCOMPATIBLE;goto done;}
    {const rf_world_checkpoint_slice *charges=world.sections+RF_WORLD_PROJECTILES-1;
     status=scene_continuous_charge_checkpoint_preflight(charges->data,charges->bytes);}
    if(status)*outcome=RF_SCENE_FRONTEND_LOAD_INCOMPATIBLE;
 done:
    rf_vpp_close(&tables);rf_vpp_close(&target_archive);
    free(candidate);free(data);
    /* Native close is a publication barrier even on an earlier rejection.
     * Report a failed close instead of mislabeling it as missing/corrupt data. */
    closed=scene_world_storage_close(&scene_frontend_load_storage);
    if(closed){
        if(!scene_session_retire_status)scene_session_retire_status=closed;
        *outcome=RF_SCENE_FRONTEND_LOAD_BLOCKED;
        return closed;
    }
    if(status)return status;
    /* All transient source owners have closed. The existing resource demand
     * and frame-zero world transaction still perform their full validation. */
    memcpy(level,identity.level,sizeof(identity.level));
    scene_live_load_auto_source=source==RF_SCENE_FRONTEND_LOAD_AUTO;
    scene_live_load_active=1;
    *outcome=RF_SCENE_FRONTEND_LOAD_READY;
    return RF_OK;
#endif
}
#endif
