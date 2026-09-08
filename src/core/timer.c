#include "rf/timer.h"
static int valid_time(int32_t time) { return time>=0 && time<=RF_TIMER_PERIOD; }
static int valid_clock(const rf_game_clock *clock)
{
    return clock && valid_time(clock->game_ms) && valid_time(clock->real_ms) && clock->pause_depth>=0;
}
static int32_t advance_time(int32_t time, int32_t delta)
{
    int32_t gap=RF_TIMER_PERIOD-time;
    return delta>gap ? delta-gap : time+delta;
}
int rf_clock_advance(rf_game_clock *clock, int32_t delta_ms)
{
    if (!valid_clock(clock) || delta_ms<0 || delta_ms>RF_TIMER_PERIOD) return RF_RANGE;
    if (!clock->pause_depth) clock->game_ms=advance_time(clock->game_ms,delta_ms);
    clock->real_ms=advance_time(clock->real_ms,delta_ms);
    return RF_OK;
}
int rf_clock_pause(rf_game_clock *clock)
{
    if (!valid_clock(clock) || clock->pause_depth==INT32_MAX) return RF_RANGE;
    ++clock->pause_depth; return RF_OK;
}
int rf_clock_resume(rf_game_clock *clock)
{
    if (!valid_clock(clock) || !clock->pause_depth) return RF_RANGE;
    --clock->pause_depth; return RF_OK;
}
int rf_timer_set(int32_t *deadline, int32_t now_ms, int32_t offset_ms)
{
    int32_t time;
    if (!deadline || !valid_time(now_ms) || offset_ms < -RF_TIMER_PERIOD || offset_ms>RF_TIMER_PERIOD) return RF_RANGE;
    if (offset_ms>=0) time=advance_time(now_ms,offset_ms);
    else { time=now_ms+offset_ms; if (time<0) time+=RF_TIMER_PERIOD; }
    *deadline=time; return RF_OK;
}
void rf_timer_clear(int32_t *deadline) { if (deadline) *deadline=-1; }
int rf_timer_expired(int32_t deadline, int32_t now_ms, int *expired)
{
    if (!expired || !valid_time(now_ms) || deadline>RF_TIMER_PERIOD) return RF_RANGE;
    if (deadline<0) *expired=0;
    else if (deadline>now_ms) *expired=deadline-now_ms>RF_TIMER_PERIOD/2;
    else *expired=now_ms-deadline<=RF_TIMER_PERIOD/2;
    return RF_OK;
}
int rf_timer_remaining(int32_t deadline, int32_t now_ms, int32_t *remaining_ms)
{
    int32_t delta;
    if (!remaining_ms || !valid_time(now_ms) || deadline>RF_TIMER_PERIOD) return RF_RANGE;
    if (deadline<0) { *remaining_ms=RF_TIMER_PERIOD; return RF_OK; }
    delta=deadline-now_ms;
    if (delta>RF_TIMER_PERIOD/2) delta-=RF_TIMER_PERIOD;
    else if (delta < -RF_TIMER_PERIOD/2) delta+=RF_TIMER_PERIOD;
    *remaining_ms=delta; return RF_OK;
}
