#ifndef RF_SCENE_NANO_SHIELD_INC
#define RF_SCENE_NANO_SHIELD_INC
/* Event76 ON4b9b60/OFF4ba380: mutate actor814 bit20 only.
 * Include after campaign_set_invulnerable. Existing generic damage admission
 * already consumes these flags through rf_entity_armor_immunity.
 * Ambient actor-slot binding and shield presentation remain separate systems; no invented hum or armor refill. */
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
/* 4c5a90..4c5b5e: contact-owned shield debit precedes generic damage.
 * Firearms currently use this prepass; projectile contact/bursts remain separate.
 * Rail consumes100 armor regardless of its ordinary health damage. The contact
 * that breaks the shield is still consumed, with no health spill-through.
 * Exhausted armor is clamped to0 for the port's nonnegative save/vitals contract. */
uint32_t rf_scene_nano_contact[8]; /* contacts,breaks,invulnerable,last UID,amount,armor,health,last weapon */
static int scene_nano_firearm_contact(campaign_npc_body *owner,int32_t weapon,float amount,uint32_t *consumed)
{
    uint32_t cls;float adjusted;
    if(!owner||!consumed||!isfinite(amount)||amount<0)return RF_RANGE;
    *consumed=0;cls=campaign_seeds.items[owner-campaign_npc_bodies].class_index;
    if(cls>=campaign_seeds.class_count)return RF_FORMAT;
    if(!rf_entity_armor_immunity(owner->damage.effects.armor,campaign_seeds.classes[cls].physics.flags,
        owner->damage.effects.flags_814))return RF_OK;
    adjusted=weapon==campaign_rail_id?100.f:amount;
    if(!isfinite(owner->damage.effects.armor))return RF_FORMAT;
    *consumed=1;++rf_scene_nano_contact[0];
    if(owner->object_flags&4u)++rf_scene_nano_contact[2];
    else {
        owner->damage.effects.armor=fmaxf(0,owner->damage.effects.armor-adjusted);
        if(owner->damage.effects.armor==0)++rf_scene_nano_contact[1];
    }
    rf_scene_nano_contact[3]=(uint32_t)campaign_seeds.records.items[owner-campaign_npc_bodies].record.uid;
    memcpy(rf_scene_nano_contact+4,&adjusted,4);memcpy(rf_scene_nano_contact+5,&owner->damage.effects.armor,4);
    memcpy(rf_scene_nano_contact+6,&owner->damage.effects.health,4);rf_scene_nano_contact[7]=(uint32_t)weapon;
    return RF_OK;
}
#endif
