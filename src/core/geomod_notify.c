#include "rf/geomod_notify.h"
#include <math.h>
int rf_geomod_notify_debris(const float position[3],int32_t bounces,float age,float lifetime,
    const float center[3],float radius,float *out_age)
{
    float d[3],limit;double distance;uint32_t i;
    if(!position || !center || !out_age)return RF_RANGE;
    if(!isfinite(age) || !isfinite(lifetime) || !isfinite(radius))return RF_FORMAT;
    for(i=0;i<3;i++) {
        if(!isfinite(position[i]) || !isfinite(center[i]))return RF_FORMAT;
        d[i]=(float)((double)center[i]-position[i]);
        if(!isfinite(d[i]))return RF_RANGE;
    }
    limit=(float)((double)radius*radius);if(!isfinite(limit))return RF_RANGE;
    distance=((double)d[0]*d[0]+(double)d[1]*d[1])+(double)d[2]*d[2];
    *out_age=bounces<=0 && distance<limit?lifetime:age;return RF_OK;
}
static int valid_box(const rf_geomod_changed_box *b)
{
    uint32_t k;for(k=0;k<3;k++)if(!isfinite(b->minimum[k])||!isfinite(b->maximum[k])||b->minimum[k]>b->maximum[k])return 0;
    return 1;
}
int rf_geomod_notify_append_fragment_box(const rf_geomod_changed_box *fragment,
    const float translation[3],rf_geomod_changed_box boxes[32],uint32_t *count)
{
    rf_geomod_changed_box value;uint32_t i;
    if(!fragment || !translation || !boxes || !count || *count>32)return RF_RANGE;
    if(!valid_box(fragment))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(translation[i]))return RF_FORMAT;
    if(*count==32)return RF_OK;
    for(i=0;i<3;i++) {
        float low=(float)((double)fragment->minimum[i]+translation[i]);
        float high=(float)((double)fragment->maximum[i]+translation[i]);
        value.minimum[i]=(float)((double)low-.5);value.maximum[i]=(float)((double)high+.5);
    }
    if(!valid_box(&value))return RF_RANGE;
    boxes[*count]=value;++*count;return RF_OK;
}
int rf_geomod_notify_changed_boxes(const rf_geomod_notify_object *object,const rf_geomod_notify_change *change,rf_geomod_notify_result *out)
{
    rf_geomod_notify_result value;uint32_t i,k;
    if(!object||!change||!out||change->count>32||(change->count&&!change->boxes)||object->kind>255||object->parent_is_kind8>1)return RF_RANGE;
    if(!isfinite(change->radius)||!valid_box(&object->bounds))return RF_FORMAT;
    for(i=0;i<change->count;i++)if(!valid_box(change->boxes+i))return RF_FORMAT;
    if(change->radius>0)return RF_NOT_FOUND;
    value=(rf_geomod_notify_result){RF_GEOMOD_NOTIFY_NONE,object->object_flags,object->physics_flags,0};
    if((object->object_flags&0x400000u)&&!(object->object_flags&0x80000u)&&!object->parent_is_kind8){
        for(i=0;i<change->count;i++){
            const rf_geomod_changed_box *box=change->boxes+i;
            for(k=0;k<3;k++)if(!(object->bounds.minimum[k]<box->maximum[k]&&object->bounds.maximum[k]>box->minimum[k]))break;
            if(k<3)continue;++value.overlaps;
        }
        if(value.overlaps){
            if(object->kind!=4&&(object->physics_flags&0x71u)){
                value.action=RF_GEOMOD_NOTIFY_WAKE;value.physics_flags|=0x80000000u;value.object_flags|=0x06000000u;
            }else if((change->retirement_enabled&255u)&&!(object->object_flags&4u)){
                value.action=RF_GEOMOD_NOTIFY_RETIRE;value.object_flags|=2u;
            }
        }
    }
    *out=value;return RF_OK;
}

int rf_geomod_notify_object_change(const rf_geomod_notify_object *object,const rf_geomod_notify_change *change,
    const rf_geomod_notify_radial *radial,rf_geomod_notify_result *out)
{
    rf_geomod_notify_change boxes;rf_geomod_notify_result value;uint32_t i;int status;
    if(!object||!change||!out)return RF_RANGE;
    if(!isfinite(change->radius))return RF_FORMAT;
    if(change->radius<=0)return rf_geomod_notify_changed_boxes(object,change,out);
    if(!radial||radial->class_present>1||radial->suppress_retirement>255||radial->network_active>255)return RF_RANGE;
    if(!isfinite(radial->body_radius)||radial->body_radius<0||!isfinite(radial->scalar_78))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(radial->center[i])||!isfinite(radial->position[i]))return RF_FORMAT;
    boxes=*change;boxes.radius=0;status=rf_geomod_notify_changed_boxes(object,&boxes,&value);if(status)return status;
    if((object->object_flags&0x400000u)&&!(object->object_flags&0x80000u)&&!object->parent_is_kind8){
        float delta[3];double distance,radius=(double)change->radius+radial->body_radius;
        for(i=0;i<3;i++){
            volatile float component=(float)((double)radial->position[i]-radial->center[i]);
            if(!isfinite(component))return RF_RANGE;delta[i]=component;
        }
        distance=((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2];
        if(distance<radius*radius){
            uint32_t special=object->kind==1||object->kind==7||(object->kind==4&&radial->scalar_78<3);
            if(!special){
                if(object->physics_flags&0x71u){value.action|=RF_GEOMOD_NOTIFY_WAKE;value.physics_flags|=0x80000000u;value.object_flags|=0x06000000u;}
            }else if(!(object->object_flags&(4u|0x4000u))&&
                (!radial->class_present||(!(radial->class_flags&1u)&&!radial->suppress_retirement&&!radial->network_active))){
                value.action|=RF_GEOMOD_NOTIFY_RETIRE;value.object_flags|=2u;
            }
        }
    }
    *out=value;return RF_OK;
}
