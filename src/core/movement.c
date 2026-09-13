#include "rf/movement.h"
#include <math.h>
#include <string.h>
int rf_movement_body_rotation(const uint32_t reference[3],const float input[3],float output[3])
{
    uint32_t values[3],axis;
    if(!reference || !input || !output)return RF_RANGE;
    memcpy(values,input,sizeof(values));
    for(axis=0;axis<3;++axis)if(reference[axis]==0 || reference[axis]==1)values[axis]=0;
    memcpy(output,values,sizeof(values));return RF_OK;
}
int rf_movement_acceleration(const uint32_t reference[3],const float input[3],float acceleration,
    const float eye[9],const float body[9],const float parent[9],float output[3])
{
    float scaled[3],value[3];double length;uint32_t k;int status;
    if(!input || !output || !isfinite(acceleration) || acceleration<0)return RF_RANGE;
    for(k=0;k<3;++k)scaled[k]=(float)((double)input[k]*acceleration);
    status=rf_movement_transform(reference,scaled,eye,body,parent,value);if(status)return status;
    length=sqrt(((double)value[0]*value[0]+(double)value[1]*value[1])+(double)value[2]*value[2]);
    if(length>acceleration) {
        float scale=(float)((double)acceleration/length);
        for(k=0;k<3;++k)value[k]=(float)((double)value[k]*scale);
    }
    for(k=0;k<3;++k)output[k]=value[k];return RF_OK;
}
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

uint32_t rf_movement_start(const rf_movement_descriptor descriptors[16],int32_t requested,uint32_t *body_flags)
{
    uint32_t slot=0;
    if(!descriptors || !body_flags)return 0;
    if(requested>=0 && requested<16 && (descriptors[requested].enabled&255u))slot=(uint32_t)requested;
    if(descriptors[slot].index==10)*body_flags&=~0x10u;
    return slot;
}

uint32_t rf_movement_fall(const rf_movement_descriptor descriptors[16],uint32_t class_flags,uint32_t *body_flags)
{
    uint32_t slot;
    if(!descriptors || !body_flags)return 0;
    *body_flags|=1u;slot=(class_flags&0x400u)?8u:3u;
    if(!(descriptors[slot].enabled&255u))slot=0;
    return slot;
}
