#ifndef RF_SCENE_BURNING_SOURCE_IDENTITY_INCLUDED
#define RF_SCENE_BURNING_SOURCE_IDENTITY_INCLUDED
/* A burn outlives its ignition source. Original41a350 retains the source
 * handle for damage credit;48684f invalidates that handle at object removal.
 * Keep the authored NPC UID separately for RFAP4, never as a native handle.
 * This identity belongs to one burn and is cleared with that entire record. */
static campaign_npc_body *scene_burning_source_npc(uint32_t uid)
{
    uint32_t i;
    if(!uid||uid>INT32_MAX||!campaign_npc_bodies||!campaign_seeds.records.items)return NULL;
    for(i=0;i<campaign_npc_body_count&&i<campaign_seeds.records.count;i++){
        campaign_npc_body *owner=campaign_npc_bodies+i;
        if((uint32_t)campaign_seeds.records.items[i].record.uid!=uid)continue;
        if(!owner->persistence_registered||owner->persistence_slot>=rf_scene_defeated_actors.count||
           owner->persistence_slot>=RF_CAMPAIGN_ACTOR_SLOTS||
           rf_scene_defeated_actors.items[owner->persistence_slot].uid!=uid)return NULL;
        return owner;
    }
    return NULL;
}
static int scene_burning_source_registered(const campaign_npc_body *owner,uint32_t handle)
{
    return owner&&handle&&handle!=UINT32_MAX&&owner->registration.view==&owner->view&&
        owner->registration.handle==handle&&owner->damage.effects.handle==handle&&
        rf_object_registry_lookup(&campaign_registry,handle)==&owner->registration&&
        rf_entity_lookup(&campaign_entities,(int32_t)handle)==&owner->view;
}
static int scene_burning_source_retired(const campaign_npc_body *owner)
{
    return owner&&!owner->registration.view&&!owner->registration.handle&&owner->view.handle==-1&&
        owner->persistence_registered&&owner->persistence_slot<rf_scene_defeated_actors.count&&
        owner->persistence_slot<RF_CAMPAIGN_ACTOR_SLOTS&&
        rf_scene_defeated_actors.items[owner->persistence_slot].retired&&
        owner->damage.effects.health<=0&&(owner->view.flags_810&1u)&&
        (owner->object_flags&(2u|0x4000u))==(2u|0x4000u);
}
static uint32_t scene_burning_source_uid(uint32_t handle)
{
    uint32_t i;
    if(!handle||handle==UINT32_MAX||!campaign_seeds.records.items)return 0;
    for(i=0;i<campaign_npc_body_count&&i<campaign_seeds.records.count;i++){
        campaign_npc_body *owner=campaign_npc_bodies+i;
        uint32_t uid=(uint32_t)campaign_seeds.records.items[i].record.uid;
        if(scene_burning_source_registered(owner,handle)&&scene_burning_source_npc(uid)==owner)return uid;
    }
    return 0; /* Other supported source kinds keep their existing encoder. */
}
/* Registered corpses retain native attribution. A removed NPC cannot supply
 * affiliation or retaliation, even if its old registry slot has been reused.
 * Never acquire a replacement handle solely by looking up the saved UID. */
static uint32_t scene_burning_source_native(const scene_burning_record *burn)
{
    if(!burn->source_uid)return burn->source;
    return scene_burning_source_registered(scene_burning_source_npc(burn->source_uid),burn->source)?
        burn->source:UINT32_MAX;
}
static int scene_burning_source_checkpoint(const scene_burning_record *burn,uint32_t *uid)
{
    const campaign_npc_body *owner=scene_burning_source_npc(burn->source_uid);
    if(!owner)return RF_FORMAT;
    if(owner->registration.view){
        if(!scene_burning_source_registered(owner,burn->source))return RF_FORMAT;
    }else{
        /* A save can occur in the same frame as Remove_Object, before the
         * burn tick normalizes its native source. Qualify the retained owner,
         * not any object now occupying the old handle's slot. */
        if(!scene_burning_source_retired(owner)||
           (burn->source!=UINT32_MAX&&burn->source!=owner->damage.effects.handle))return RF_FORMAT;
    }
    *uid=burn->source_uid;return RF_OK;
}
#endif
