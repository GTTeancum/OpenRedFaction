#ifndef RF_SCENE_TACTICAL_COMPOSED_INCLUDED
#define RF_SCENE_TACTICAL_COMPOSED_INCLUDED
/* Read-only RFNS/player joins, including canonical recovery-only sources. */
static int scene_tactical_composed(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_seat_checkpoint_stage *seats,const scene_world_player_stage *player,uint32_t player_outside)
{
    int status;
    if(!stage||!player||player->npcs!=stage||player_outside>1||
       (campaign_npc_body_count?stage->tactical_cursor>=campaign_npc_body_count:stage->tactical_cursor!=0))return RF_RANGE;
    for(uint32_t i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;uint32_t target=e->tactical_target;
        status=scene_tactical_prepared(stage,e);if(status)return status;
        if(!e->saved.tactical.present)continue;
        status=scene_blind_prepared_live(e,seats);if(status)return status;
        if(!e->saved.tactical.phase)continue;
        if(target==campaign_player_object.handle){
            if(player_outside||!isfinite(player->player.player.health)||player->player.player.health<=0||
               (campaign_player_view.flags_810&1u)||campaign_player_object.view!=&campaign_player_view||
               rf_object_registry_lookup(&campaign_registry,target)!=&campaign_player_object||
               rf_entity_lookup(&campaign_entities,(int32_t)target)!=&campaign_player_view)return RF_FORMAT;
        }else{
            uint32_t j;
            for(j=0;j<stage->count;j++)if(stage->entries[j].handle==target)break;
            if(j==stage->count)return RF_FORMAT;
            status=scene_blind_prepared_live(stage->entries+j,seats);if(status)return status;
        }
    }
    return RF_OK;
}
static int scene_tactical_composed_prepare(scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_seat_checkpoint_stage *seats,const scene_world_player_stage *player,uint32_t player_outside)
{
    int status=scene_tactical_composed(stage,seats,player,player_outside);if(status)return status;
    stage->tactical_joined=1;return RF_OK;
}
#endif
