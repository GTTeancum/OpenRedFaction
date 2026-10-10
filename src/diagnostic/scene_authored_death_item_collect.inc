#ifndef RF_SCENE_AUTHORED_DEATH_ITEM_COLLECT_INC
#define RF_SCENE_AUTHORED_DEATH_ITEM_COLLECT_INC
static int scene_death_items_tick(scene_stream *stream,const float eye[3])
{
    uint32_t i,k;int status,current,kind;
    if(campaign_player_damage.state.effects.health<=0)return RF_OK;
    if(rf_scene_defeated_actors.count>RF_CAMPAIGN_ACTOR_SLOTS)return RF_RANGE;
    for(i=0;i<rf_scene_defeated_actors.count;i++) {
        rf_campaign_item_drop *drop=rf_scene_defeated_actors.item_drops+i;const rf_item_definition *definition;
        rf_weapon_pickup_grant grant={0};float delta[3],distance=0,restored=0;uint32_t blocked=0;
        status=scene_death_item_current_slot(i,&current);if(status)return status;
        if(!current||drop->state!=RF_CAMPAIGN_ITEM_DROP_AVAILABLE)continue;
        status=rf_campaign_item_drop_validate(drop);if(status)return status;
        kind=scene_death_item_kind(drop->stable_definition_id);
        if(kind<0||!stream->pickup_resources)return RF_FORMAT;
        status=scene_death_item_resource_admit(stream->pickup_resources+kind-1,drop->stable_definition_id);if(status)return status;
        definition=&stream->pickup_resources[kind-1].definition;
        if(drop->quantity!=definition->count)return RF_FORMAT;
        for(k=0;k<3;k++){float d=drop->position[k]-scene_actor_body.state.position[k];distance+=d*d;delta[k]=drop->position[k]-eye[k];}
        if(distance>4)continue;
        status=combat_shot_obstructed(stream,eye,delta,1,&blocked);if(status)return status;if(blocked)continue;
        if(kind==1) {
            rf_damage_effect_state *v=&campaign_player_damage.state.effects;float total;
            status=rf_entity_vital_pickup_sp(&v->health,v->class_health,drop->quantity,
                rf_scene_campaign_countdown.difficulty,&restored);if(status)return status;if(restored<=0)continue;
            memcpy(&total,rf_scene_pickup_vitals+2,4);total+=restored;memcpy(rf_scene_pickup_vitals+2,&total,4);
        }else {
            int32_t id=rf_weapon_name_find(&campaign_weapon_supply.names,definition->weapon);
            if(id<0||id>=64)return RF_FORMAT;
            /* This is Ammo For, never a weapon grant or an auto-equip request. */
            status=scene_pickup_shared_ammo_grant(&campaign_player_inventory,&campaign_weapon_supply,
                id,drop->quantity,0,&grant);if(status)return status;if(!grant.rounds)continue;
        }
        /* Ledger first: optional feedback cannot replay a successful grant. */
        drop->state=RF_CAMPAIGN_ITEM_DROP_COLLECTED;
        ++rf_scene_pickups[3];rf_scene_pickups[4]+=grant.rounds;rf_scene_pickups[5]=rf_scene_defeated_actors.items[i].uid;
        campaign_pickup_sound(definition,drop->position,rf_scene_defeated_actors.items[i].uid);
        campaign_pickup_notice_grant(definition->pickup_messages,0,grant.rounds?grant.rounds:(uint32_t)drop->quantity);
        campaign_ammo_publish();
    }
    return RF_OK;
}
#endif
