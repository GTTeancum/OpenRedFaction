#ifndef RF_CAMPAIGN_PICKUPS_CHECKPOINT_H
#define RF_CAMPAIGN_PICKUPS_CHECKPOINT_H
#include "rf/campaign.h"
enum {RF_CAMPAIGN_PICKUPS_CHECKPOINT_MAX_BYTES=64+RF_CAMPAIGN_PICKUP_LEVELS*64+RF_CAMPAIGN_PICKUP_SLOTS*12};
/* RFIP1 component: LE64-byte identity/checksum header, level names64, then
 * level-index/UID/retired rows12. Preserve slot order for existing bindings.
 * No allocation. Disjoint inputs/outputs; validation precedes all writes.
 * Caller validates authored identities and binds pickup_taken after restore. */
int rf_campaign_pickups_checkpoint_encode(const unsigned char identity[32],const rf_campaign_pickups *,void *,uint32_t capacity,uint32_t *written);
int rf_campaign_pickups_checkpoint_preflight(const void *,uint32_t bytes,const unsigned char identity[32]);
int rf_campaign_pickups_checkpoint_decode(const void *,uint32_t bytes,const unsigned char identity[32],rf_campaign_pickups *);
#endif
