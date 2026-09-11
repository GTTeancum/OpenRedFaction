#ifndef RF_TIMER_H
#define RF_TIMER_H
#include "rf/vpp.h"
#include "rf/random.h"
#define RF_TIMER_PERIOD INT32_C(1072800000)
typedef struct rf_game_clock { int32_t game_ms, real_ms, pause_depth; } rf_game_clock;
/* Original 0x4fa2d0/320/330. Inclusive [0,PERIOD] clocks; forward delta
 * <=PERIOD. Pause nesting stops only game_ms. Invalid input is unchanged. */
int rf_clock_advance(rf_game_clock *clock, int32_t delta_ms);
int rf_clock_pause(rf_game_clock *clock);
int rf_clock_resume(rf_game_clock *clock);
/* 0x4fa360: signed offsets within one period, with strict > wrap.
 * Negative offsets create past deadlines, not disabled timers. */
int rf_timer_set(int32_t *deadline, int32_t now_ms, int32_t offset_ms);
/* 0x4fa350/3e0. All negative deadlines are inactive to the query routines. */
void rf_timer_clear(int32_t *deadline);
/* 0x4fa3f0/420. Half-period comparison disambiguates wrap. Disabled timers
 * are not expired and have PERIOD remaining; elapsed timers return <=0. */
int rf_timer_expired(int32_t deadline, int32_t now_ms, int *expired);
int rf_timer_remaining(int32_t deadline, int32_t now_ms, int32_t *remaining_ms);
/*408ec0: timer active AND not expired. Disabled timers never block AI.
 * Uses the same bounded clock/deadline domain as rf_timer_expired. */
int rf_timer_pending(int32_t deadline,int32_t now_ms,int *pending);
/*4fa3b0: one CRT draw, inclusive integer modulo range, then4fa360.
 * Even minimum==maximum advances RNG. Offset endpoints must lie within one
 * period and be ordered. Deadline and RNG must be disjoint; errors preserve
 * both. Caller owns the shared stream and actual invocation order. */
int rf_timer_set_random(int32_t *deadline,int32_t now_ms,int32_t minimum,
    int32_t maximum,rf_random_state *random);
#endif
