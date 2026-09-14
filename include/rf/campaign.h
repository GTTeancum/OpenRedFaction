#ifndef RF_CAMPAIGN_H
#define RF_CAMPAIGN_H
#include "rf/weapon.h"
/* First-pass section handoff, not an on-disk save format. No handles/pointers.
 * Catalog hash must match before importing weapon indices. Placement and mission
 * flags are separate; transient firing/reload state is intentionally omitted. */
typedef struct rf_campaign_player_state {
    rf_weapon_inventory inventory;
    float health,armor;
    uint32_t weapon,catalog_hash;
} rf_campaign_player_state;
/* Living players only; failure preserves destination. No allocation. */
int rf_campaign_player_copy(rf_campaign_player_state *destination,
    const rf_campaign_player_state *source,uint32_t catalog_hash);
#endif
