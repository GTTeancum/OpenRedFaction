#ifndef RF_SCENE_AI_MELEE_DELAYED_GUARD_INCLUDED
#define RF_SCENE_AI_MELEE_DELAYED_GUARD_INCLUDED
/* Include in scene_ai_drone_smash.inc immediately before its impact function,
 * after strike, live and geometry definitions. No scheduler/lifetime changes.
 * The callback is read-only: no target selection, animation step or mutation. */
typedef struct scene_ai_melee_delayed_guard {
    scene_ai_melee_guard common;
    const scene_ai_drone_smash_strike *slot,*strike;
} scene_ai_melee_delayed_guard;
static uint32_t scene_ai_melee_delayed_valid(void *context)
{
    scene_ai_melee_delayed_guard *guard=context;campaign_npc_body *owner,*victim;
    const rf_physics_body *body;const float *eye;const rf_weapon_primary_definition *primary;
    float delta[3],distance;
    if(!guard || !guard->slot || !guard->strike ||
       guard->slot->ticket!=guard->strike->ticket || guard->slot->phase!=3 ||
       !scene_ai_melee_guard_valid(&guard->common))return 0;
    return scene_ai_drone_smash_live(guard->strike,guard->common.player_eye,&owner,&victim,&eye,&body,&primary) &&
        scene_ai_drone_smash_geometry(owner,eye,primary,delta,&distance);
}
#endif
