#ifndef RF_CAMPAIGN_H
#define RF_CAMPAIGN_H
#include "rf/weapon.h"
/* First-pass section handoff, not an on-disk save format. No handles/pointers.
 * Catalog hash must match before importing weapon indices. Placement and mission
 * flags are separate; transient firing/reload state is intentionally omitted. */
typedef struct rf_campaign_player_state {
    rf_weapon_inventory inventory;
    float health,armor;
    uint32_t weapon,catalog_hash; /* UINT32_MAX weapon means unarmed. */
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
/* Section-local goal snapshots. Preserve values only; declarations are rebuilt
 * from the destination level before restore. Fixed bound, no runtime handles. */
#define RF_CAMPAIGN_LOCAL_GOALS_MAX 64
typedef struct rf_campaign_local_goals {
    uint32_t count;
    struct {char level[64],name[256];int32_t value;} items[RF_CAMPAIGN_LOCAL_GOALS_MAX];
} rf_campaign_local_goals;
int rf_campaign_local_goals_save(rf_campaign_local_goals *,const char *level,const rf_campaign_goals *);
int rf_campaign_local_goals_restore(const rf_campaign_local_goals *,const char *level,rf_campaign_goals *);
/* First-pass campaign item persistence, keyed by owned level name and authored
 * UID. Register before gameplay so taking an item never allocates or can exhaust
 * capacity after granting its reward. Zero the owner for a new campaign. */
#define RF_CAMPAIGN_PICKUP_LEVELS 128
#define RF_CAMPAIGN_PICKUP_SLOTS 1024
typedef struct rf_campaign_object_record {uint32_t level,uid,retired;} rf_campaign_object_record;
typedef rf_campaign_object_record rf_campaign_pickup_record;
typedef struct rf_campaign_pickups {
    uint32_t level_count,count;
    char levels[RF_CAMPAIGN_PICKUP_LEVELS][64];
    rf_campaign_pickup_record items[RF_CAMPAIGN_PICKUP_SLOTS];
} rf_campaign_pickups;
int rf_campaign_pickup_register(rf_campaign_pickups *state,const char *level,uint32_t uid,uint32_t *slot);
#define RF_CAMPAIGN_TRIGGER_SLOTS 4096
typedef struct rf_campaign_trigger_state {
    uint32_t flags,count,object_flags,activation_time_bits;
    int32_t limit,cooldown_remaining,contact_remaining;
} rf_campaign_trigger_state;
typedef struct rf_campaign_triggers {
    uint32_t level_count,count;
    char levels[RF_CAMPAIGN_PICKUP_LEVELS][64];
    rf_campaign_object_record items[RF_CAMPAIGN_TRIGGER_SLOTS];
    rf_campaign_trigger_state states[RF_CAMPAIGN_TRIGGER_SLOTS];
} rf_campaign_triggers;
int rf_campaign_trigger_register(rf_campaign_triggers *,const char *level,uint32_t uid,uint32_t *slot);
#define RF_CAMPAIGN_ACTOR_SLOTS 2048
/* First-pass supported weapon drops, keyed by the actor's persistent slot.
 * state: 0 never emitted, 1 available, 2 collected. Fixed48KiB, no handles. */
typedef struct rf_campaign_weapon_drop {
    uint32_t state;int32_t weapon,quantity;float position[3];
} rf_campaign_weapon_drop;
/* Same owned key layout, separate namespace and capacity from pickups. */
typedef struct rf_campaign_actors {
    uint32_t level_count,count;
    char levels[RF_CAMPAIGN_PICKUP_LEVELS][64];
    rf_campaign_object_record items[RF_CAMPAIGN_ACTOR_SLOTS];
    /* Section-local living vitals, keyed by the same slots; no runtime handles.
     * Valid only after capture; retirement takes precedence. Fixed24KiB. */
    struct {uint32_t valid;float health,armor;} vitals[RF_CAMPAIGN_ACTOR_SLOTS];
    /* Captured with vitals: allegiance and authored hidden/invulnerable bits.
     * Other object bits remain owned by normal actor construction. Fixed16KiB. */
    struct {uint32_t affiliation,flags;} mission[RF_CAMPAIGN_ACTOR_SLOTS];
    rf_campaign_weapon_drop drops[RF_CAMPAIGN_ACTOR_SLOTS];
} rf_campaign_actors;
int rf_campaign_actor_register(rf_campaign_actors *state,const char *level,uint32_t uid,uint32_t *slot);
/* Quantity is remaining ammunition: zero still emits an acquirable empty
 * weapon; negatives are invalid. Emission is idempotent even after collection.
 * Invalid input preserves state; an existing drop is never replenished. */
int rf_campaign_actor_drop_emit(rf_campaign_actors *,uint32_t slot,int32_t weapon,int32_t quantity,const float position[3]);
#endif
