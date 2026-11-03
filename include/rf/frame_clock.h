#ifndef RF_FRAME_CLOCK_H
#define RF_FRAME_CLOCK_H
#include <stdint.h>
/* Port-owned 60 Hz pacing, not recovered original timing. Zero initialize.
 * A millisecond adds 60 units; a simulation tick consumes 1000 units.
 * At most fifteen ticks (0.25 s, the PS2 1fd9e0 frame-time cap) of debt
 * survive a stall. Unsigned ms wraps naturally;
 * callers must update at intervals shorter than the 32-bit clock period.
 * All functions run on the owning simulation thread. No allocation. */
/* Sixtieths of a second covered by the current game update: 1 at 60 Hz,
 * more when the console falls behind (rf_frame_clock_next). Game time stays counted in 60ths, so frame numbers and their
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
/* Size of the next update in 60ths after a completed wait: the minimum plus
 * every whole tick already owed, at most maximum. The PS2 frame time is the
 * real time since the previous frame, at least 1/120 s and at most 0.25 s
 * (1fd9e0: 2abae0, 2abae4); the port keeps 60ths, so it covers the owed time
 * in whole 60ths up to 15 (0.25 s) instead of slowing down. */
uint32_t rf_frame_clock_next(const rf_frame_clock *clock,uint32_t minimum,uint32_t maximum);
#endif
