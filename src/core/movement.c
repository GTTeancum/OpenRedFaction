#include "rf/movement.h"
#include <math.h>
int rf_movement_transform(const uint32_t reference[3],const float input[3],
    const float eye[9],const float body[9],const float parent[9],float output[3])
{
    float matrix[9],value[3];uint32_t axis,k;
    if(!reference || !input || !eye || !body || !parent || !output)return RF_RANGE;
    for(axis=0;axis<3;++axis) {
        const float *source=reference[axis]==1?eye:reference[axis]==2?body:reference[axis]==3?parent:NULL;
        if(!isfinite(input[axis]))return RF_RANGE;
        for(k=0;k<3;++k) {matrix[axis*3+k]=source?source[axis*3+k]:0;
            if(!isfinite(matrix[axis*3+k]))return RF_RANGE;}
    }
    for(k=0;k<3;++k) {
        value[k]=(float)(((double)matrix[k]*input[0]+(double)matrix[3+k]*input[1])+(double)matrix[6+k]*input[2]);
        if(!isfinite(value[k]))return RF_RANGE;
    }
    for(k=0;k<3;++k)output[k]=value[k];return RF_OK;
}
int rf_movement_set_mode(rf_movement_settings *state, const rf_movement_config *config,
                         int32_t requested, int32_t forced_action, float entity_scale,
                         uint8_t override_enabled)
{
    rf_movement_settings next;
    if (!state || !config) return RF_RANGE;
    if (!isfinite(config->base_speed) || !isfinite(config->slow_factor) || !isfinite(config->alternate_factor) ||
        !isfinite(config->response) || !isfinite(config->override_slow) || !isfinite(config->override_normal) ||
        !isfinite(entity_scale)) return RF_FORMAT;
    next=*state;
    if (forced_action!=-1) requested=0;
    if (config->flags & 0x800u) {
        if (!requested) next.response=3000;
        else {
            if (config->base_speed==0) return RF_RANGE;
            next.response=(float)(((double)config->response*entity_scale)/config->base_speed);
            if (!isfinite(next.response)) return RF_RANGE;
        }
    }
    if (!requested) {
        next.mode=0;
        next.speed=override_enabled ? config->override_slow : (float)((double)config->slow_factor*config->base_speed);
    } else if (requested==2) {
        next.mode=2; next.speed=(float)((double)config->alternate_factor*config->base_speed);
    } else {
        next.mode=1; next.speed=override_enabled ? config->override_normal : config->base_speed;
    }
    if (!isfinite(next.speed)) return RF_RANGE;
    *state=next; return RF_OK;
}
