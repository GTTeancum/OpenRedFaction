#ifndef RF_CAMPAIGN_PICKUPS_CHECKPOINT_H
#define RF_CAMPAIGN_PICKUPS_CHECKPOINT_H
#include "rf/campaign.h"
enum {RF_CAMPAIGN_PICKUPS_CHECKPOINT_MAX_BYTES=64+RF_CAMPAIGN_PICKUP_LEVELS*64+RF_CAMPAIGN_PICKUP_SLOTS*16};
/* RFIP1: LE64-byte identity/checksum header, level names64, then
 * level-index/UID/retired rows12. RFIP2 adds one pickup-state word per row:
 * 0 inherits class policy, 1 enables pickup, 2 disables pickup. Slot order and
 * original retired bits are unchanged; legacy encode still writes RFIP1.
 * No allocation. Disjoint inputs/outputs; validation precedes all writes.
 * Caller validates authored identities and binds pickup_taken after restore. */
int rf_campaign_pickups_checkpoint_encode(const unsigned char identity[32],const rf_campaign_pickups *,void *,uint32_t capacity,uint32_t *written);
int rf_campaign_pickups_checkpoint_preflight(const void *,uint32_t bytes,const unsigned char identity[32]);
int rf_campaign_pickups_checkpoint_decode(const void *,uint32_t bytes,const unsigned char identity[32],rf_campaign_pickups *);
/* State-aware calls retain RFIP1 when every supplied state is zero. Decode
 * RFIP1 clears the state buffer. Capacity is bytes/slots, at most the fixed
 * campaign limit and at least the encoded count. Validation is atomic. Legacy
 * decode rejects RFIP2 instead of discarding policy. Inputs/outputs disjoint. */
int rf_campaign_pickups_checkpoint_encode_states(const unsigned char identity[32],const rf_campaign_pickups *,
    const uint8_t *states,uint32_t state_capacity,void *,uint32_t capacity,uint32_t *written);
int rf_campaign_pickups_checkpoint_decode_states(const void *,uint32_t bytes,const unsigned char identity[32],
    rf_campaign_pickups *,uint8_t *states,uint32_t state_capacity);
#endif
