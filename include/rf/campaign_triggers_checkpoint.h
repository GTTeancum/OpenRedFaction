#ifndef RF_CAMPAIGN_TRIGGERS_CHECKPOINT_H
#define RF_CAMPAIGN_TRIGGERS_CHECKPOINT_H
#include "rf/event.h"
enum { RF_CAMPAIGN_TRIGGERS_CHECKPOINT_HEADER=64,
    RF_CAMPAIGN_TRIGGERS_CHECKPOINT_ROW=40,
    RF_CAMPAIGN_TRIGGERS_CHECKPOINT_MAX_BYTES=64+RF_CAMPAIGN_PICKUP_LEVELS*64+RF_CAMPAIGN_TRIGGER_SLOTS*40 };
/* RFTC1: LE magic/version/bytes/checksum/level_count/count/identity32/reserved8;
 * level names64 then rows level/uid/retired/flags/count/object_flags/activation
 * time bits/limit/cooldown remaining/contact remaining. Uses existing session
 * save policy: caller captures current live owners with rf_runtime_trigger_save
 * before encode and recreates handles/volumes/links before runtime restore.
 * Transient contact bit64 must already be cleared. This does not save events,
 * actors, pickups, switches, missions or external side effects. Caller validates
 * authored UID membership and identity. No allocation, complete preflight before
 * publication, outputs unchanged on error; all buffers/owners must be disjoint. */
int rf_campaign_triggers_checkpoint_encode(const unsigned char identity[32],
    const rf_campaign_triggers *,void *,uint32_t capacity,uint32_t *written);
int rf_campaign_triggers_checkpoint_preflight(const void *,uint32_t bytes,const unsigned char identity[32]);
int rf_campaign_triggers_checkpoint_decode(const void *,uint32_t bytes,const unsigned char identity[32],rf_campaign_triggers *);
#endif
