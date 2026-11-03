#ifndef RF_SCENE_MP_PICKUPS_INCLUDED
#define RF_SCENE_MP_PICKUPS_INCLUDED
/* Multiplayer bot item pickups, reconstructed from the PS2 game (SLUS-20073;
 * docs/MP-BOTS-PS2.md section 8). Include after campaign_pickups_tick.
 *  - 16c2f0/16cba0: in multiplayer any character picks items up, not only
 *    the human; the item's handler runs for the picker.
 *  - Health 16dfb0 / armour 16de80: refused when the picker is already at its
 *    class maximum (class +0x44 / +0x48); otherwise the item amount is added
 *    (unscaled in multiplayer) and capped at that maximum.
 *  - Weapons and ammo 16d970: added to the inventory (1327e0); a bot then
 *    takes its preferred usable weapon (154a20 -> 12a700 forced): the first
 *    weapon of its preference row (28e760) that it owns with clip or reserve
 *    ammo (12a680).
 *  - 16cb00: a bot in AI mode 4 or 7 whose life and armour are both above 95%
 *    of the class maximum goes back to fighting (133d80).
 * Port: the touch test is the player's (within 2 units of the body origin);
 * remote charges and the riot shield are left to the player; pickup sounds
 * and item respawn are not ported yet (items stay taken). */
uint32_t rf_scene_mp_pickups[6]; /* health, armour, weapon or ammo, weapon switches, full refusals, back to fighting */
/* 12a700 forced: switch to the first weapon of the bot's row it can fire. */
static int scene_mp_bot_choose_weapon(uint32_t slot)
{
    campaign_npc_body *npc=campaign_npc_bodies+slot;uint32_t i;int32_t id,type;
    for(i=0;i<8;i++){
        id=scene_mp_weapon_ids[scene_mp_preference_rows[slot][i]];
        if(id<0 || id>=64 || (uint32_t)id>=campaign_weapon_supply.names.count || !npc->inventory.owned[id])continue;
        type=campaign_weapon_supply.definitions[id].ammo_type;
        if(type<0 || type>=32)continue;
        if(npc->inventory.loaded[id]<=0 && npc->inventory.reserve[type]<=0)continue;
        if(id==npc->view.weapons[0])return RF_OK;
        /* A weapon the bot's class has no motions for is passed over. */
        if(campaign_enemy_weapon_switch(slot,id))continue;
        ++rf_scene_mp_pickups[3];return RF_OK;
    }
    return RF_OK;
}
static int scene_mp_bot_pickups(scene_stream *stream)
{
    uint32_t slot,i,k,count;int status;
    if(!rf_scene_mp[0] || rf_scene_mp[6])return RF_OK;
    count=campaign_npc_body_count<SCENE_MP_BOTS?campaign_npc_body_count:SCENE_MP_BOTS;
    for(slot=0;slot<count;slot++){
        campaign_npc_body *npc=campaign_npc_bodies+slot;rf_damage_effect_state *v=&npc->damage.effects;
        uint32_t switched=0;
        if(!rf_scene_mp_roster[slot].used || !npc->registration.view || v->health<=0)continue;
        for(i=0;i<stream->pickups.count;i++){
            const rf_level_item *item=stream->pickups.items+i;const rf_item_definition *definition;
            int kind=pickup_class(item->class_name);float distance=0;
            if(stream->pickup_taken[i] || kind<0 || item->quantity<0)continue;
            for(k=0;k<3;k++){float d=item->position[k]-npc->body.state.position[k];distance+=d*d;}
            if(distance>4)continue;
            definition=kind?&stream->pickup_resources[kind-1].definition:&stream->handgun_pickup;
            if(kind==1 || kind==2 || kind==SCENE_PICKUP_FIRST_AID){
                float *value=kind==2?&v->armor:&v->health,limit=kind==2?v->class_armor:v->class_health;
                if(*value>=limit){++rf_scene_mp_pickups[4];continue;}
                *value+=(float)item->quantity;if(*value>limit)*value=limit;
                ++rf_scene_mp_pickups[kind==2?1:0];
            }else{
                rf_weapon_pickup_grant grant={0};int32_t id=-1,weapon_slot=scene_pickup_weapon_slot(kind);
                if(kind==11 || kind==12 || kind==22 || (kind>=SCENE_PICKUP_MINER_SUIT && kind!=SCENE_PICKUP_SILENCED_HANDGUN))continue;
                if(weapon_slot>=0)id=campaign_slot_weapon((uint32_t)weapon_slot);
                else if(definition->weapon[0])id=rf_weapon_name_find(&campaign_weapon_supply.names,definition->weapon);
                if(id<0 || id>=64 || (uint32_t)id>=campaign_weapon_supply.names.count)continue;
                /* A refused grant (full, or a reserve the shared rule rejects) leaves the item. */
                if(scene_pickup_shared_ammo_grant(&npc->inventory,&campaign_weapon_supply,id,
                    campaign_item_charge(id,item->quantity,definition->gives_weapon),definition->gives_weapon,&grant) ||
                   (!grant.rounds && !grant.acquired)){++rf_scene_mp_pickups[4];continue;}
                ++rf_scene_mp_pickups[2];switched=1;
            }
            stream->pickup_taken[i]=1;rf_scene_campaign_pickups.items[stream->pickup_slots[i]].retired=1;
            /* 16cb00: a bot running a route with life and armour above 95% fights again. */
            if(scene_mp_routing[slot]==1 && v->health>v->class_health*.95f && v->armor>v->class_armor*.95f){
                npc->script_move.active=0;npc->navigation.retained.count=0;scene_mp_routing[slot]=0;++rf_scene_mp_pickups[5];
            }
        }
        if(switched){status=scene_mp_bot_choose_weapon(slot);if(status)return status;}
    }
    return RF_OK;
}
#endif
