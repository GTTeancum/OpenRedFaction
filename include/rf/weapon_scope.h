#ifndef RF_WEAPON_SCOPE_H
#define RF_WEAPON_SCOPE_H
#include <stdint.h>
/* Practical two-state aiming policy, not retail variable magnification RE.
 * World default90 degrees matches preview projection; scoped25 is a labelled
 * port default. Sniper weapons.tbl FOV65 describes the first-person mesh,
 * not a recovered scoped world FOV. Callers may supply different finite FOVs. */
#define RF_WEAPON_SCOPE_WORLD_FOV 90.0f
#define RF_WEAPON_SCOPE_ZOOM_FOV 25.0f
typedef struct rf_weapon_scope {uint32_t active,held;} rf_weapon_scope;
typedef struct rf_weapon_scope_result {
    uint32_t active,changed;
    float horizontal_fov,projection_scale,look_scale;
} rf_weapon_scope_result;
/* Call every input tick, even when another weapon is selected or dead. A fresh
 * alternate-button press toggles; release alone does nothing. Losing selected
 * or alive resets zoom but still consumes held input (no accidental zoom when
 * switching back while held). Zero initialize on new scene/load/respawn.
 * Scale applies around viewport center to WORLD projection, not HUD/viewmodel.
 * look_scale is inverse projection scale for approximately constant screen aim.
 * No ammo, rendering, allocation or input ownership. Errors preserve outputs. */
int rf_weapon_scope_step(rf_weapon_scope *,uint32_t alternate,uint32_t selected,
    uint32_t alive,float world_fov,float zoom_fov,rf_weapon_scope_result *);
#endif
