#ifndef RF_FRAME_CLOCK_H
#define RF_FRAME_CLOCK_H
#include <stdint.h>
/* Port-owned 60 Hz pacing, not recovered original timing. Zero initialize.
 * A millisecond adds 60 units; a simulation tick consumes 1000 units.
 * At most eight ticks of debt survive a stall. Unsigned ms wraps naturally;
 * callers must update at intervals shorter than the 32-bit clock period.
 * All functions run on the owning simulation thread. No allocation. */
typedef struct rf_frame_clock {
    uint32_t initialized,last_ms,credit,skipped;
    uint32_t steps,presentations;
    uint64_t discarded_units;
} rf_frame_clock;
/* Return milliseconds to wait, or zero after consuming exactly one tick.
 * First call consumes the initial pose tick immediately. */
uint32_t rf_frame_clock_step(rf_frame_clock *clock,uint32_t now_ms);
/* Once per completed tick: zero skips presentation while catching up.
 * Forces presentation after eight consecutive skips, or when the current
 * simulation tick took >=17 ms. Physics still runs. */
int rf_frame_clock_present(rf_frame_clock *clock,uint32_t now_ms);
#endif
