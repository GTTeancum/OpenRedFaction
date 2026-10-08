#ifndef RF_SCENE_WEAPON_DROP_FEEDBACK_INC
#define RF_SCENE_WEAPON_DROP_FEEDBACK_INC
/* Existing scene weapon slots -> installed items.tbl class indices. Remote
 * Charge uses the single-charge class. Undercover's authored display name is
 * Silenced 12mm Pistol; class30 supplies only that presentation, never its
 * different Gives Weapon grant. Detonator/shield/alternate resources cannot
 * emit death drops through scene_enemy_drop_weapon_slot. */
static const signed char scene_weapon_drop_pickup_kinds[17]={
    0,4,6,8,14,15,13,16,11,-1,20,-1,31,23,24,25,30
};
/* metadata loaded,metadata failures,last load status,accepted feedback,
 * notice requests,sound requests,last weapon,last corpse UID */
uint32_t rf_scene_weapon_drop_feedback[8];
static void scene_weapon_drop_feedback_open(scene_stream *stream,rf_vpp *tables)
{
    uint32_t slot;memset(rf_scene_weapon_drop_feedback,0,sizeof(rf_scene_weapon_drop_feedback));
    if(!stream->pickup_resources)return;
    for(slot=0;slot<17;slot++) {
        int kind=scene_weapon_drop_pickup_kinds[slot],status;rf_item_definition *definition;
        if(kind<0)continue;
        definition=kind?&stream->pickup_resources[kind-1].definition:&stream->handgun_pickup;
        if(!definition->mesh_kind) {
            /* Metadata only, in the already allocated class catalog: a level
             * need not place this item to receive the same weapon from an NPC.
             * No new model/texture demand and no table reads during pickup. */
            status=rf_item_definition_load(tables,pickup_classes[kind],128*1024,definition);
            if(status){++rf_scene_weapon_drop_feedback[1];rf_scene_weapon_drop_feedback[2]=(uint32_t)status;continue;}
        }
        ++rf_scene_weapon_drop_feedback[0];
    }
}
static void scene_weapon_drop_feedback(scene_stream *stream,const rf_campaign_weapon_drop *drop,
    const rf_weapon_pickup_grant *grant,uint32_t uid)
{
    int slot=scene_enemy_drop_weapon_slot(&campaign_weapon_supply.names,drop->weapon);
    const rf_item_definition *definition=NULL;
    if(slot>=0 && slot<17) {
        int kind=scene_weapon_drop_pickup_kinds[slot];
        if(kind==0)definition=&stream->handgun_pickup;
        else if(kind>0 && stream->pickup_resources)definition=&stream->pickup_resources[kind-1].definition;
        if(definition && (!definition->mesh_kind || !definition->weapon[0]))definition=NULL;
    }
    ++rf_scene_weapon_drop_feedback[3];rf_scene_weapon_drop_feedback[6]=(uint32_t)drop->weapon;
    rf_scene_weapon_drop_feedback[7]=uid;
    /* Inventory and one-time retirement have already committed. Both calls
     * are the ordinary placed-item feedback; missing audio is nonfatal. */
    campaign_pickup_sound(definition,drop->position,uid);++rf_scene_weapon_drop_feedback[5];
    if(definition) {
        uint32_t displayed=grant->rounds;
        if(displayed && (drop->weapon==campaign_riot_id || drop->weapon==campaign_flame_id)) {
            int32_t ignored,units;
            /*45a5a0..45a69c reports hundredths as whole batteries/canisters.
             * Use only the existing display result; inventory already holds
             * the exact accepted charge, including partial-to-cap pickups. */
            if(!rf_weapon_pickup_amount((int32_t)displayed,0,INT32_MAX,0,1,1,&ignored,&units))
                displayed=(uint32_t)units;
        }
        campaign_pickup_notice_grant(definition->pickup_messages,grant->acquired,displayed);
        ++rf_scene_weapon_drop_feedback[4];
    }
}
#endif
