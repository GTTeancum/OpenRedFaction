#ifndef RF_FRAME_CLOCK_H
#define RF_FRAME_CLOCK_H
#include <stdint.h>
/* Port-owned 60 Hz pacing, not recovered original timing. Zero initialize.
 * A millisecond adds 60 units; a simulation tick consumes 1000 units.
 * At most eight ticks of debt survive a stall. Unsigned ms wraps naturally;
 * callers must update at intervals shorter than the 32-bit clock period.
 * All functions run on the owning simulation thread. No allocation. */
/* Sixtieths of a second covered by the current game update: 1 (60 Hz) or
 * 2 (30 Hz). Game time stays counted in 60ths, so frame numbers and their
 * millisecond conversions keep their meaning; only the step size changes.
 * rf_tick_frames_next is the request applied at the next update boundary. */
extern uint32_t rf_tick_frames,rf_tick_frames_next;
/* Count a 60ths countdown down by one game update (rf_tick_frames 60ths),
 * stopping at zero. Countdowns stay in 60ths so 30 Hz keeps their duration. */
#define RF_TICK_DOWN(x) ((x)=(x)>rf_tick_frames?(x)-rf_tick_frames:0)
typedef struct rf_frame_clock {
    uint32_t initialized,last_ms,credit,skipped;
    uint32_t steps,presentations;
    uint64_t discarded_units;
    uint32_t last_present_ms;
} rf_frame_clock;
/* Return milliseconds to wait, or zero after consuming exactly one tick
 * (rf_tick_frames sixtieths).
 * First call consumes the initial pose tick immediately. */
uint32_t rf_frame_clock_step(rf_frame_clock *clock,uint32_t now_ms);
/* Once per completed tick: zero skips presentation while catching up.
 * Skips while a whole tick is already owed (catch-up), forcing presentation
 * after three consecutive skips or >=100 ms since the last draw. Physics
 * still runs for every tick. */
int rf_frame_clock_present(rf_frame_clock *clock,uint32_t now_ms);
#endif
