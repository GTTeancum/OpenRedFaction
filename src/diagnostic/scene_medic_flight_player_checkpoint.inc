#ifndef RF_SCENE_MEDIC_FLIGHT_PLAYER_CHECKPOINT_INCLUDED
#define RF_SCENE_MEDIC_FLIGHT_PLAYER_CHECKPOINT_INCLUDED
/* Called after actual player/environment/form candidate admission. The existing
 * RFPL player owner identifies the new target; no raw saved handle or UID0. */
static int scene_medic_flight_player_checkpoint_admit(scene_npc_checkpoint_restore_stage *stage,
    const scene_world_player_stage *player)
{
    uint32_t i,handle=campaign_player_object.handle;int status;
    if(!stage||!player||player->npcs!=stage)return RF_RANGE;
    if(!handle||handle==UINT32_MAX||campaign_player_object.view!=&campaign_player_view||
       rf_object_registry_lookup(&campaign_registry,handle)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&campaign_player_view)return RF_FORMAT;
    for(i=0;i<stage->count;i++){
        scene_npc_checkpoint_restore_entry *e=stage->entries+i;campaign_npc_body candidate;
        if(e->saved.guard_retreat.present){
            if(!isfinite(player->player.player.health)||player->player.player.health<=0)return RF_FORMAT;
            status=scene_guard_retreat_checkpoint_validate(&e->guard_retreat);if(status)return status;
            status=scene_guard_retreat_player_onfoot(scene_actor_collision_owner);if(status)return status;
        }
        if(!e->saved.flight.present)continue;
        if(e->slot>=campaign_npc_body_count)return RF_FORMAT;
        candidate=campaign_npc_bodies[e->slot];
        candidate.ai_mode.action_280=e->effective_ai_mode;candidate.view.action_520=e->effective_ai_mode;
        candidate.ai_mode.state_2b4=0; /* This qualified grammar admits only substate0. */
        candidate.damage.effects.health=e->saved.health;candidate.damage.effects.armor=e->saved.armor;
        candidate.object_flags=e->flags;candidate.body.state=e->body;
        candidate.view.weapons[0]=e->saved.primary;candidate.view.weapons[1]=e->saved.secondary;
        candidate.patrol.enabled=e->saved.route.patrol&1;
        memset(&candidate.script_move,0,sizeof(candidate.script_move));
        candidate.script_move.active=e->saved.move.active;
        candidate.script_move.path=e->move_path;
        candidate.navigation.retained.count=e->saved.move.retained_count;
        candidate.script_look.active=e->saved.look.active;
        candidate.script_shoot.active=e->saved.combat.active==1;
        candidate.script_once.count=e->saved.shot_count;
        candidate.combat_alert=e->saved.combat_alert;candidate.combat_scripted=e->saved.combat.active;
        candidate.combat_target=e->attack_target_handle;candidate.controller_handle=e->controller_handle;
        status=scene_medic_flight_restore_admit(&candidate,e->effective_ai_mode,&e->flight,handle);
        if(status)return status;
    }
    stage->flight_player=handle;stage->flight_joined=1;return RF_OK;
}
#endif
