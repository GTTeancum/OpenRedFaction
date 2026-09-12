#ifndef RF_CORPSE_EFFECT_H
#define RF_CORPSE_EFFECT_H
#include "rf/vpp.h"

typedef struct rf_corpse_surface_effect {
    float elapsed,extent,growth_time,max_extent,growth_rate;
    float position[3],basis[9];
    uint32_t descriptor,color;
    struct rf_corpse_surface_effect *next,*previous;
} rf_corpse_surface_effect;
typedef struct rf_corpse_surface_pool {
    rf_corpse_surface_effect *free,*active;
    uint32_t capacity;
} rf_corpse_surface_pool;
/* Original42dbb0 resets exactly eight slots, preserving their payloads.
 * Storage stays caller-owned; reset discards both previous rings. */
#define RF_CORPSE_SURFACE_CAPACITY 8u
int rf_corpse_surface_reset(rf_corpse_surface_pool *,rf_corpse_surface_effect slots[RF_CORPSE_SURFACE_CAPACITY]);
/* Original42e190 advances elapsed only; no expiry or extent update.
 * Caller supplies valid bounded rings and finite elapsed values. */
int rf_corpse_surface_tick(rf_corpse_surface_pool *,float dt);

typedef struct rf_corpse_surface_source {
    uint32_t descriptor,model;
    float position[3],basis[9];
} rf_corpse_surface_source;
typedef struct rf_corpse_surface_hit {
    float point[3],normal[3];uint32_t face;
} rf_corpse_surface_hit;
typedef struct rf_corpse_surface_backend {
    uint32_t (*metadata)(void *,uint32_t model);
    int32_t (*lookup)(void *,uint32_t metadata,const char *name,uint32_t fallback);
    int (*place)(void *,const rf_corpse_surface_source *,int32_t attachment,float point[3]);
    /* Original4df690: descriptor room tree, start=point, delta=(0,-1,0),
     * radius=0, flags=4, initial limit=FLT_MAX. Misses set matched=0. */
    int (*surface)(void *,uint32_t descriptor,const float point[3],rf_corpse_surface_hit *,uint32_t *matched);
    int (*color)(void *,uint32_t face,const float point[3],uint32_t *color);
    void *context;
} rf_corpse_surface_backend;
/* Original42dc50 construction with supplied attachment/room/color owners.
 * Caller supplies valid circular rings within capacity; no allocation occurs.
 * Full pools recycle greatest elapsed (first tie) BEFORE attachment lookup.
 * Lookup/query misses retain the reserved free slot and return RF_OK.
 * Callbacks must not mutate rings or reenter. Errors after reservation do not
 * roll it back; color errors can leave the reserved slot's position changed.
 * Finite inputs, positive growth time. Rendering/ticking/retirement separate. */
int rf_corpse_surface_create(rf_corpse_surface_pool *,const rf_corpse_surface_source *,
    uint32_t enabled,const char *attachment,float growth_time,float max_extent,
    const rf_corpse_surface_backend *);
/* Original42dc00 eye/spine dispatch. Rereads flags after the eye callback.
 * First backend error stops dispatch; original callbacks have no error status. */
int rf_corpse_source_effects(rf_corpse_surface_pool *,const rf_corpse_surface_source *,
    const uint32_t *flags,uint32_t enabled,const rf_corpse_surface_backend *);
#endif
