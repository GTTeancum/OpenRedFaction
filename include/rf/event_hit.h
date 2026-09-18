#ifndef RF_EVENT_HIT_H
#define RF_EVENT_HIT_H
#include "rf/level.h"
/* When_Hit52 poll4b8dd0 after common tick4b8ce0. Any still-valid common
 * deadline blocks polling; own disabled flag does not. Query returns current
 * object7c flags; NOT_FOUND skips missing/non-object links. Damage admission
 * sets0x200000 BEFORE immunity. Never consume that signal here: multiple
 * observers must see it until the common object-update clear boundary. */
typedef int (*rf_event_hit_query)(void *,uint32_t handle,uint32_t *flags);
/* Called for every resolved mixed link after any hit signal. Runtime service
 * tries event ON(-1,-1), else mover(-1,-1). No trigger enable fallback. */
typedef int (*rf_event_hit_effect)(void *,uint32_t handle);
int rf_event_hit_poll(int32_t common_deadline,const rf_level_link_target *,uint32_t count,
    rf_event_hit_query,rf_event_hit_effect,void *context);
#endif
