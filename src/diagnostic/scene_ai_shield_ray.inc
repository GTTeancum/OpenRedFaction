#ifndef RF_SCENE_AI_SHIELD_RAY_INC
#define RF_SCENE_AI_SHIELD_RAY_INC
/* Direct NPC gunfire against its resolved NPC victim. Include after
 * scene_riot_shield_query.inc and scene_riot_shield_body_order.inc.
 * Existing body spheres remain ordinary damage admission. A held shield is
 * independently discoverable outside those spheres; actual posed body
 * triangles before it veto protection even when the spheres overlap it.
 * No durability mutation until caller resolves world/rubble cover and commits.
 */
enum {SCENE_AI_SHIELD_RAY_MISS=0,SCENE_AI_SHIELD_RAY_BODY=1,SCENE_AI_SHIELD_RAY_SHIELD=2};
typedef struct scene_ai_shield_ray_selection {
    uint32_t kind;float fraction;scene_npc_shield_candidate shield;
} scene_ai_shield_ray_selection;
static int scene_ai_shield_ray_select(uint32_t victim_slot,const float start[3],const float end[3],
    uint32_t body_hit,float body_fraction,scene_ai_shield_ray_selection *out)
{
    scene_ai_shield_ray_selection selected={0};uint32_t found,before;int status;
    if(!start || !end || !out || body_hit>1 || !isfinite(body_fraction) || body_fraction<0 || body_fraction>1)return RF_RANGE;
    selected.kind=body_hit?SCENE_AI_SHIELD_RAY_BODY:SCENE_AI_SHIELD_RAY_MISS;
    selected.fraction=body_hit?body_fraction:1;
    if(!scene_npc_shields.owners){*out=selected;return RF_OK;}
    /* Do not cap at coarse body entry: an outstretched shield may lie inside
     * an enclosing sphere while remaining in front of every body triangle. */
    status=scene_npc_shield_query(victim_slot,start,end,1,&selected.shield,&found);if(status)return status;
    if(found){
        status=scene_npc_shield_body_before(&selected.shield,start,end,&before);if(status)return status;
        if(!before){selected.kind=SCENE_AI_SHIELD_RAY_SHIELD;selected.fraction=selected.shield.hit.time;}
        /* If exact body precedes the shield, retain existing body-sphere
         * admission/cover fraction. Never convert a body miss into protection. */
    }
    *out=selected;return RF_OK;
}
/* Call only after cover to selection.fraction passed. Use original complete
 * ray endpoints; candidate.limit remains1 for strict-nearer revalidation.
 * accepted=1 consumes that pellet/ray, including the hit that breaks shield.
 * If commit rejects a changed candidate, caller may use its original body_hit
 * to continue normal body damage. Source/request remain caller-owned unchanged.
 */
static int scene_ai_shield_ray_commit(const scene_ai_shield_ray_selection *selected,
    const float start[3],const float end[3],const rf_damage_request *request,
    uint32_t *accepted,uint32_t *broken)
{
    if(!selected || !request || !accepted || !broken)return RF_RANGE;
    *accepted=*broken=0;
    if(selected->kind!=SCENE_AI_SHIELD_RAY_SHIELD)return RF_OK;
    return scene_npc_shield_commit(&selected->shield,start,end,request->amount,request->kind,accepted,broken);
}
#endif
