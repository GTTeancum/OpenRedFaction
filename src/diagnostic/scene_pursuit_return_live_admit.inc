#ifndef RF_SCENE_PURSUIT_RETURN_LIVE_ADMIT_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_LIVE_ADMIT_INCLUDED
/* Uncalled bounded admission for enqueue AND every execution boundary.
 * This deliberately retains ordinary tactical no-active ownership policy.
 * Pending return must exclude ordinary fire and competing chase refresh;
 * stronger owners must complete explicit cancellation. A refusal alone is
 * not that scheduling/cancellation policy and must not strand a continuation.
 * No callbacks, queries, allocation or live writes; output on success only. */
static int scene_pursuit_return_live_admit(uint32_t slot,float *primary_limit)
{
    const campaign_npc_body *o;float limit;int status;
    if(!primary_limit)return RF_RANGE;
    status=scene_pursuit_return_primary_limit(slot,&limit);if(status)return status;
    o=campaign_npc_bodies+slot;
    if(o->view.weapons[0]<=0||o->view.weapons[0]>=64||
       !o->inventory.owned[o->view.weapons[0]]||o->ai_mode.state_2b4==11)return RF_NOT_FOUND;
    for(uint32_t i=0;i<64;i++)if(o->firing.active[i])return RF_NOT_FOUND;
    *primary_limit=limit;return RF_OK;
}
#endif
