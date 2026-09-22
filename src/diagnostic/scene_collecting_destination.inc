#ifndef RF_SCENE_COLLECTING_DESTINATION_INCLUDED
#define RF_SCENE_COLLECTING_DESTINATION_INCLUDED
#include "rf/level.h"
#include "rf/event.h"
#include <float.h>
#include <math.h>
#include <string.h>

typedef struct scene_collecting_destination {
    uint32_t index,uid;
    float position[3],distance;
} scene_collecting_destination;

/* Original 4057f0: scan authored event order, type27 only; full 3D
 * Euclidean distance, strictly nearer wins (ties retain first). No range or
 * event-link restriction. The live hauling/attachment state is separate. */
static int scene_collecting_destination_find(const rf_level_owned_events *events,
    const float origin[3],scene_collecting_destination *result)
{
    scene_collecting_destination next;uint32_t i,j,found=0;
    double best=DBL_MAX;
    if(!events || !origin || !result || (events->count && !events->items))return RF_RANGE;
    for(j=0;j<3;j++)if(!isfinite(origin[j]))return RF_FORMAT;
    memset(&next,0,sizeof(next));
    for(i=0;i<events->count;i++) {
        const rf_level_event *event=&events->items[i].record;
        double squared=0,distance;
        if(!memchr(event->type,0,sizeof(event->type)))return RF_FORMAT;
        if(rf_event_type_id(event->type)!=27)continue;
        for(j=0;j<3;j++) {
            double delta;
            if(!isfinite(event->position[j]))return RF_FORMAT;
            delta=(double)event->position[j]-origin[j];squared+=delta*delta;
        }
        distance=sqrt(squared);
        if(distance>=FLT_MAX)return RF_RANGE;
        if(distance>=best)continue;
        best=distance;found=1;next.index=i;next.uid=event->uid;
        next.distance=(float)distance;memcpy(next.position,event->position,sizeof(next.position));
    }
    if(!found)return RF_NOT_FOUND;
    *result=next;return RF_OK;
}
#endif
