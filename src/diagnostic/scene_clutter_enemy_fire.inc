#ifndef SCENE_CLUTTER_ENEMY_FIRE_INC
#define SCENE_CLUTTER_ENEMY_FIRE_INC
/* Include after firearm select/damage, combat_shot_obstructed and fragment
 * helpers, before scene_ai_shotgun.inc. Call after resolving nearest intended
 * actor/shield/vehicle fraction, BEFORE the existing fragment dispatch.
 * consumed=1 ends this ray/pellet even when nearer world/rubble blocks it.
 * Caller preserves existing rail penetration policy through penetrates_world.
 * Damage ownership/presentation remains the existing live clutter service. */
static int campaign_clutter_enemy_fire(scene_stream *stream,const float start[3],const float delta[3],
    float limit,float damage,int32_t kind,uint32_t penetrates_world,uint32_t *consumed)
{
    uint32_t slot,blocked;float fraction;int status;
    if(!stream||!start||!delta||!consumed||!isfinite(limit)||limit<0||limit>1||
       !isfinite(damage)||damage<0||kind<0||kind>8||penetrates_world>1)return RF_RANGE;
    status=campaign_clutter_firearm_select(start,delta,limit,&slot,&fraction);if(status)return status;
    if(slot==UINT32_MAX){*consumed=0;return RF_OK;}
    if(!penetrates_world){
        /* Check world before applying any damage to either clutter or rubble. */
        status=combat_shot_obstructed(stream,start,delta,fraction,&blocked);if(status)return status;
        if(blocked){*consumed=1;return RF_OK;}
        status=combat_enemy_fragment_shot(stream,start,delta,fraction,damage,&blocked);if(status)return status;
        if(blocked){*consumed=1;return RF_OK;}
    }
    status=campaign_clutter_firearm_damage(slot,damage,kind);if(status)return status;
    *consumed=1;return RF_OK;
}
#endif
