#ifndef RF_CAMPAIGN_GOALS_CHECKPOINT_H
#define RF_CAMPAIGN_GOALS_CHECKPOINT_H
#include "rf/campaign.h"
enum { RF_CAMPAIGN_GOALS_CHECKPOINT_HEADER=64,
    RF_CAMPAIGN_GOALS_CHECKPOINT_GOAL_ROW=264,
    RF_CAMPAIGN_GOALS_CHECKPOINT_LOCAL_ROW=324,
    RF_CAMPAIGN_GOALS_CHECKPOINT_MAX_BYTES=64+64*264+64*324 };
/* RFGC1 LE header: magic/version/bytes/checksum/goal_count/local_count,
 * caller identity32, reserved8. Active goal rows: name256/value/persistent;
 * local rows: level64/name256/value. Signed counters retain all int32 values.
 * Names retain spelling and complete fixed-width storage; inactive slots zero
 * on decode. ASCII case-insensitive duplicate keys are rejected. No allocation.
 * Caller buffers/owners/identity/written must be disjoint. All outputs remain
 * unchanged on failure; decode preflights the complete payload before copying.
 * Component only: caller supplies source identity and owns scene integration. */
int rf_campaign_goals_checkpoint_encode(const unsigned char identity[32],
    const rf_campaign_goals *,const rf_campaign_local_goals *,void *,uint32_t capacity,uint32_t *written);
int rf_campaign_goals_checkpoint_preflight(const void *,uint32_t bytes,const unsigned char identity[32]);
int rf_campaign_goals_checkpoint_decode(const void *,uint32_t bytes,const unsigned char identity[32],
    rf_campaign_goals *,rf_campaign_local_goals *);
#endif
