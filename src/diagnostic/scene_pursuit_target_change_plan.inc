#ifndef RF_SCENE_PURSUIT_TARGET_CHANGE_PLAN_INCLUDED
#define RF_SCENE_PURSUIT_TARGET_CHANGE_PLAN_INCLUDED
/* Uncalled read-only proposal for 409050..409130. Supplied predicates stand
 * for the already resolved source4270a0 and optional48aaf0 decisions; this
 * helper never runs either query, resolves handles or changes owner state.
 * Do not substitute this plan for the caller's complete lifetime/resource
 * join. In particular40a450 is a separate reference update, not a raw write. */
typedef struct scene_pursuit_target_change_plan {
    uint32_t write_target,update_reference,write_last_seen;
    uint32_t copy_target_position,arm_position_clock,arm_sight_clock;
    int32_t target;
} scene_pursuit_target_change_plan;
static int scene_pursuit_target_change(int32_t owner_handle,int32_t old_target,
    int32_t requested_target,int32_t action,uint32_t owner_predicate_4270a0,
    uint32_t filter_enabled,uint32_t target_predicate_48aaf0,
    uint32_t resolved_target_actor,scene_pursuit_target_change_plan *result)
{
    scene_pursuit_target_change_plan out={0};
    if(!result)return RF_RANGE;
    if(owner_handle==-1||(requested_target==-1&&resolved_target_actor)||
       owner_predicate_4270a0>1||filter_enabled>1||
       target_predicate_48aaf0>1||resolved_target_actor>1)return RF_FORMAT;
    out.target=old_target;
    if(requested_target==old_target){*result=out;return RF_OK;}
    if(requested_target==owner_handle){
        /*409070 returns before predicates, timestamps, position or clocks. */
        out.write_target=1;out.target=-1;*result=out;return RF_OK;
    }
    if((!owner_predicate_4270a0&&action!=15)||
       (filter_enabled&&!target_predicate_48aaf0)){
        *result=out;return RF_OK;
    }
    out.write_target=1;out.target=requested_target;
    out.update_reference=(action==3||action==7)&&requested_target!=-1;
    out.write_last_seen=requested_target!=-1;
    out.copy_target_position=resolved_target_actor;
    out.arm_position_clock=resolved_target_actor;
    out.arm_sight_clock=1;
    /* Clock+238 is deliberately absent: source retargeting does not rearm
     * the independent visible-to-hidden delay. RFNC40's portable typed
     * target receipt still requires its documented identity reconciliation. */
    *result=out;return RF_OK;
}
#endif
