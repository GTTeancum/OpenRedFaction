#ifndef RF_SCENE_PURSUIT_RUNTIME_ASSEMBLY_INCLUDED
#define RF_SCENE_PURSUIT_RUNTIME_ASSEMBLY_INCLUDED
/* External candidate assembly after all existing scene definitions.
 * This includes reviewed helpers only: dispatch/enqueue/service callers remain
 * disabled, save fences remain, and return_contract_ready stays zero.
 * Declaration order is source-assessed, not compiled or stack/size-proven. */
#include "scene_pursuit_notice_mirrors.inc"
#include "scene_pursuit_recovery.inc"
#include "scene_pursuit_return_normal.inc"
#include "scene_pursuit_return_outcome_plan.inc"
#include "scene_pursuit_return_pending_service.inc"
#include "scene_pursuit_return_primary_limit.inc"
#include "scene_pursuit_route_prepare.inc"
#include "scene_pursuit_route_request_prepare.inc"
#include "scene_pursuit_transition_plan.inc"
#include "scene_pursuit_work_readiness.inc"
#include "scene_pursuit_notice_distribution.inc"
#include "scene_pursuit_return_completion.inc"
#include "scene_pursuit_return_live_admit.inc"
#include "scene_pursuit_return_stop_sample.inc"
#include "scene_pursuit_route_publish.inc"
#include "scene_pursuit_route_select_prepare.inc"
#include "scene_pursuit_work_scheduler.inc"
#include "scene_pursuit_return_attack_enqueue.inc"
#include "scene_pursuit_return_fallback_prepare.inc"
#include "scene_pursuit_return_found_publish.inc"
#include "scene_pursuit_return_reset.inc"
#include "scene_pursuit_return_route_prepare.inc"
#include "scene_pursuit_return_fallback_publish.inc"
#include "scene_pursuit_return_input.inc"
#include "scene_pursuit_return_miss_publish.inc"
#include "scene_pursuit_return_fallback_query.inc"
#include "scene_pursuit_return_geometry_cancel.inc"
#include "scene_pursuit_return_execute.inc"
#include "scene_pursuit_work_dispatch.inc"
#include "scene_pursuit_cancelled_service.inc"
#include "scene_pursuit_ordinary_consume.inc"
#endif
