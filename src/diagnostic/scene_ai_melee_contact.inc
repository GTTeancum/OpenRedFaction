#ifndef RF_SCENE_AI_MELEE_CONTACT_INCLUDED
#define RF_SCENE_AI_MELEE_CONTACT_INCLUDED
/* First-pass NPC melee uses the same bounded collision segment as player
 * melee: the intended target must occupy the strike, and nearer world/rubble
 * or currently posed retained clutter absorbs it. One contact can damage
 * rubble or clutter; it cannot also damage the actor when that cover breaks.
 * Original40956b ->4c77a0 melee flag20 reaches ordinary object/model contact
 * at4c7fc8/4c7fd6 and object damage4c6132. Segment/range remain port policy.
 * Cadence and animation remain with enemy_tick. This is not an animation-
 * marker reconstruction or a swept weapon-bone hit volume. */
static int campaign_enemy_melee_contact(scene_stream *stream,const float start[3],
    const float target_delta[3],float reach,const rf_physics_body *target,
    float damage,int32_t kind,uint32_t *contact)
{
    float squared=0,ray[3],fraction=1,prop_fraction;
    uint32_t i,body_hit,blocked,prop_slot,prop_handle=UINT32_MAX;int status;
    rf_clutter_base_owner *prop=NULL;
    if(!stream || !start || !target_delta || !target || !contact ||
       !isfinite(reach) || reach<=0 || !isfinite(damage) || damage<0 || kind<0 || kind>8)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(start[i]) || !isfinite(target_delta[i]))return RF_RANGE;
        squared+=target_delta[i]*target_delta[i];
    }
    if(!isfinite(squared))return RF_RANGE;
    if(squared<.0001f){*contact=0;return RF_OK;}
    for(i=0;i<3;i++)ray[i]=target_delta[i]*(reach/sqrtf(squared));
    body_hit=combat_body(start,ray,target,1,&fraction);
    if(!body_hit)fraction=1;
    *contact=0;
    status=campaign_clutter_firearm_select(start,ray,fraction,&prop_slot,&prop_fraction);if(status)return status;
    if(prop_slot!=UINT32_MAX) {
        /* Retain the exact selected owner before any obstruction effects.
         * A stale/replaced cover candidate consumes this contact rather than
         * allowing damage through it or hitting the slot's new occupant. */
        if(!campaign_clutter_bodies || prop_slot>=campaign_clutter_records.count ||
           !(prop=campaign_clutter_bodies[prop_slot]) ||
           !scene_clutter_damage_identity(&campaign_registry,prop))return RF_OK;
        prop_handle=prop->state.handle;fraction=prop_fraction;
    }
    /* Do not precheck with combat_shot_obstructed: it also sees rubble and
     * would consume a nearer chunk without the existing direct damage below.
     * This single call orders world/rubble before the retained prop/body. */
    status=combat_enemy_fragment_shot(stream,start,ray,fraction,damage,&blocked);if(status)return status;
    if(blocked)return RF_OK;
    if(prop) {
        if(!campaign_clutter_bodies || prop_slot>=campaign_clutter_records.count ||
           campaign_clutter_bodies[prop_slot]!=prop ||
           rf_object_registry_lookup(&campaign_registry,prop_handle)!=(const void *)prop ||
           prop->state.handle!=prop_handle || (prop->state.flags&(2u|0x4000u)))return RF_OK;
        return campaign_clutter_firearm_damage(prop_slot,damage,kind);
    }
    *contact=body_hit;return RF_OK;
}
#endif
