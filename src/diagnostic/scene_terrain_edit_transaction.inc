/* Authored terrain only. Clone before mutation: replaying history into an
 * already mutated live owner is not rollback-safe when allocation fails.
 * Independent of scene_stream so real-core tests exercise the same wrapper.
 * The caller owns admission/RNG/effects; do not change them before success.
 */
#include "rf/geomod.h"
#include <stdlib.h>
#define SCENE_TERRAIN_EDIT_HISTORY_BYTES 12380u
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
static int scene_terrain_edit_transaction(rf_geomod_terrain **live,uint32_t *serial,
    const scene_terrain_edit_limits *limits,const scene_terrain_edit_callbacks *ops,
    void *context,scene_terrain_edit_result *out)
{
    rf_geomod_terrain *candidate=NULL,*old;rf_geomod_terrain_view before,after;
    scene_terrain_edit_result result;unsigned char *history=NULL;
    uint64_t reserved;uint32_t bytes,next;int status;
    if(!live || !*live || !serial || !limits || !ops || !out ||
       !ops->create || !ops->mutate || !ops->publish || !ops->abort ||
       !limits->clone_budget || *serial==UINT32_MAX)return RF_RANGE;
    status=rf_geomod_terrain_get(*live,&before);if(status)return status;
    reserved=(uint64_t)limits->external_peak_bytes+before.resident_bytes+
        limits->clone_budget+SCENE_TERRAIN_EDIT_HISTORY_BYTES;
    if(reserved>limits->total_budget)return RF_RANGE;
    status=rf_geomod_terrain_history_size(*live,&bytes);if(status)return status;
    if(bytes>SCENE_TERRAIN_EDIT_HISTORY_BYTES)return RF_RANGE;
    history=malloc(SCENE_TERRAIN_EDIT_HISTORY_BYTES);if(!history)return RF_IO;
    status=rf_geomod_terrain_history_encode(*live,history,bytes);if(status)goto rejected;
    status=ops->create(context,limits->clone_budget,&candidate);
    if(candidate==*live){candidate=NULL;status=RF_FORMAT;goto rejected;}
    if(status)goto rejected;
    if(!candidate){status=RF_FORMAT;goto rejected;}
    status=rf_geomod_terrain_history_decode(candidate,history,bytes);if(status)goto rejected;
    status=ops->mutate(context,candidate);if(status)goto rejected;
    status=rf_geomod_terrain_get(candidate,&after);if(status)goto rejected;
    if(after.peak_bytes>limits->clone_budget){status=RF_RANGE;goto rejected;}
    next=*serial+1;result.reserved_peak_bytes=(uint32_t)reserved;
    result.history_bytes=bytes;result.serial=next;result.core_generation=after.mesh.generation;
    status=ops->publish(context,candidate,next);if(status)goto rejected;
    /* No allocation, validation or other fallible operation after publication. */
    old=*live;*live=candidate;*serial=next;*out=result;
    rf_geomod_terrain_close(&old);free(history);return RF_OK;
rejected:
    ops->abort(context);rf_geomod_terrain_close(&candidate);free(history);return status;
}
