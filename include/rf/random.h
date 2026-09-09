#ifndef RF_RANDOM_H
#define RF_RANDOM_H
#include "rf/vpp.h"
/* Original 57312d CRT state at thread-data +14. The runtime owns and seeds
 * each stream explicitly; no host rand(), implicit seed or hidden global. */
typedef struct rf_random_state {uint32_t value;} rf_random_state;
/* One original 15-bit draw, with modulo-2^32 state advancement. Arguments
 * must not alias. Invalid pointers preserve output/state. */
int rf_random_next(rf_random_state *state,uint32_t *result);
#endif
