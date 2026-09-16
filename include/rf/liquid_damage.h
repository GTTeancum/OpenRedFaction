#ifndef RF_LIQUID_DAMAGE_H
#define RF_LIQUID_DAMAGE_H
#include "rf/entity.h"
#include "rf/geometry.h"
/* Exactly12 bytes/room. Dry rows use NaN for both floats; presence is not type. */
typedef struct rf_liquid_room { float minimum_y,depth; int32_t type; } rf_liquid_room;
typedef struct rf_liquid_rooms { rf_liquid_room *items; uint32_t count,bytes; } rf_liquid_rooms;
int rf_liquid_rooms_open(const rf_geometry *,uint32_t budget,rf_liquid_rooms *);
void rf_liquid_rooms_close(rf_liquid_rooms *);
typedef struct rf_liquid_damage_input {
    uint32_t target,actor_flags_810,actor_kind_1fc,room_present;
    int32_t liquid_type; /* Original room+180:2 lava,3 acid;1 water. */
    uint32_t rejection_4290d0; /* Only low byte exactly1 rejects. */
    float frame_seconds,lava_per_second,acid_per_second;
} rf_liquid_damage_input;
typedef struct rf_liquid_damage_result {
    uint32_t emit,target;
    int32_t hit_region; /* Original4892c0 argument4=-1, external to request. */
    rf_damage_request request;
} rf_liquid_damage_result;
/* Complete421240 selection/arithmetic after caller resolves room/predicate.
 * Wet requires either actor flag1000 or2000. Lava also requires kind1fc==3.
 * No oxygen timer, damage accumulation, room lookup or health mutation.
 * Eligible dt0 still emits request0, matching original dispatch. Reached rate
 * and dt must be finite/nonnegative; invalid data preserves output. Ineligible
 * inputs produce zeroed result without reading numeric damage inputs.
 * Submit emitted request through the existing shared SP damage service. */
int rf_liquid_damage_prepare(const rf_liquid_damage_input *,rf_liquid_damage_result *);
#endif
