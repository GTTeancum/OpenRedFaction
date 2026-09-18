#include "rf/weapon_scanner.h"
#include <math.h>
#include <string.h>
int rf_weapon_scanner_select(const rf_object_registry *registry,const rf_weapon_scanner_candidate *items,
    uint32_t count,const rf_model_projection *view,float range,uint32_t width,uint32_t height,
    rf_weapon_scanner_result *out)
{
    rf_weapon_scanner_result result={0};rf_model_projection projection;uint32_t i,k;
    if(!registry || !view || !out || (count && !items) || !width || !height ||
       !isfinite(range) || range<=0 || !view->perspective)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(view->camera[i]))return RF_RANGE;
    for(i=0;i<9;i++)if(!isfinite(view->rotation[i]))return RF_RANGE;
    for(i=0;i<4;i++)if(!isfinite(view->screen[i]))return RF_RANGE;
    if(!view->screen[0] || !view->screen[1])return RF_RANGE;
    projection=*view;projection.compute_clip=0;projection.screen_clip=1;projection.depth_factor=0;
    projection.bounds[0]=projection.bounds[1]=0;projection.bounds[2]=(float)width;projection.bounds[3]=(float)height;
    for(i=0;i<count;i++){
        const rf_weapon_scanner_candidate *c=items+i;double distance2=0,z=0;float delta[3];
        rf_model_render_cache cache={0};float clip[3];uint8_t vertex[40]={0};uint32_t visible,pos,j;int status;
        rf_weapon_scanner_marker marker;
        if(!c->eligible || !c->identity || rf_object_registry_lookup(registry,c->handle)!=c->identity)continue;
        if(!isfinite(c->health))return RF_RANGE;if(c->health<=0)continue;
        for(k=0;k<3;k++){
            if(!isfinite(c->center[k]))return RF_RANGE;
            delta[k]=c->center[k]-view->camera[k];if(!isfinite(delta[k]))return RF_RANGE;
            distance2+=(double)delta[k]*delta[k];z+=(double)delta[k]*view->rotation[6+k];
        }
        if(z<=.001 || distance2>(double)range*range)continue;
        if(!isfinite(z) || z>3.402823466e38)return RF_RANGE;
        status=rf_model_project_vertex(c->center,&projection,&cache,clip,vertex,&visible);if(status)return status;
        if(!isfinite(cache.projected[0]) || !isfinite(cache.projected[1]))return RF_RANGE;
        if(!visible)continue;
        marker=(rf_weapon_scanner_marker){c->handle,i,{cache.projected[0],cache.projected[1]},(float)z,(float)sqrt(distance2)};
        for(j=0;j<result.count;j++)if(result.markers[j].handle==marker.handle)break;
        if(j<result.count){if(result.markers[j].distance<=marker.distance)continue;
            for(;j+1<result.count;j++)result.markers[j]=result.markers[j+1];--result.count;}
        for(pos=0;pos<result.count && result.markers[pos].distance<=marker.distance;pos++){}
        if(result.count==RF_WEAPON_SCANNER_CAPACITY){result.truncated=1;if(pos==result.count)continue;}
        else ++result.count;
        for(j=result.count-1;j>pos;j--)result.markers[j]=result.markers[j-1];result.markers[pos]=marker;
    }
    *out=result;return RF_OK;
}
