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
 *  - Respawn (178220 loads the authored seconds: 0 becomes -1, else
 *    seconds * 1000 ms; 16cba0 on a pickup: negative removes the item,
 *    positive hides it (189ff0) and starts timer +0x280; 16ce10 shows it
 *    again (16cda0, sound 1) once the timer expires, checked every 100 ms).
 *  - Spin (16c5e0): in multiplayer an item whose class has items.tbl flag
 *    `spins_in_multi` (bit 2) is drawn turned about the vertical by its own
 *    angle +0x290, advanced 0.75 * pi rad/s while it is drawn.
 * Port: the touch test is the player's (within 2 units of the body origin);
 * remote charges and the riot shield are left to the player; pickup and
 * reappear sounds are not played; the respawn is checked every frame rather
 * than every 100 ms; respawn and spin apply in multiplayer only (single
 * player keeps taken items retired for saves). */
uint32_t rf_scene_mp_pickups[8]; /* health, armour, weapon or ammo, weapon switches, full refusals, back to fighting, items respawned, items spinning */
#define SCENE_MP_ITEMS 256u
static int32_t scene_mp_item_due[SCENE_MP_ITEMS]; /* ms left until shown again; 0 none */
static float scene_mp_item_angle[SCENE_MP_ITEMS];
static uint8_t scene_mp_item_was_taken[SCENE_MP_ITEMS];
static const rf_item_definition *scene_mp_item_definition(const scene_stream *stream,uint32_t i)
{
    int kind=pickup_class(stream->pickups.items[i].class_name);
    if(kind<0)return NULL;
    return kind?(stream->pickup_resources?&stream->pickup_resources[kind-1].definition:NULL):&stream->handgun_pickup;
}
/* 16c5e0: drawing basis of a spinning multiplayer item; 0 = use authored. */
static uint32_t scene_mp_item_basis(const scene_stream *stream,uint32_t i,float basis[9])
{
    const rf_item_definition *definition;float c,s;
    if(!rf_scene_mp[0] || i>=SCENE_MP_ITEMS)return 0;
    definition=scene_mp_item_definition(stream,i);if(!definition || !(definition->flags&2u))return 0;
    c=cosf(scene_mp_item_angle[i]);s=sinf(scene_mp_item_angle[i]);
    basis[0]=c;basis[1]=0;basis[2]=-s;basis[3]=0;basis[4]=1;basis[5]=0;basis[6]=s;basis[7]=0;basis[8]=c;
    return 1;
}
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
/* Respawn timers and spin for every item, whoever took it (16ce10, 16c5e0). */
static void scene_mp_items_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t i,count=stream->pickups.count<SCENE_MP_ITEMS?stream->pickups.count:SCENE_MP_ITEMS;
    int32_t step=(int32_t)(scene_step_seconds*1000.0f+.5f);
    if(!frame){memset(scene_mp_item_due,0,sizeof(scene_mp_item_due));memset(scene_mp_item_angle,0,sizeof(scene_mp_item_angle));
        for(i=0;i<count;i++)scene_mp_item_was_taken[i]=stream->pickup_taken[i];}
    rf_scene_mp_pickups[7]=0;
    for(i=0;i<count;i++){
        const rf_level_item *item=stream->pickups.items+i;const rf_item_definition *definition;
        if(stream->pickup_taken[i] && !scene_mp_item_was_taken[i] && item->respawn_seconds>0)
            scene_mp_item_due[i]=item->respawn_seconds*1000;
        if(stream->pickup_taken[i] && scene_mp_item_due[i]>0){
            scene_mp_item_due[i]-=step;
            if(scene_mp_item_due[i]<=0){
                scene_mp_item_due[i]=0;stream->pickup_taken[i]=0;
                rf_scene_campaign_pickups.items[stream->pickup_slots[i]].retired=0;++rf_scene_mp_pickups[6];
            }
        }
        scene_mp_item_was_taken[i]=stream->pickup_taken[i];
        definition=scene_mp_item_definition(stream,i);
        if(!stream->pickup_taken[i] && definition && (definition->flags&2u)){
            scene_mp_item_angle[i]+=scene_step_seconds*.75f*3.1415927f;
            if(scene_mp_item_angle[i]>6.2831855f)scene_mp_item_angle[i]-=6.2831855f;
            ++rf_scene_mp_pickups[7];
        }
    }
}
static int scene_mp_bot_pickups(scene_stream *stream,uint32_t frame)
{
    uint32_t slot,i,k,count;int status;
    if(!rf_scene_mp[0])return RF_OK;
    scene_mp_items_tick(stream,frame);
    if(rf_scene_mp[6])return RF_OK;
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
