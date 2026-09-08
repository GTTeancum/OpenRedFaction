#include "rf/motion.h"
#include <math.h>
#include <string.h>
int rf_motion_map_loop(int32_t start, int32_t end, float phase, int32_t previous,
                       const int32_t markers[2], int wrapped, rf_motion_loop_result *out)
{
    int64_t duration=(int64_t)end-start; unsigned i; rf_motion_loop_result result;
    if (!markers || !out || !isfinite(phase) || phase<0 || phase>1) return RF_RANGE;
    if (duration<=0) return RF_FORMAT;
    if (duration>INT32_MAX) return RF_RANGE;
    result.tick=(int32_t)((int64_t)floor((double)phase*(double)duration)+start);
    result.event_mask=0;
    for (i=0;i<2;++i)
        if ((markers[i]<=result.tick && previous<markers[i]) ||
            (wrapped && (previous<markers[i] || markers[i]<result.tick))) result.event_mask|=1u<<i;
    *out=result; return RF_OK;
}

int rf_motion_advance_phase(const rf_motion_phase_slot *slots, uint32_t count, float phase,
                            int32_t delta_ticks, rf_motion_phase_result *out)
{
    float rate=0, total=0, greatest=0; uint32_t i;
    rf_motion_phase_result result={0,-1,0}; double advanced;
    if (!slots || !out || !count || count>16 || !isfinite(phase) || phase<0 || phase>1 || delta_ticks<0) return RF_RANGE;
    for (i=0;i<count;++i) {
        if (slots[i].duration<=0 || !isfinite(slots[i].weight) || slots[i].weight<0) return RF_FORMAT;
        if (!slots[i].looping) continue;
        rate=(float)(((1.0/(double)slots[i].duration)*slots[i].weight)+(double)rate);
        total+=slots[i].weight;
        if (slots[i].weight>greatest) { greatest=slots[i].weight; result.dominant_slot=(int32_t)i; }
    }
    if (!isfinite(rate) || !isfinite(total)) return RF_RANGE;
    if (total!=0) {
        advanced=((double)rate/total)*delta_ticks+phase;
        if (!isfinite(advanced) || advanced>=16777216.0) return RF_RANGE;
        result.phase=(float)advanced;
        /* Compare the unspilled value first, just like original fst/fcomp.
         * Below 2^24, repeated float subtraction of one is exactly represented. */
        if (advanced>=1.0) {
            result.wrapped=1;
            result.phase=(float)((double)result.phase-floor((double)result.phase));
        }
    }
    *out=result; return RF_OK;
}

int rf_motion_elapsed_ticks(float elapsed, int32_t *out)
{
    double ticks;
    if (!out || !isfinite(elapsed)) return RF_RANGE;
    ticks = ((double)elapsed * 30.0) * 160.0;
    if (ticks <= (double)INT32_MIN-1.0 || ticks >= (double)INT32_MAX+1.0) return RF_RANGE;
    *out = (int32_t)ticks;
    return RF_OK;
}

int rf_motion_sample_weight(const rf_motion_weight_envelope *envelope, int32_t tick, int bypass, float *out)
{
    int64_t duration, elapsed;
    double result;
    if (!envelope || !out) return RF_RANGE;
    if (!isfinite(envelope->weight) || envelope->fade_in < 0 || envelope->fade_out < 0 ||
        envelope->end_tick < envelope->start_tick) return RF_FORMAT;
    duration = (int64_t)envelope->end_tick - envelope->start_tick;
    elapsed = (int64_t)tick - envelope->start_tick;
    if (duration > INT32_MAX || elapsed < INT32_MIN || elapsed > INT32_MAX) return RF_RANGE;
    if (envelope->weight < 1.0e-5f) result = 0;
    else if (bypass) result = envelope->weight;
    else if (elapsed < 0) result = 0;
    /* Fade-in takes precedence, including when it extends beyond end_tick. */
    else if (elapsed < envelope->fade_in)
        result = ((double)elapsed / envelope->fade_in) * envelope->weight;
    else if (elapsed > duration) result = 0;
    else if (elapsed <= duration - envelope->fade_out) result = envelope->weight;
    else result = ((double)(duration - elapsed) / envelope->fade_out) * envelope->weight;
    if (result > envelope->weight && envelope->weight >= 1.0e-5f) result = envelope->weight;
    *out = (float)result;
    return RF_OK;
}

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

static float motion_ease(float t, int8_t outgoing, int8_t incoming)
{
    float a = outgoing * 0.0078740157186985015869140625f;
    float b = incoming * 0.0078740157186985015869140625f;
    float sum = a + b;
    double scale, remaining;
    if (t == 0 || t == 1 || sum == 0) return t;
    if (sum > 1) { a /= sum; b /= sum; }
    scale = 1.0 / ((2.0 - a) - b);
    if (t < a) return (float)(((scale / a) * t) * t);
    if (t < 1.0 - b) return (float)(((t + (double)t) - a) * scale);
    remaining = 1.0 - t;
    return (float)(1.0 - ((scale / b) * remaining) * remaining);
}

int rf_motion_sample_rotation(const rf_motion_rotation_key *keys, uint32_t count, int32_t tick, float out[4])
{
    uint32_t i, upper;
    int16_t packed[4];
    unsigned char bytes[8];
    float t, result[4] = {0,0,0,1};
    int status;
    if (!out || (count && !keys)) return RF_RANGE;
    for (i = 0; i < count; ++i) {
        if ((i && keys[i].tick <= keys[i-1].tick) || keys[i].incoming < 0 || keys[i].outgoing < 0)
            return RF_FORMAT;
    }
    if (!count) { memcpy(out,result,sizeof(result)); return RF_OK; }
    if (count == 1) memcpy(packed,keys[0].packed,sizeof(packed));
    else {
        if (tick <= keys[0].tick) { upper = 1; t = 0; }
        else if (tick >= keys[count-1].tick) { upper = count-1; t = 1; }
        else {
            int64_t delta;
            for (upper = 1; tick >= keys[upper].tick; ++upper) {}
            delta = (int64_t)keys[upper].tick - keys[upper-1].tick;
            if (delta > INT32_MAX) return RF_RANGE;
            t = (float)(tick - keys[upper-1].tick) / (float)delta;
        }
        t = motion_ease(t,keys[upper-1].outgoing,keys[upper].incoming);
        status = rf_motion_interpolate_rotation(keys[upper-1].packed,keys[upper].packed,t,packed);
        if (status != RF_OK) return status;
    }
    for (i = 0; i < 4; ++i) {
        bytes[i*2] = (unsigned char)((uint16_t)packed[i] & 255);
        bytes[i*2+1] = (unsigned char)((uint16_t)packed[i] >> 8);
    }
    return rf_motion_decode_rotation(bytes,sizeof(bytes),out);
}

int rf_motion_sample_position(const rf_motion_position_key *keys, uint32_t count, int32_t tick, float out[3])
{
    uint32_t i, c, upper;
    float result[3] = {0,0,0}, t;
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
    return rf_motion_interpolate_position(&keys[upper-1],&keys[upper],t,out);
}

int rf_motion_interpolate_position(const rf_motion_position_key *previous, const rf_motion_position_key *next, float t, float out[3])
{
    uint32_t c; float s, result[3];
    if (!previous || !next || !out || !isfinite(t) || t<0 || t>1) return RF_RANGE;
    for (c=0;c<3;++c)
        if (!isfinite(previous->position[c]) || !isfinite(previous->outgoing[c]) ||
            !isfinite(next->position[c]) || !isfinite(next->incoming[c])) return RF_FORMAT;
    s = 1.0f - t;
    for (c = 0; c < 3; ++c) {
        float a = previous->position[c], b = previous->outgoing[c];
        float d = next->position[c], e = next->incoming[c];
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
