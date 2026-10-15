#ifndef RF_SCENE_PURSUIT_TOKEN_SCOPE_INCLUDED
#define RF_SCENE_PURSUIT_TOKEN_SCOPE_INCLUDED
/* Transient registry for the staged tick wrapper, included after dispatch result types. A tick
 * wrapper owns the private result for the entire synchronous body, starting
 * before paid services with both valid flags clear. Every normal exit must
 * unregister that exact address. Reject nested entry before any side effects.
 * No actor/save layout change. Caller supplies authenticated current identity
 * at a successful command/retirement commit; attempted commands do not count.
 * This helper never dereferences actor storage retained in either token.
 */
static scene_pursuit_work_dispatch_result *scene_pursuit_token_scope;
static int scene_pursuit_token_scope_begin(scene_pursuit_work_dispatch_result *result)
{
    if(!result)return RF_RANGE;
    if(scene_pursuit_token_scope)return RF_FORMAT;
    memset(result,0,sizeof(*result));
    scene_pursuit_token_scope=result;return RF_OK;
}
static int scene_pursuit_token_scope_end(scene_pursuit_work_dispatch_result *result)
{
    if(!result)return RF_RANGE;
    if(scene_pursuit_token_scope!=result)return RF_FORMAT;
    scene_pursuit_token_scope=NULL;return RF_OK;
}
/* Accepted whole-player restart also replaces every NPC combat relation.
 * Clear only transient selection validity; keep exact-address scope cleanup. */
static void scene_pursuit_token_restart(void)
{
    scene_pursuit_work_dispatch_result *r=scene_pursuit_token_scope;
    if(r)r->ordinary_valid=r->cancelled_valid=0;
}
/* A committed target death invalidates its selected decisions before effects.
 * Compare full handles only; never inspect retained target/actor storage. */
static void scene_pursuit_token_invalidate_target(uint32_t handle)
{
    scene_pursuit_work_dispatch_result *r=scene_pursuit_token_scope;
    if(!r)return;
    if(r->ordinary_valid&&r->ordinary.target==handle)r->ordinary_valid=0;
    if(r->cancelled_valid&&r->cancelled_owner.target==handle)r->cancelled_valid=0;
}
static uint32_t scene_pursuit_token_identity(const scene_pursuit_ordinary_token *t,
    uint64_t epoch,const campaign_npc_body *owners,uint32_t slot,uint32_t handle)
{
    return t->epoch==epoch&&t->owners==owners&&t->slot==slot&&t->handle==handle;
}
static void scene_pursuit_token_invalidate(uint64_t epoch,
    const campaign_npc_body *owners,uint32_t slot,uint32_t handle)
{
    scene_pursuit_work_dispatch_result *r=scene_pursuit_token_scope;
    if(!r)return;
    if(r->ordinary_valid&&scene_pursuit_token_identity(&r->ordinary,epoch,owners,slot,handle))
        r->ordinary_valid=0;
    if(r->cancelled_valid&&scene_pursuit_token_identity(&r->cancelled_owner,epoch,owners,slot,handle))
        r->cancelled_valid=0;
}
#endif
