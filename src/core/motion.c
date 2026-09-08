#include "rf/motion.h"
#include <math.h>
#include <string.h>
static int32_t motion_wrap16(int32_t value)
{
    uint32_t bits = (uint32_t)value & 65535u;
    return bits < 32768u ? (int32_t)bits : (int32_t)bits - 65536;
}

static double motion_packed_dot(const int32_t a[4], const int32_t b[4])
{
    uint32_t sum = 0;
    int64_t signed_sum;
    unsigned i;
    for (i = 0; i < 4; ++i) sum += (uint32_t)(a[i] * b[i]);
    signed_sum = sum < 0x80000000u ? (int64_t)sum : (int64_t)sum - 4294967296LL;
    return (double)signed_sum * 3.725745045812800526619e-9;
}

int rf_motion_interpolate_rotation(const int16_t a[4], const int16_t b[4], float t, int16_t out[4])
{
    int32_t first[4], second[4], difference[4], sum[4];
    int16_t result[4];
    float dot;
    double wa, wb;
    unsigned i;
    int opposite;
    if (!a || !b || !out || !isfinite(t) || t < 0 || t > 1) return RF_RANGE;
    for (i = 0; i < 4; ++i) {
        first[i] = a[i]; second[i] = b[i];
        difference[i] = motion_wrap16(first[i] - second[i]);
        sum[i] = motion_wrap16(first[i] + second[i]);
    }
    if (motion_packed_dot(sum,sum) <= (float)motion_packed_dot(difference,difference))
        for (i = 0; i < 4; ++i) second[i] = motion_wrap16(-second[i]);
    dot = (float)motion_packed_dot(first,second);
    opposite = (double)dot + 1 <= (double)1.0e-6f;
    if (opposite) {
        wa = sin((1.0 - t) * (double)1.5707963705062866f);
        wb = sin((double)t * (double)1.5707963705062866f);
    } else if (1.0 - dot <= (double)1.0e-6f) {
        wa = 0; wb = 1;
    } else {
        double angle = acos((double)dot);
        float rounded_angle = (float)angle;
        float reciprocal = (float)(1.0 / sin(angle));
        if (!isfinite(angle) || !isfinite(reciprocal)) return RF_FORMAT;
        wa = sin((1.0 - t) * rounded_angle) * reciprocal;
        wb = sin((double)t * rounded_angle) * reciprocal;
    }
    for (i = 0; i < 4; ++i) {
        double value = first[i] * wa;
        if (opposite) value += ((i & 1) ? second[i-1] : -second[i+1]) * wb;
        else value += second[i] * wb;
        if (!isfinite(value) || value < INT32_MIN || value > INT32_MAX) return RF_RANGE;
        result[i] = (int16_t)motion_wrap16((int32_t)value);
    }
    if (!result[3]) result[3] = 1;
    memcpy(out,result,sizeof(result));
    return RF_OK;
}

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
