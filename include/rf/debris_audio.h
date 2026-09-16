#ifndef RF_DEBRIS_AUDIO_H
#define RF_DEBRIS_AUDIO_H
#include "rf/random.h"
/*48f900 aggregate,48f4e0 flush. Level owns state; initialize deadline=-1.
 * Original global CRT becomes an explicitly owned caller stream. */
typedef struct rf_debris_audio_state {
    float position_sum[3],speed_sum;int32_t count,deadline;
} rf_debris_audio_state;
typedef struct rf_debris_audio_request {
    float position[3],gain;int32_t sample,delay_ms;uint32_t ready;
} rf_debris_audio_request;
void rf_debris_audio_init(rf_debris_audio_state *state);
/* Supply the chunk position/velocity BEFORE contact placement/bounce/zeroing.
 * wet_contact is the stored room's inclusive-plane test at the contact point.
 * Includes terminal floor bounce; no material/age gate. Errors roll back. */
int rf_debris_audio_contact(rf_debris_audio_state *state,const float position[3],
    const float velocity[3],float normal_y,uint32_t wet_contact,uint32_t *contributed);
/* Once AFTER chunk list, not per contact. count0 models unresolved Foley group:
 * ready request has sample=-1 but still consumes cooldown draw/resets sums.
 * Valid pool chooses one draw iff count>1; next draw chooses cooldown even0.
 * Caller performs positional playback, applying authored sample gain, and MUST
 * NOT retry the request on device failure. No device callbacks or PCM ownership.
 * State/RNG/output unchanged on error. Nonaliasing arguments required. */
int rf_debris_audio_dispatch(rf_debris_audio_state *state,int32_t now_ms,
    const int32_t *samples,uint32_t sample_count,rf_random_state *random,
    rf_debris_audio_request *request);
#endif
