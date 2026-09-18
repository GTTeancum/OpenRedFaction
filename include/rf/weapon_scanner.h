#ifndef RF_WEAPON_SCANNER_H
#define RF_WEAPON_SCANNER_H
#include "rf/model.h"
#include "rf/object_registry.h"
#define RF_WEAPON_SCANNER_CAPACITY 32u
#define RF_WEAPON_SCANNER_DEFAULT_RANGE 100.0f
/* Port marker policy, not original thermal silhouettes. weapons.tbl rail_gun
 * has has_scanner, but no scanner-specific range;100 is an explicit port
 * default matching the present precision ray. AI attack range50 is NOT reused.
 * Caller supplies living NPC centers and eligibility; no world/cover query. */
typedef struct rf_weapon_scanner_candidate {
    uint32_t handle,eligible;const void *identity;float center[3],health;
} rf_weapon_scanner_candidate;
typedef struct rf_weapon_scanner_marker {
    uint32_t handle,index;float screen[2],depth,distance;
} rf_weapon_scanner_marker;
typedef struct rf_weapon_scanner_result {
    uint32_t count,truncated;rf_weapon_scanner_marker markers[RF_WEAPON_SCANNER_CAPACITY];
} rf_weapon_scanner_result;
/* Select up to32 nearest distinct live registered centers in front of camera,
 * within spherical range and strict viewport bounds. Stable ties preserve input
 * order. Uses rf_model_project_vertex with supplied rotation/scales/offsets;
 * depth is positive camera Z, not reciprocal-Z. No allocations or occlusion.
 * Snapshot/registry stable during call. Dead/ineligible/stale owners skipped
 * before center validation. Invalid live input preserves result. Bounds crossing
 * screen with center offscreen is intentionally omitted in this first pass. */
int rf_weapon_scanner_select(const rf_object_registry *,const rf_weapon_scanner_candidate *,
    uint32_t count,const rf_model_projection *,float range,uint32_t width,uint32_t height,
    rf_weapon_scanner_result *);
#endif
