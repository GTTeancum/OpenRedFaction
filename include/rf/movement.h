#ifndef RF_MOVEMENT_H
#define RF_MOVEMENT_H
#include "rf/vpp.h"
typedef struct rf_movement_settings {
    float response, speed; /* Entity +8c and +8c0. Response meaning pending. */
    int32_t mode; /* Entity +8c4, read by animation selection. */
} rf_movement_settings;
typedef struct rf_movement_config {
    uint32_t flags; /* Entity info +724. */
    float base_speed, slow_factor, alternate_factor, response; /* Info +50..5c. */
    float override_slow, override_normal; /* Globals 594590 / 59458c. */
} rf_movement_config;
/* Complete numeric 0x427450, including flag predicate 0x40a210.
 * A forced action other than -1 forces mode zero. Request zero/2 selects
 * modes zero/2; all other requests select mode one. override_enabled is
 * original byte 64ecb9; entity_scale is +98. Invalid data preserves state. */
int rf_movement_set_mode(rf_movement_settings *state, const rf_movement_config *config,
                         int32_t requested, int32_t forced_action, float entity_scale,
                         uint8_t override_enabled);
#endif
