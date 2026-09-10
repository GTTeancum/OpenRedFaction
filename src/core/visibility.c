#include "rf/visibility.h"
#include <math.h>
#include <string.h>
static int valid(const rf_visibility *s)
{
    return s && (!s->count || (s->rooms && s->order)) && s->visible_count<=s->count;
}
int rf_visibility_begin_render(rf_visibility *s)
{
    uint32_t i;if(!valid(s))return RF_RANGE;
    for(i=0;i<s->count;i++)s->rooms[i].visible=0;
    return RF_OK;
}
int rf_visibility_begin_view(rf_visibility *s)
{
    uint32_t i;if(!valid(s))return RF_RANGE;
    for(i=0;i<s->count;i++){s->rooms[i].visited=0;s->rooms[i].depth=255;}
    s->visible_count=0;return RF_OK;
}
int rf_visibility_visit(rf_visibility *s,uint32_t index,const float rectangle[4],uint32_t depth)
{
    rf_room_visibility value;uint32_t i,position;
    if(!valid(s) || index>=s->count || !rectangle)return RF_RANGE;
    for(i=0;i<4;i++)if(!isfinite(rectangle[i]))return RF_RANGE;
    value=s->rooms[index];position=s->visible_count;
    if(value.visited) {
        for(position=0;position<s->visible_count;position++)if(s->order[position]==index)break;
        if(position==s->visible_count)return RF_RANGE;
        for(i=0;i<4;i++)value.rectangle[i]=i<2?
            (rectangle[i]<value.rectangle[i]?rectangle[i]:value.rectangle[i]):
            (rectangle[i]>value.rectangle[i]?rectangle[i]:value.rectangle[i]);
    } else {
        if(s->visible_count==s->count)return RF_RANGE;
        memcpy(value.rectangle,rectangle,sizeof(value.rectangle));s->visible_count++;
    }
    for(i=position;i+1<s->visible_count;i++)s->order[i]=s->order[i+1];
    s->order[s->visible_count-1]=index;
    value.visible=1;value.visited=1;value.depth=depth;s->rooms[index]=value;
    return RF_OK;
}
