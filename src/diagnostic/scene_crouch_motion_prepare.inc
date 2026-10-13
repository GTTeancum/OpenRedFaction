#ifndef RF_SCENE_CROUCH_MOTION_PREPARE_INCLUDED
#define RF_SCENE_CROUCH_MOTION_PREPARE_INCLUDED
/* Ordinary action3 portion of41f61d..41f7c1. The armed/combat gate selects
 * special9 at41f6a9; the ordinary fallback selects8 at41f6ee. It is NOT a
 * walking/stationary test. The turn-action candidate helper and movement
 * tail remain with their existing owners. This prepares controller state
 * privately; the caller performs actual clearance/ground work separately. */
typedef struct scene_crouch_motion_candidate {
    rf_motion_controller controller;
    rf_motion_stance_decision stance;
    uint32_t priority_handled,query_required;
} scene_crouch_motion_candidate;
static int scene_crouch_motion_prepare(const rf_motion_controller *before,
    const int32_t motions[23],const rf_motion_priority *priority,
    const rf_motion_crouch_input *eligibility,uint32_t combat_eligible,
    uint32_t query_resolved,uint32_t query_blocked,
    scene_crouch_motion_candidate *out)
{
    scene_crouch_motion_candidate next={0};uint32_t eligible;int handled=0,status;
    if(!before||!motions||!priority||!eligibility||!out||combat_eligible>1||
       query_resolved>1||query_blocked>1||(!query_resolved&&query_blocked)||
       eligibility->action!=3||priority->action!=3||
       eligibility->mode!=priority->mode)return RF_RANGE;
    next.controller=*before;
    status=rf_motion_select_priority(&next.controller,motions,priority,&handled);
    if(status)return status;
    next.priority_handled=!!handled;
    if(handled){*out=next;return RF_OK;}
    status=rf_motion_crouch_eligibility(eligibility,&eligible);if(status)return status;
    if(eligible==RF_MOTION_CROUCH_QUERY){
        if(!query_resolved){
            /* No live publication while the visibility result is pending. */
            next.controller=*before;next.query_required=1;*out=next;return RF_OK;
        }
        eligible=query_blocked?RF_MOTION_CROUCH_NO:RF_MOTION_CROUCH_YES;
    }
    status=rf_motion_select_stance(&next.controller,motions,combat_eligible?9:8,
        eligible==RF_MOTION_CROUCH_YES,priority->flags,&next.stance);
    if(status)return status;
    *out=next;return RF_OK;
}
#endif
