#ifndef RF_EVENT_THRESHOLD_H
#define RF_EVENT_THRESHOLD_H
#include "rf/level.h"
/* Original When_Life_Reaches87/4bd400 and When_Armor_Reaches88/4bd500.
 * Authored words[0] is signed threshold. Fresh owners clear fired; same-level
 * checkpoint ownership must preserve it. No common disabled/deadline gate. */
typedef struct rf_event_threshold {int32_t threshold;uint32_t fired;} rf_event_threshold;
typedef int (*rf_event_threshold_query)(void *,uint32_t handle,uint32_t armor,float *value);
/* Effect receives EVERY resolved mixed link after any entity meets threshold.
 * Runtime adapter independently handles event ON(-1,-1), mover(-1,-1), and
 * trigger ENABLE (not activation). Unsupported targets return NOT_FOUND. */
typedef int (*rf_event_threshold_effect)(void *,uint32_t handle);
int rf_event_threshold_poll(rf_event_threshold *,uint32_t armor,
    const rf_level_link_target *,uint32_t count,rf_event_threshold_query,
    rf_event_threshold_effect,void *context);
#endif
