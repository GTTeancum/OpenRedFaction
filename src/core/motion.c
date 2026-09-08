#include "rf/motion.h"
#include <math.h>
#include <string.h>
int rf_motion_decode_rotation(const void *packed, size_t bytes, float out[4])
{
    const unsigned char *p = (const unsigned char *)packed;
    float result[4];
    unsigned i;
    if (!packed || !out || bytes < 8) return RF_RANGE;
    for (i = 0; i < 4; ++i) {
        int32_t value = (int32_t)p[i*2] | ((int32_t)p[i*2+1] << 8);
        if (value >= 32768) value -= 65536;
        /* RF.exe 0x589524: float bits 0x38800200, not exactly 1/16384. */
        result[i] = (float)value * 0.0000610388815402984619140625f;
    }
    memcpy(out, result, sizeof(result));
    return RF_OK;
}

int rf_motion_sample_position(const rf_motion_position_key *keys, uint32_t count, int32_t tick, float out[3])
{
    uint32_t i, c, upper;
    float result[3] = {0,0,0}, t, s;
    if (!out || (count && !keys)) return RF_RANGE;
    for (i = 0; i < count; ++i) {
        if (i && keys[i].tick <= keys[i-1].tick) return RF_FORMAT;
        for (c = 0; c < 3; ++c)
            if (!isfinite(keys[i].position[c]) || !isfinite(keys[i].incoming[c]) || !isfinite(keys[i].outgoing[c])) return RF_FORMAT;
    }
    if (!count) { memcpy(out, result, sizeof(result)); return RF_OK; }
    if (tick <= keys[0].tick) { memcpy(out, keys[0].position, sizeof(result)); return RF_OK; }
    if (tick >= keys[count-1].tick) { memcpy(out, keys[count-1].position, sizeof(result)); return RF_OK; }
    for (upper = 1; upper < count && tick >= keys[upper].tick; ++upper) {}
    /* Avoid signed overflow in validation; valid original durations use int32. */
    {
        int64_t delta = (int64_t)keys[upper].tick - keys[upper-1].tick;
        if (delta > INT32_MAX) return RF_RANGE;
        t = (float)(tick - keys[upper-1].tick) / (float)delta;
    }
    s = 1.0f - t;
    for (c = 0; c < 3; ++c) {
        float a = keys[upper-1].position[c], b = keys[upper-1].outgoing[c];
        float d = keys[upper].position[c], e = keys[upper].incoming[c];
        a *= s; a *= s; a *= s;
        b *= 3.0f; b *= t; b *= s; b *= s;
        e *= 3.0f; e *= t; e *= t; e *= s;
        d *= t; d *= t; d *= t;
        result[c] = ((a + b) + e) + d;
        if (!isfinite(result[c])) return RF_RANGE;
    }
    memcpy(out, result, sizeof(result));
    return RF_OK;
}
