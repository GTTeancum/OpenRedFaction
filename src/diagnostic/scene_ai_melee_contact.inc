#ifndef RF_SCENE_AI_MELEE_CONTACT_INCLUDED
#define RF_SCENE_AI_MELEE_CONTACT_INCLUDED
/* First-pass NPC melee uses the same bounded collision segment as player
 * melee: the intended target must occupy the strike, and nearer world/rubble
 * absorbs it. A swing can damage rubble; it cannot also damage the actor.
 * Cadence and animation remain with enemy_tick. This is not an animation-
 * marker reconstruction or a swept weapon-bone hit volume. */
static int campaign_enemy_melee_contact(scene_stream *stream,const float start[3],
    const float target_delta[3],float reach,const rf_physics_body *target,
    float damage,uint32_t *contact)
{
    float squared=0,ray[3],fraction=1;uint32_t i,body_hit,blocked;int status;
    if(!stream || !start || !target_delta || !target || !contact ||
       !isfinite(reach) || reach<=0 || !isfinite(damage) || damage<0)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(start[i]) || !isfinite(target_delta[i]))return RF_RANGE;
        squared+=target_delta[i]*target_delta[i];
    }
    if(!isfinite(squared))return RF_RANGE;
    if(squared<.0001f){*contact=0;return RF_OK;}
    for(i=0;i<3;i++)ray[i]=target_delta[i]*(reach/sqrtf(squared));
    body_hit=combat_body(start,ray,target,1,&fraction);
    if(!body_hit)fraction=1;
    status=combat_enemy_fragment_shot(stream,start,ray,fraction,damage,&blocked);if(status)return status;
    *contact=body_hit && !blocked;return RF_OK;
}
#endif
