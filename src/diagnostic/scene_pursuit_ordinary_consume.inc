#ifndef RF_SCENE_PURSUIT_ORDINARY_CONSUME_INCLUDED
#define RF_SCENE_PURSUIT_ORDINARY_CONSUME_INCLUDED
/* Uncalled once-only adapter after token matcher and scope declarations.
 * Invoke immediately before the existing fully qualifying ordinary service,
 * after cheap maintenance at identical now. Other owner slots leave the sole
 * token alone. The selected slot consumes it BEFORE any refusal or service;
 * no second attempt, refund, replacement selection, or later token refresh.
 * selected is private writable storage disjoint from the result and actors.
 * Result belongs to the currently registered private tick scope. Caller still
 * enforces command invalidation, epoch exits and complete service lifetimes.
 */
static int scene_pursuit_ordinary_consume(scene_pursuit_work_dispatch_result *r,
    uint32_t slot,uint32_t frame,int32_t now,uint32_t *selected)
{
    int status;
    if(!r||!selected||r!=scene_pursuit_token_scope)return RF_RANGE;
    *selected=UINT32_MAX;
    if(!r->ordinary_valid||r->ordinary.slot!=slot)return RF_OK;
    r->ordinary_valid=0;
    status=scene_pursuit_ordinary_token_matches(&r->ordinary,frame,now);
    if(status==RF_NOT_FOUND)return RF_OK;
    if(status)return status;
    *selected=slot;return RF_OK;
}
#endif
