/* Endpoint readiness evidence on an unmodified ordinary level. This does not
 * publish a save or relax admission. Enabled explicitly for one replay. */
#include "rf/campaign_goals_checkpoint.h"
#include "rf/campaign_pickups_checkpoint.h"
#include "rf/campaign_triggers_checkpoint.h"
static void scene_world_checkpoint_probe_result(const char *name,int status,uint32_t bytes)
{printf("WORLD_CHECKPOINT_PROBE %s status%d bytes%u\n",name,status,status?0:bytes);}
static int scene_world_checkpoint_probe(scene_stream *s,const rf_level *level,const char *tables_path)
{
    unsigned char identity[32],*buffer=NULL;rf_level_uid_object *objects=NULL;uint32_t object_count=0,bytes=0,i,slot;
    rf_player_checkpoint player;rf_player_checkpoint_catalog catalog;rf_campaign_triggers *triggers=NULL;
    scene_event_checkpoint_map map;rf_event_checkpoint_refs refs;int status;int32_t now;
#ifndef RF_IMAGE_XBOX_NATIVE
    const char *enabled=getenv("RF_REPLAY_WORLD_CHECKPOINT_PROBE");if(!enabled||strcmp(enabled,"1"))return RF_OK;
#else
    return RF_OK; /* PC diagnostic only; underlying codecs remain shared. */
#endif
    if(!s||!level||!tables_path||combat_frame==UINT32_MAX)return RF_RANGE;
    now=(int32_t)(((uint64_t)combat_frame*1000/60)%RF_TIMER_PERIOD);
    status=scene_world_checkpoint_source_identity(level,tables_path,rf_scene_weapon_supply[3],identity);
    scene_world_checkpoint_probe_result("identity",status,32);if(status)return status;
    buffer=malloc(RF_CHECKPOINT_FILE_MAX);if(!buffer)return RF_IO;
    status=scene_checkpoint_player_snapshot(s,1,&player,&catalog);
    if(!status)status=rf_player_checkpoint_encode(&player,&catalog,buffer,RF_PLAYER_CHECKPOINT_BYTES);
    scene_world_checkpoint_probe_result("player",status,RF_PLAYER_CHECKPOINT_BYTES);
    if(status)printf("WORLD_CHECKPOINT_PLAYER health%.9g landing%u support%u crouch%u velocity%.9g,%.9g,%.9g\n",
        (double)campaign_player_damage.state.effects.health,rf_scene_actor_landing[1],campaign_support_handle,campaign_crouched,
        (double)scene_actor_body.state.velocity[0],(double)scene_actor_body.state.velocity[1],(double)scene_actor_body.state.velocity[2]);
    status=rf_scene_npc_checkpoint_export(identity,now,buffer,RF_CHECKPOINT_FILE_MAX,&bytes);
    scene_world_checkpoint_probe_result("npc",status,bytes);
    if(status)for(i=0;i<campaign_npc_body_count;i++){
        rf_npc_checkpoint_record row;int result=scene_npc_checkpoint_row(i,now,&row);const campaign_npc_body *o=campaign_npc_bodies+i;
        if(result&&result!=RF_NOT_FOUND)printf("WORLD_CHECKPOINT_NPC uid%u status%d route%u retained%u animation%u alert%u due%d mode%d velocity%.6g,%.6g,%.6g\n",
            (uint32_t)campaign_seeds.records.items[i].record.uid,result,o->script_move.path.count,o->navigation.retained.count,
            o->script_animation.active,o->combat_alert,o->combat_due,o->ai_mode.action_280,
            (double)o->body.state.velocity[0],(double)o->body.state.velocity[1],(double)o->body.state.velocity[2]);
        if(result&&o->script_animation.active)printf("WORLD_CHECKPOINT_NPC_SCRIPT uid%u loop%u freeze%u motion%d\n",
            (uint32_t)campaign_seeds.records.items[i].record.uid,o->script_animation.loop,o->script_animation.freeze,o->script_animation.motion);
    }
    status=rf_scene_mover_checkpoint_export(identity,now,buffer,RF_CHECKPOINT_FILE_MAX,&bytes);
    scene_world_checkpoint_probe_result("mover",status,bytes);
    status=scene_clutter_checkpoint_capture(identity,now,buffer,RF_CHECKPOINT_FILE_MAX,&bytes);
    scene_world_checkpoint_probe_result("clutter",status,bytes);
    status=rf_campaign_goals_checkpoint_encode(identity,&rf_scene_mission_goals,&campaign_local_goals,buffer,RF_CHECKPOINT_FILE_MAX,&bytes);
    scene_world_checkpoint_probe_result("goals",status,bytes);
    status=rf_campaign_pickups_checkpoint_encode(identity,&rf_scene_campaign_pickups,buffer,RF_CHECKPOINT_FILE_MAX,&bytes);
    scene_world_checkpoint_probe_result("pickups",status,bytes);
    status=rf_campaign_pickups_checkpoint_encode(identity,&campaign_startup_inventory,buffer,RF_CHECKPOINT_FILE_MAX,&bytes);
    scene_world_checkpoint_probe_result("startup",status,bytes);
    triggers=malloc(sizeof(*triggers));if(!triggers){free(buffer);return RF_IO;}*triggers=campaign_trigger_history;
    status=RF_OK;for(i=0;i<campaign_triggers.count&&!status;i++){
        const rf_runtime_trigger *t=campaign_triggers.items+i;
        status=rf_campaign_trigger_register(triggers,campaign_current_level,t->authored->record.uid,&slot);
        if(!status)status=rf_runtime_trigger_save(t,now,triggers->states+slot);
        if(!status)triggers->items[slot].retired=1;
    }
    if(!status)status=rf_campaign_triggers_checkpoint_encode(identity,triggers,buffer,RF_CHECKPOINT_FILE_MAX,&bytes);
    scene_world_checkpoint_probe_result("triggers",status,bytes);free(triggers);
    status=scene_world_checkpoint_uid_map_open(s,&objects,&object_count,RF_CHECKPOINT_FILE_MAX);
    scene_world_checkpoint_probe_result("uid_map",status,object_count*(uint32_t)sizeof(*objects));
    if(!status){
        int event_status=RF_OK;
        map=(scene_event_checkpoint_map){objects,object_count,&campaign_registry};
        refs=(rf_event_checkpoint_refs){scene_event_checkpoint_export,scene_event_checkpoint_import,&map};
        for(i=0;i<campaign_events.count;i++){
            const rf_runtime_event *e=campaign_events.items+i;
            status=rf_event_checkpoint_encode_mapped(identity,e,now,&refs,buffer,RF_CHECKPOINT_FILE_MAX);
            if(status&&!event_status)event_status=status;
            if(status)printf("WORLD_CHECKPOINT_EVENT uid%u type%u status%d deadline%d\n",(uint32_t)e->authored->record.uid,e->state.type,status,e->state.deadline);
        }
        scene_world_checkpoint_probe_result("events",event_status,campaign_events.count*RF_EVENT_CHECKPOINT_BYTES);
    }
    free(objects);free(buffer);puts("WORLD_CHECKPOINT_PROBE_ONLY no_save_written");return RF_OK;
}
