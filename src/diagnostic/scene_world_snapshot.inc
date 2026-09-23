#ifndef RF_SCENE_WORLD_SNAPSHOT_INCLUDED
#define RF_SCENE_WORLD_SNAPSHOT_INCLUDED
#include "rf/world_checkpoint.h"
/* Ordinary same-level profile. Components capture one paused endpoint;
 * restore performs resource/world/history admission before publication. */
typedef struct scene_world_snapshot_identity {unsigned char bytes[32];char level[64];} scene_world_snapshot_identity;
static int scene_world_snapshot_validate(const void *data,uint32_t bytes,void *context)
{
    const scene_world_snapshot_identity *identity=context;rf_world_checkpoint bundle;int status;
    uint32_t required=RF_WORLD_CHECKPOINT_MASK(RF_WORLD_PLAYER)|RF_WORLD_CHECKPOINT_MASK(RF_WORLD_NPC)|
        RF_WORLD_CHECKPOINT_MASK(RF_WORLD_MOVER)|RF_WORLD_CHECKPOINT_MASK(RF_WORLD_TRIGGER)|
        RF_WORLD_CHECKPOINT_MASK(RF_WORLD_GOALS)|RF_WORLD_CHECKPOINT_MASK(RF_WORLD_PICKUPS)|
        RF_WORLD_CHECKPOINT_MASK(RF_WORLD_CLUTTER)|RF_WORLD_CHECKPOINT_MASK(RF_WORLD_WEAPON_MODES)|
        RF_WORLD_CHECKPOINT_MASK(RF_WORLD_STARTUP)|RF_WORLD_CHECKPOINT_MASK(RF_WORLD_CAMPAIGN_HISTORY);
    status=rf_world_checkpoint_decode(data,bytes,required,&bundle);if(status)return status;
    if(!identity||memcmp(bundle.identity,identity->bytes,32)||strcmp(bundle.level,identity->level))return RF_FORMAT;
    return RF_OK; /* Transport validation only; restore needs semantic staging. */
}
/* Saving may replace a different level's valid save. Select its generation
 * structurally to preserve the previous slot; loading still requires identity. */
static int scene_world_snapshot_prior_validate(const void *data,uint32_t bytes,void *context)
{
    rf_world_checkpoint world;(void)context;return rf_world_checkpoint_decode(data,bytes,0,&world);
}
static inline void scene_world_history_probe(const rf_world_checkpoint *world,int32_t now)
{
#ifndef RF_IMAGE_XBOX_NATIVE
    scene_campaign_history_checkpoint_stage *history=NULL;scene_campaign_history_rebind_stage *bindings=NULL;
    const rf_world_checkpoint_slice *v=world->sections+RF_WORLD_CAMPAIGN_HISTORY-1;
    const char *enabled=getenv("RF_REPLAY_WORLD_RESTORE_PROBE");int status;
    if(!enabled||strcmp(enabled,"1"))return;
    status=scene_campaign_history_checkpoint_prepare(world->identity,v->data,v->bytes,1024u*1024u,&history);
    if(!status)status=scene_campaign_history_checkpoint_rebind_prepare(history,1024u*1024u,&bindings);
    if(!status)status=scene_campaign_history_checkpoint_rebind_validate(bindings);
    printf("WORLD_RESTORE_HISTORY status%d actors%u carry%u no_publication1\n",status,history?history->actors.count:0,history?history->carry_mask:0);
    scene_campaign_history_checkpoint_rebind_close(&bindings);scene_campaign_history_checkpoint_close(&history);
#else
    (void)world;
#endif
    (void)now;
}
static int scene_world_snapshot_capture_mode(scene_stream *s,const rf_level *level,const char *tables_path,uint32_t requested)
{
#ifndef RF_IMAGE_XBOX_NATIVE
    const char *path=requested?"redfaction-save":getenv("RF_REPLAY_WORLD_SNAPSHOT_OUT");
#else
    rf_xbox_checkpoint_storage storage={0};uint32_t enabled=0;
    const char *path=RF_XBOX_WORLD_CHECKPOINT_BASE;
#endif
    unsigned char *buffer=NULL;rf_world_checkpoint bundle={0};scene_world_snapshot_identity identity;
    rf_checkpoint_file_selection selection={0};rf_player_checkpoint player;rf_player_checkpoint_catalog catalog;
    rf_weapon_modes_checkpoint modes;rf_campaign_triggers *triggers=NULL;rf_level_uid_object *objects=NULL;
    scene_event_checkpoint_map map;rf_event_checkpoint_refs refs;
    uint32_t at=RF_WORLD_CHECKPOINT_PREFIX,bytes=0,total=0,i,slot,object_count=0,omissions=0;
    int status=RF_OK;int32_t now;const char *part="identity";
#ifdef RF_IMAGE_XBOX_NATIVE
    if(!requested){status=scene_world_storage_flag("D:\\world-hdd-save.flag",&enabled);if(status||!enabled)return status;}
#endif
    if(!path||!*path)return RF_OK;
    rf_scene_world_checkpoint_state[9]=1;
    if(!s||!level||!tables_path||s->terrain||combat_frame==UINT32_MAX){status=RF_RANGE;goto done;}
    now=(int32_t)(((uint64_t)combat_frame*1000/60)%RF_TIMER_PERIOD);
    status=scene_world_checkpoint_source_identity(level,tables_path,rf_scene_weapon_supply[3],identity.bytes);if(status)goto done;
    if(strlen(campaign_current_level)>=sizeof(identity.level)){status=RF_RANGE;goto done;}
    memset(identity.level,0,sizeof(identity.level));strcpy(identity.level,campaign_current_level);
    memcpy(bundle.identity,identity.bytes,32);memcpy(bundle.level,identity.level,64);
    buffer=malloc(RF_CHECKPOINT_FILE_MAX);if(!buffer){status=RF_IO;goto done;}
    part="prior_snapshot";
#ifndef RF_IMAGE_XBOX_NATIVE
    status=rf_checkpoint_file_load(path,buffer,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_prior_validate,NULL,&selection);
#else
    (void)path;status=rf_xbox_checkpoint_storage_open_world(&storage,1);
    if(!status)status=rf_xbox_checkpoint_storage_load(&storage,buffer,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_prior_validate,NULL);
#endif
    if(status==RF_NOT_FOUND)status=RF_OK;if(status)goto done;
#define SNAP_START(name) do{part=name;bytes=0;}while(0)
#define SNAP_END(type) do{if(status)goto done;if(bytes>RF_CHECKPOINT_FILE_MAX-at){status=RF_RANGE;goto done;}     bundle.sections[(type)-1]=(rf_world_checkpoint_slice){bytes?buffer+at:NULL,bytes};at+=bytes;     printf("WORLD_SNAPSHOT_COMPONENT %s %u\n",part,bytes);}while(0)
    SNAP_START("player");status=scene_checkpoint_player_snapshot(s,1,&player,&catalog);
    if(!status&&RF_PLAYER_CHECKPOINT_BYTES>RF_CHECKPOINT_FILE_MAX-at)status=RF_RANGE;
    if(!status)status=rf_player_checkpoint_encode(&player,&catalog,buffer+at,RF_PLAYER_CHECKPOINT_BYTES);
    bytes=RF_PLAYER_CHECKPOINT_BYTES;SNAP_END(RF_WORLD_PLAYER);
    SNAP_START("npc");status=rf_scene_npc_checkpoint_export(identity.bytes,now,buffer+at,RF_CHECKPOINT_FILE_MAX-at,&bytes);SNAP_END(RF_WORLD_NPC);
    SNAP_START("mover");status=rf_scene_mover_checkpoint_export(identity.bytes,now,buffer+at,RF_CHECKPOINT_FILE_MAX-at,&bytes);SNAP_END(RF_WORLD_MOVER);
    SNAP_START("events");status=scene_world_checkpoint_uid_map_open(s,&objects,&object_count,RF_CHECKPOINT_FILE_MAX);if(status)goto done;
    if(campaign_events.count>(RF_CHECKPOINT_FILE_MAX-at)/RF_EVENT_CHECKPOINT_BYTES){status=RF_RANGE;goto done;}
    map=(scene_event_checkpoint_map){objects,object_count,&campaign_registry};
    refs=(rf_event_checkpoint_refs){scene_event_checkpoint_export,scene_event_checkpoint_import,&map};
    for(i=0;i<campaign_events.count;i++){
        const rf_runtime_event *event=campaign_events.items+i;
        if(!event->retired && event->state.deadline>=0 &&
           (rf_event_checkpoint_external_requirements(event->state.type)&RF_EVENT_CHECKPOINT_EXTERNAL_UNIMPLEMENTED)){
            status=RF_NOT_FOUND;goto done;
        }
        status=rf_event_checkpoint_encode_mapped(identity.bytes,campaign_events.items+i,now,&refs,buffer+at+bytes,RF_EVENT_CHECKPOINT_BYTES);
        if(status)goto done;bytes+=RF_EVENT_CHECKPOINT_BYTES;
    }
    SNAP_END(RF_WORLD_EVENT);free(objects);objects=NULL;
    SNAP_START("triggers");triggers=malloc(sizeof(*triggers));if(!triggers){status=RF_IO;goto done;}*triggers=campaign_trigger_history;
    for(i=0;i<campaign_triggers.count&&!status;i++){
        const rf_runtime_trigger *trigger=campaign_triggers.items+i;
        status=rf_campaign_trigger_register(triggers,campaign_current_level,trigger->authored->record.uid,&slot);
        if(!status)status=rf_runtime_trigger_save(trigger,now,triggers->states+slot);
        if(!status)triggers->items[slot].retired=1;
    }
    if(!status)status=rf_campaign_triggers_checkpoint_encode(identity.bytes,triggers,buffer+at,RF_CHECKPOINT_FILE_MAX-at,&bytes);
    SNAP_END(RF_WORLD_TRIGGER);free(triggers);triggers=NULL;
    SNAP_START("goals");status=rf_campaign_goals_checkpoint_encode(identity.bytes,&rf_scene_mission_goals,&campaign_local_goals,buffer+at,RF_CHECKPOINT_FILE_MAX-at,&bytes);SNAP_END(RF_WORLD_GOALS);
    SNAP_START("pickups");status=rf_campaign_pickups_checkpoint_encode(identity.bytes,&rf_scene_campaign_pickups,buffer+at,RF_CHECKPOINT_FILE_MAX-at,&bytes);SNAP_END(RF_WORLD_PICKUPS);
    SNAP_START("clutter");status=scene_clutter_checkpoint_capture(identity.bytes,now,buffer+at,RF_CHECKPOINT_FILE_MAX-at,&bytes);SNAP_END(RF_WORLD_CLUTTER);
    SNAP_START("weapon_modes");status=scene_weapon_modes_checkpoint_capture(s,&modes);
    if(!status)status=rf_weapon_modes_checkpoint_encode(scene_remote_checkpoint_catalog_hash(),&modes,buffer+at,RF_CHECKPOINT_FILE_MAX-at,&bytes);SNAP_END(RF_WORLD_WEAPON_MODES);
    SNAP_START("remote");status=scene_remote_checkpoint_capture(buffer+at,RF_CHECKPOINT_FILE_MAX-at,&bytes);SNAP_END(RF_WORLD_REMOTE);
    SNAP_START("vehicle");if(RF_CHECKPOINT_FILE_MAX-at<RF_JEEP_CHECKPOINT_BYTES){status=RF_RANGE;goto done;}
    status=scene_vehicle_checkpoint_capture(s,buffer+at,&bytes);SNAP_END(RF_WORLD_VEHICLE);
    /* Ordinary static scene has no active terrain owner; DESTRUCTION is absent.
     * Switch records are held by CAMPAIGN_HISTORY instead of duplicated. */
    SNAP_START("startup");status=rf_campaign_pickups_checkpoint_encode(identity.bytes,&campaign_startup_inventory,buffer+at,RF_CHECKPOINT_FILE_MAX-at,&bytes);SNAP_END(RF_WORLD_STARTUP);
    SNAP_START("campaign_history");omissions=scene_campaign_history_checkpoint_omissions();
    status=scene_campaign_history_checkpoint_encode(identity.bytes,now,buffer+at,RF_CHECKPOINT_FILE_MAX-at,&bytes);SNAP_END(RF_WORLD_CAMPAIGN_HISTORY);
    SNAP_START("environment");status=scene_world_environment_encode(identity.bytes,buffer+at,RF_CHECKPOINT_FILE_MAX-at,&bytes);SNAP_END(RF_WORLD_ENVIRONMENT);
    part="envelope";status=rf_world_checkpoint_encode(&bundle,0,buffer,RF_CHECKPOINT_FILE_MAX,&total);if(status)goto done;
    (void)scene_world_restore_probe(s,buffer,total,identity.bytes,now);
    (void)scene_world_mission_probe(s,&bundle,now);
    scene_world_history_probe(&bundle,now);
    part="storage";
#ifndef RF_IMAGE_XBOX_NATIVE
    status=rf_checkpoint_file_store(path,buffer,total,scene_world_snapshot_validate,&identity,&selection);
#else
    status=rf_xbox_checkpoint_storage_store(&storage,buffer,total,scene_world_snapshot_validate,&identity);selection=storage.selection;
    if(!status){free(rf_scene_world_checkpoint_data);rf_scene_world_checkpoint_data=buffer;buffer=NULL;}
#endif
    rf_scene_world_checkpoint_state[4]=status?0:total;rf_scene_world_checkpoint_state[5]=selection.generation;rf_scene_world_checkpoint_state[6]=selection.slot;
    if(!status)printf("WORLD_SNAPSHOT_STORED bytes%u slot%u generation%u history_omissions%u restore_available0\n",total,selection.slot,selection.generation,omissions);
#undef SNAP_START
#undef SNAP_END
 done:free(objects);free(triggers);free(buffer);
#ifdef RF_IMAGE_XBOX_NATIVE
    {int closed=scene_world_storage_close(&storage);if(!status)status=closed;}
#endif
    rf_scene_world_checkpoint_state[3]=(uint32_t)status;
    if(status)printf("WORLD_SNAPSHOT_REJECT %s %d\n",part,status);return status;
}
static int scene_world_snapshot_capture(scene_stream *s,const rf_level *level,const char *tables_path)
{return scene_world_snapshot_capture_mode(s,level,tables_path,0);}
#endif
