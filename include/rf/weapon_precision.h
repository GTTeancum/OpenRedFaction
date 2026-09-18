#ifndef RF_WEAPON_PRECISION_H
#define RF_WEAPON_PRECISION_H
#include "rf/hitscan_select.h"

#define RF_WEAPON_PRECISION_HITS 32u
enum rf_weapon_precision_mode {
    RF_WEAPON_PRECISION_SNIPER = 0,
    RF_WEAPON_PRECISION_RAIL = 1
};
typedef struct rf_weapon_precision_result {
    uint32_t count,truncated;
    rf_hitscan_selection hits[RF_WEAPON_PRECISION_HITS];
} rf_weapon_precision_result;

/* Working port shot policy, not an original dispatch reconstruction. Sniper
 * selects one target up to the inclusive world obstruction fraction. Rail
 * ignores that obstruction and pierces targets along the complete supplied
 * ray, retaining the closest32 distinct live handles, ordered near to far.
 * Exact ties retain snapshot order. Multiple shapes with the same handle
 * produce one hit at the nearest fraction. truncated means rail hit storage
 * omitted additional targets; no allocation or unbounded scene-owned scratch.
 *
 * Uses rf_hitscan_select registry/identity validation and optional trace; all
 * candidate eligibility (including self, dead actors and penetrable clutter)
 * remains caller-owned. Wall penetration does not modify terrain. Snapshot,
 * registry and callback data must remain stable and callbacks must be pure.
 * Errors preserve result. Revalidate identity before damage publication, and
 * use hits[i].index to recover the original candidate. This only gathers hits:
 * caller consumes ammo once, publishes damage/effects, and manages scope input.
 * An empty result is a successful miss, not a dry-fire event. */
int rf_weapon_precision_select(const rf_object_registry *,
    const rf_hitscan_candidate *,uint32_t count,
    const float start[3],const float delta[3],float world_fraction,
    uint32_t mode,rf_hitscan_trace,void *context,rf_weapon_precision_result *);
#endif
