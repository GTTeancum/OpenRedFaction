#ifndef RF_CAMPAIGN_TRIGGERS_CHECKPOINT_H
#define RF_CAMPAIGN_TRIGGERS_CHECKPOINT_H
#include "rf/event.h"
enum { RF_CAMPAIGN_TRIGGERS_CHECKPOINT_HEADER=64,
    RF_CAMPAIGN_TRIGGERS_CHECKPOINT_ROW_V1=40,
    RF_CAMPAIGN_TRIGGERS_CHECKPOINT_ROW=48,
    RF_CAMPAIGN_TRIGGERS_CHECKPOINT_MAX_BYTES=64+RF_CAMPAIGN_PICKUP_LEVELS*64+
        RF_CAMPAIGN_TRIGGER_SLOTS*RF_CAMPAIGN_TRIGGERS_CHECKPOINT_ROW };
/* RFTC1/2: LE magic/version/bytes/checksum/level_count/count/identity32/reserved8;
 * level names64 then rows level/uid/retired/flags/count/object_flags/activation
 * time bits/limit/cooldown remaining/contact remaining. RFTC2 appends chamber
 * UID/pressure encoding: 0=legacy/unowned, 1=pressure0, 2=pressure1. Absent
 * pressure requires UID0; explicit UID cannot be UINT32_MAX. Retained explicit
 * peers keyed by (level,chamber UID) must agree, regardless of trigger flags.
 * Writer uses RFTC2 only when any row retains explicit pressure; otherwise it
 * preserves RFTC1. Legacy decode produces canonical zero pressure and UID.
 * Uses existing full-session save policy: caller captures current live owners
 * with rf_runtime_trigger_save, then supplies settled pressure before encode.
 * Handles/volumes/links are recreated before runtime restore. Transient contact
 * bit64 must already be cleared. No pending airlock actor/deadline is stored.
 * Caller guards pending work and validates authored UID/chamber membership,
 * topology, current-level peer completeness and identity before publication.
 * This does not save events, actors, pickups, switches, missions or external
 * side effects. No allocation, complete preflight before publication, outputs
 * unchanged on error; all buffers/owners must be disjoint. */
int rf_campaign_triggers_checkpoint_encode(const unsigned char identity[32],
    const rf_campaign_triggers *,void *,uint32_t capacity,uint32_t *written);
int rf_campaign_triggers_checkpoint_preflight(const void *,uint32_t bytes,const unsigned char identity[32]);
int rf_campaign_triggers_checkpoint_decode(const void *,uint32_t bytes,const unsigned char identity[32],rf_campaign_triggers *);
#endif
