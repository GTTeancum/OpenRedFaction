#ifndef RF_SCENE_NANO_SHIELD_INC
#define RF_SCENE_NANO_SHIELD_INC
/* Event76 ON4b9b60/OFF4ba380: mutate actor814 bit20 only.
 * Include after campaign_set_invulnerable. Existing generic damage admission
 * already consumes these flags through rf_entity_armor_immunity.
 * Ambient actor-slot binding, direct shield-hit armor consumption and shield
 * presentation remain separate systems; no invented hum or armor refill. */
static int campaign_set_nano_shield(void *context,uint32_t handle,uint32_t enabled)
{
    uint32_t i,*flags=NULL;(void)context;
    if(enabled>1)return RF_RANGE;
    if(handle==(uint32_t)campaign_player_view.handle &&
       rf_entity_lookup(&campaign_entities,(int32_t)handle)==&campaign_player_view)
        flags=&campaign_player_damage.state.effects.flags_814;
    else for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *owner=campaign_npc_bodies+i;
        if(!owner->registration.view || owner->registration.handle!=handle ||
           rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration)continue;
        flags=&owner->damage.effects.flags_814;break;
    }
    if(!flags)return RF_NOT_FOUND;
    if(enabled)*flags&=~0x20u;else *flags|=0x20u;
    return RF_OK;
}
#endif
