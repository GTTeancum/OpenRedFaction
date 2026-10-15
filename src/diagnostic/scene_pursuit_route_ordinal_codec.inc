#ifndef RF_SCENE_PURSUIT_ROUTE_ORDINAL_CODEC_INCLUDED
#define RF_SCENE_PURSUIT_ROUTE_ORDINAL_CODEC_INCLUDED
/* UNCALLED pure representation bridge. Internal graph tokens use globals
 * [0,n), goal n, start n+1. Existing checkpoint capture/restore uses globals
 * [1,n], goal UINT32_MAX-1, start UINT32_MAX. This converts one token only;
 * it does not validate a route, authorize publication or seal geometry.
 * Count must be the same retained graph identity on both sides. No pointer
 * lookup, allocation, actor mutation or wire/schema extension occurs here. */
static int scene_pursuit_route_token_to_checkpoint(uint32_t token,
    uint32_t global_count,uint32_t *ordinal)
{
    uint32_t out;
    if(!ordinal)return RF_RANGE;
    if(global_count>65534||token>global_count+1)return RF_FORMAT;
    if(token==global_count+1)out=UINT32_MAX;
    else if(token==global_count)out=UINT32_MAX-1;
    else out=token+1;
    *ordinal=out;return RF_OK;
}
static int scene_pursuit_route_token_from_checkpoint(uint32_t ordinal,
    uint32_t global_count,uint32_t *token)
{
    uint32_t out;
    if(!token)return RF_RANGE;
    if(global_count>65534)return RF_FORMAT;
    if(ordinal==UINT32_MAX)out=global_count+1;
    else if(ordinal==UINT32_MAX-1)out=global_count;
    else{
        if(!ordinal||ordinal>global_count)return RF_FORMAT;
        out=ordinal-1;
    }
    *token=out;return RF_OK;
}
#endif
