#ifndef RF_SCENE_AUTHORED_DEATH_ITEMS_INC
#define RF_SCENE_AUTHORED_DEATH_ITEMS_INC
/* Narrow installed profiles only. Stable persistence IDs are not table ordinals
 * or placed-item UIDs. Unknown/empty/none retain original lookup failure (-1). */
static int scene_death_item_kind(int32_t id)
{
    switch(id){
    case RF_CAMPAIGN_ITEM_MEDICAL_KIT:return 1;
    case RF_CAMPAIGN_ITEM_12MM_AMMO:return 3;
    case RF_CAMPAIGN_ITEM_556MM_AMMO:return 5;
    case RF_CAMPAIGN_ITEM_10GAUGE_AMMO:return 9;
    default:return -1;
    }
}
/* Only demanded, supported pickup kinds may select a durable definition. */
static int32_t scene_death_item_definition(int kind)
{
    switch(kind){
    case 1:return RF_CAMPAIGN_ITEM_MEDICAL_KIT;
    case 3:return RF_CAMPAIGN_ITEM_12MM_AMMO;
    case 5:return RF_CAMPAIGN_ITEM_556MM_AMMO;
    case 9:return RF_CAMPAIGN_ITEM_10GAUGE_AMMO;
    default:return -1;
    }
}
static int32_t scene_death_item_count(int32_t id)
{
    switch(id){
    case RF_CAMPAIGN_ITEM_MEDICAL_KIT:return 25;
    case RF_CAMPAIGN_ITEM_12MM_AMMO:return 32;
    case RF_CAMPAIGN_ITEM_556MM_AMMO:return 42;
    case RF_CAMPAIGN_ITEM_10GAUGE_AMMO:return 8;
    default:return -1;
    }
}
static int scene_death_item_authored(uint32_t actor,int32_t *id)
{
    char name[RF_LEVEL_NAME_CAPACITY];const rf_level_owned_entity *record;int status;
    if(!id||!campaign_seeds.records.items||actor>=campaign_seeds.records.count)return RF_RANGE;
    record=campaign_seeds.records.items+actor;
    /* The already-supported generated Masako dependency has no authored span
     * and no configured item. Do not silently accept any other missing span. */
    if(scene_masako.enabled&&actor==scene_masako.actor_slot&&!record->raw&&
       !record->record.bytes&&record->record.uid==SCENE_MASAKO_GENERATED_UID){*id=-1;return RF_OK;}
    status=rf_level_entity_death_item_read(record,name);if(status)return status;
    *id=scene_pickup_name_equal(name,"Medical Kit")?RF_CAMPAIGN_ITEM_MEDICAL_KIT:
        scene_pickup_name_equal(name,"12mm_ammo")?RF_CAMPAIGN_ITEM_12MM_AMMO:
        scene_pickup_name_equal(name,"5.56mm_ammo")?RF_CAMPAIGN_ITEM_556MM_AMMO:
        scene_pickup_name_equal(name,"10gauge_ammo")?RF_CAMPAIGN_ITEM_10GAUGE_AMMO:-1;
    return RF_OK;
}
static int scene_death_item_owner_admit(const campaign_npc_body *owner,const rf_campaign_item_drop *drop)
{
    int status;if(!owner||!drop)return RF_RANGE;
    status=rf_campaign_item_drop_validate(drop);if(status)return status;
    if(!drop->state)return RF_OK;
    if(drop->stable_definition_id!=owner->death.item_82c||
       drop->quantity!=scene_death_item_count(drop->stable_definition_id))return RF_FORMAT;
    return RF_OK;
}
static int scene_death_item_current_slot(uint32_t slot,int *current)
{
    const rf_campaign_object_record *key;
    if(!current||rf_scene_defeated_actors.count>RF_CAMPAIGN_ACTOR_SLOTS||
       rf_scene_defeated_actors.level_count>RF_CAMPAIGN_PICKUP_LEVELS||slot>=rf_scene_defeated_actors.count)return RF_RANGE;
    key=rf_scene_defeated_actors.items+slot;
    if(key->level>=rf_scene_defeated_actors.level_count||
       !memchr(campaign_current_level,0,sizeof(campaign_current_level))||
       !memchr(rf_scene_defeated_actors.levels[key->level],0,64))return RF_FORMAT;
    *current=scene_pickup_name_equal(campaign_current_level,rf_scene_defeated_actors.levels[key->level]);return RF_OK;
}
static int scene_death_item_resource_admit(const scene_pickup_resource *resource,int32_t id)
{
    const rf_item_definition *d;uint32_t i;
    if(!resource||scene_death_item_kind(id)<0)return RF_FORMAT;d=&resource->definition;
    if(d->count!=scene_death_item_count(id)||d->mesh_kind!=1||d->gives_weapon||d->flags&1u||
       !resource->model.parts||!resource->model.part_count)return RF_FORMAT;
    switch(id){
    case RF_CAMPAIGN_ITEM_MEDICAL_KIT:
        if(d->weapon[0]||!scene_pickup_name_equal(d->mesh,"powerup_healthpack01.V3D"))return RF_FORMAT;
        break;
    case RF_CAMPAIGN_ITEM_12MM_AMMO:
        if(!scene_pickup_name_equal(d->weapon,"12mm handgun")||
           !scene_pickup_name_equal(d->mesh,"Item_pistol_ammo.V3D"))return RF_FORMAT;
        break;
    case RF_CAMPAIGN_ITEM_556MM_AMMO:
        if(!scene_pickup_name_equal(d->weapon,"Assault Rifle")||
           !scene_pickup_name_equal(d->mesh,"Item_556mm_ammo.V3D"))return RF_FORMAT;
        break;
    case RF_CAMPAIGN_ITEM_10GAUGE_AMMO:
        if(!scene_pickup_name_equal(d->weapon,"Shotgun")||
           !scene_pickup_name_equal(d->mesh,"Item_Shotgun_ammo.V3D"))return RF_FORMAT;
        break;
    default:return RF_FORMAT;
    }
    for(i=0;i<resource->model.part_count;i++)
        if(!isfinite(resource->model.parts[i].maximum[0]))return RF_FORMAT;
    return RF_OK;
}
/* A failed mandatory item may not disappear when this scene's owners close.
 * Refuse successful section departure before any handoff history mutation. */
static int scene_death_item_handoff_admit(void)
{
    uint32_t i;
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].death_item_fault)return RF_FORMAT;
    return RF_OK;
}
static int scene_death_item_resource_demand(uint32_t *mask)
{
    uint32_t i,j,value=0;int kind,current,status;
    if(!mask||campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items||
        campaign_seeds.records.count<campaign_npc_body_count)))return RF_RANGE;
    /* Include terminal retained seeds too: an older valid save may restore an
     * available item after collection. Only the four actual authored profiles
     * can add residency; no all-class load or death-time archive read. */
    for(i=0;i<campaign_npc_body_count;i++) {
        kind=scene_death_item_kind(campaign_npc_bodies[i].death.item_82c);
        if(kind>=0)value|=1u<<kind;
    }
    if(rf_scene_defeated_actors.count>RF_CAMPAIGN_ACTOR_SLOTS)return RF_RANGE;
    for(i=0;i<rf_scene_defeated_actors.count;i++) {
        const rf_campaign_item_drop *drop=rf_scene_defeated_actors.item_drops+i;
        status=scene_death_item_current_slot(i,&current);if(status)return status;
        if(!current||!drop->state)continue;
        for(j=0;j<campaign_npc_body_count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==
            rf_scene_defeated_actors.items[i].uid)break;
        if(j==campaign_npc_body_count)return RF_FORMAT;
        status=scene_death_item_owner_admit(campaign_npc_bodies+j,drop);if(status)return status;
        if(drop->state==RF_CAMPAIGN_ITEM_DROP_AVAILABLE)value|=1u<<scene_death_item_kind(drop->stable_definition_id);
    }
    *mask=value;return RF_OK;
}
#endif
