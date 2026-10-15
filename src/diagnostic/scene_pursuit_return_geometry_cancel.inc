#ifndef RF_SCENE_PURSUIT_RETURN_GEOMETRY_CANCEL_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_GEOMETRY_CANCEL_INCLUDED
/* UNCALLED cancellation adaptation, never fallback completion. Caller proves
 * this tag came from the successful leased query for these captured inputs,
 * holds uninterrupted execution/search ownership and all backing lifetimes,
 * and keeps input private/disjoint. Arbitrary RF_NOT_FOUND has no authority.
 * Cancel only the remaining phase2 continuation after the completed prelude.
 * Preserve AI/tactical/RNG/recovery. Caller suppresses this full owner for the
 * remainder of the frame using a private identity token, invalidated by real
 * command replacement. Ordinary work can resume on a later frame.
 */
static int scene_pursuit_return_geometry_cancel(
    const scene_pursuit_return_input *input,uint32_t tag)
{
    campaign_npc_body *o;int status;
    if(!input)return RF_RANGE;
    if(tag!=SCENE_TACTICAL_QUERY_EMPTY_GRAPH&&tag!=SCENE_TACTICAL_QUERY_NO_START)
        return RF_FORMAT;
    if(!scene_pursuit_execution_active||scene_pursuit_execution_fault||
       !scene_flight_search_busy)return RF_FORMAT;
    if(input->query.before.phase!=SCENE_PURSUIT_RETURN_PENDING_FALLBACK)
        return RF_FORMAT;
    status=scene_pursuit_return_input_matches(input);if(status)return status;
    o=campaign_npc_bodies+input->slot;
    if(o->navigation.retained.count||o->script_move.active||o->script_move.follow||
       o->script_move.path.count||o->script_move.event||
       o->script_move.origin!=SCENE_MOVE_ORIGIN_NONE||
       o->script_animation.active||o->script_shoot.active||o->script_once.count)
        return RF_NOT_FOUND;
    scene_pursuit_return_receipt_clear(&o->pursuit_return);return RF_OK;
}
#endif
