/* Authored terrain only. Clone before mutation: replaying history into an
 * already mutated live owner is not rollback-safe when allocation fails.
 * Independent of scene_stream so real-core tests exercise the same wrapper.
 * The caller owns admission/RNG/effects; do not change them before success.
 */
#include "rf/geomod.h"
#include "rf/geomod_piece_bank.h"
#include <stdlib.h>
#include <string.h>
#define SCENE_TERRAIN_EDIT_HISTORY_BYTES RF_GEOMOD_HISTORY_MAX_BYTES
typedef struct scene_terrain_edit_limits {
    uint32_t total_budget;
    /* Everything other than old core, clone core and this history buffer.
     * Include active publication/atlas AND maximum pending publication,
     * lighting staging, factory scratch and other concurrent reservations. */
    uint32_t external_peak_bytes;
    uint32_t clone_budget;
} scene_terrain_edit_limits;
typedef struct scene_terrain_edit_callbacks {
    /* Recreate identical source, filters, cavity and mapping. Enforce budget
     * on all core allocations. Output starts NULL; cleanup even on failure. */
    int (*create)(void *,uint32_t budget,rf_geomod_terrain **);
    int (*mutate)(void *,rf_geomod_terrain *);
    /* Borrow candidate without changing *live. Stage everything first.
     * Failure MUST preserve active render/collision/lighting. Success means
     * complete final publication; no fallible operations after its commit.
     * Use serial for render/cache generation, not candidate core generation.
     * Never free candidate or old core; no reentry/frames during callback. */
    int (*publish)(void *,rf_geomod_terrain *,uint32_t serial);
    /* Discard pending publication/lighting before candidate is destroyed.
     * Safe even when publication has not run. Cannot fail. */
    void (*abort)(void *);
} scene_terrain_edit_callbacks;
typedef struct scene_terrain_edit_result {
    uint32_t reserved_peak_bytes,history_bytes,serial,core_generation;
} scene_terrain_edit_result;
/* One room transaction may change up to four independently owned sources.
 * Source callbacks create/mutate only; group publication is called once after
 * every candidate and piece registry is ready. No source publish is called. */
typedef struct scene_terrain_edit_source {
    rf_geomod_terrain **live;
    const scene_terrain_edit_callbacks *ops;
    void *context;
    rf_geomod_piece_registry *pieces;
    uint32_t replace_pieces;
    uint32_t fresh; /* Explicit reset: recreate immutable source, omit old history. */
} scene_terrain_edit_source;
typedef struct scene_terrain_edit_group_callbacks {
    int (*publish)(void *,rf_geomod_terrain *const *,uint32_t count,uint32_t serial);
    void (*abort)(void *);
} scene_terrain_edit_group_callbacks;
static int scene_terrain_edit_transaction_group(const scene_terrain_edit_source *sources,uint32_t count,
    uint32_t *serial,const scene_terrain_edit_limits *limits,const scene_terrain_edit_group_callbacks *ops,
    void *context,scene_terrain_edit_result *out)
{
    rf_geomod_terrain *candidate[4]={0},*old[4]={0};rf_geomod_terrain_view before,after;
    scene_terrain_edit_result result[4];unsigned char *history=NULL;
    uint64_t reserved;uint32_t bytes,i,j,next,history_needed=0,begun[4]={0};int status;
    if(!sources || !count || count>4 || !serial || *serial==UINT32_MAX || !limits ||
       !limits->clone_budget || !ops || !ops->publish || !ops->abort || !out)return RF_RANGE;
    reserved=(uint64_t)limits->external_peak_bytes+
        (uint64_t)count*limits->clone_budget;
    for(i=0;i<count;i++) {
        const scene_terrain_edit_source *source=sources+i;
        if(source->fresh>1 || (source->fresh && source->pieces && !source->replace_pieces))return RF_RANGE;
        if(!source->fresh)history_needed=1;
        if(!source->live || !*source->live || !source->ops || !source->ops->create || !source->ops->mutate)return RF_RANGE;
        for(j=0;j<i;j++)if(*source->live==old[j] ||
            (source->pieces && source->pieces==sources[j].pieces))return RF_RANGE;
        old[i]=*source->live;status=rf_geomod_terrain_get(old[i],&before);if(status)return status;
        reserved+=before.resident_bytes;
    }
    if(history_needed)reserved+=SCENE_TERRAIN_EDIT_HISTORY_BYTES;
    if(reserved>limits->total_budget)return RF_RANGE;
    if(history_needed){history=malloc(SCENE_TERRAIN_EDIT_HISTORY_BYTES);if(!history)return RF_IO;}
    for(i=0;i<count;i++) {
        const scene_terrain_edit_source *source=sources+i;
        bytes=0;
        if(!source->fresh){status=rf_geomod_terrain_history_size(old[i],&bytes);if(status)goto rejected;}
        if(bytes>SCENE_TERRAIN_EDIT_HISTORY_BYTES){status=RF_RANGE;goto rejected;}
        result[i].history_bytes=bytes;
        if(source->pieces) {
            status=rf_geomod_piece_registry_begin(source->pieces,source->replace_pieces);if(status)goto rejected;
            begun[i]=1;
        }
        if(!source->fresh){status=rf_geomod_terrain_history_encode(old[i],history,bytes);if(status)goto rejected;}
        status=source->ops->create(source->context,limits->clone_budget,candidate+i);
        for(j=0;j<count;j++)if(candidate[i]==old[j]){candidate[i]=NULL;status=RF_FORMAT;break;}
        for(j=0;j<i;j++)if(candidate[i] && candidate[i]==candidate[j]){candidate[i]=NULL;status=RF_FORMAT;break;}
        if(status)goto rejected;
        if(!candidate[i]){status=RF_FORMAT;goto rejected;}
        if(source->pieces) {
            status=rf_geomod_terrain_set_extraction(candidate[i],rf_geomod_piece_registry_emit,source->pieces);
            if(status)goto rejected;
        }
        if(!source->fresh){status=rf_geomod_terrain_history_decode(candidate[i],history,bytes);if(status)goto rejected;}
        if(source->pieces && !source->fresh) {
            if(source->replace_pieces) {
                rf_geomod_piece_registry_abort(source->pieces);begun[i]=0;
                status=rf_geomod_piece_registry_begin(source->pieces,1);if(!status)begun[i]=1;
            } else status=rf_geomod_piece_registry_rewind(source->pieces);
            if(status)goto rejected;
        }
        status=source->ops->mutate(source->context,candidate[i]);if(status)goto rejected;
        status=rf_geomod_terrain_get(candidate[i],&after);if(status)goto rejected;
        if(after.peak_bytes>limits->clone_budget){status=RF_RANGE;goto rejected;}
        result[i].core_generation=after.mesh.generation;
        result[i].reserved_peak_bytes=(uint32_t)reserved;result[i].serial=*serial+1;
    }
    next=*serial+1;status=ops->publish(context,candidate,count,next);if(status)goto rejected;
    /* One successful publication is the commit point for every source. */
    for(i=0;i<count;i++) {
        *sources[i].live=candidate[i];
        if(sources[i].pieces)rf_geomod_piece_registry_commit(sources[i].pieces);
    }
    *serial=next;memcpy(out,result,count*sizeof(*result));
    for(i=0;i<count;i++)rf_geomod_terrain_close(old+i);
    free(history);return RF_OK;
rejected:
    ops->abort(context);
    for(i=0;i<count;i++) {
        if(begun[i])rf_geomod_piece_registry_abort(sources[i].pieces);
        rf_geomod_terrain_close(candidate+i);
    }
    free(history);return status;
}
typedef struct scene_terrain_single_publication {
    const scene_terrain_edit_callbacks *ops;void *context;
} scene_terrain_single_publication;
static int scene_terrain_single_publish(void *opaque,rf_geomod_terrain *const *candidate,uint32_t count,uint32_t serial)
{
    scene_terrain_single_publication *p=opaque;
    if(count!=1)return RF_RANGE;
    return p->ops->publish(p->context,candidate[0],serial);
}
static void scene_terrain_single_abort(void *opaque)
{scene_terrain_single_publication *p=opaque;p->ops->abort(p->context);}
static int scene_terrain_edit_transaction_pieces(rf_geomod_terrain **live,uint32_t *serial,
    const scene_terrain_edit_limits *limits,const scene_terrain_edit_callbacks *ops,
    void *context,scene_terrain_edit_result *out,rf_geomod_piece_registry *pieces,uint32_t replace_pieces)
{
    scene_terrain_edit_source source={live,ops,context,pieces,replace_pieces,0};
    scene_terrain_single_publication publication={ops,context};
    scene_terrain_edit_group_callbacks group={scene_terrain_single_publish,scene_terrain_single_abort};
    if(!ops || !ops->publish || !ops->abort)return RF_RANGE;
    return scene_terrain_edit_transaction_group(&source,1,serial,limits,&group,&publication,out);
}

/* Legacy edits retain the same transaction with no detached owner. Registry
 * callers reserve its entire construction budget in external_peak_bytes. */
static int scene_terrain_edit_transaction(rf_geomod_terrain **live,uint32_t *serial,
    const scene_terrain_edit_limits *limits,const scene_terrain_edit_callbacks *ops,
    void *context,scene_terrain_edit_result *out)
{return scene_terrain_edit_transaction_pieces(live,serial,limits,ops,context,out,NULL,0);}
