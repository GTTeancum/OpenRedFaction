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
/* Owned mission counters. Zero-initialize for a new campaign. Persistence is
 * explicit here; the authored Goal_Create flag mapping belongs to the loader.
 * Capacity is a first-pass Xbox budget, not an original executable limit. */
#define RF_CAMPAIGN_GOALS_MAX 64
typedef struct rf_campaign_goal {
    char name[256];
    int32_t value;
    uint32_t persistent;
} rf_campaign_goal;
typedef struct rf_campaign_goals {
    uint32_t count;
    rf_campaign_goal items[RF_CAMPAIGN_GOALS_MAX];
} rf_campaign_goals;
int rf_campaign_goal_declare(rf_campaign_goals *goals,const char *name,uint32_t persistent);
int rf_campaign_goal_adjust(rf_campaign_goals *goals,const char *name,uint32_t on);
int rf_campaign_goal_check(const rf_campaign_goals *goals,const char *name,int32_t threshold,uint32_t *passed);
/* Drop section-local counters; retained counters survive declarations on revisit. */
int rf_campaign_goals_next_section(rf_campaign_goals *goals);
#endif
