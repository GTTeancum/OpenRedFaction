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

int rf_visibility_traverse(rf_visibility *s,const rf_visibility_room_links *rooms,
    const uint32_t *links,uint32_t link_count,const rf_visibility_portal *portals,
    uint32_t portal_count,uint32_t start,uint32_t special,uint32_t flags,
    const float rectangle[4],rf_visibility_frame scratch[257])
{
    uint32_t i,j,top=0;
    if(!valid(s) || !rooms || !scratch || !rectangle || start>=s->count ||
       (link_count && !links) || (portal_count && !portals))return RF_RANGE;
    for(i=0;i<4;i++)if(!isfinite(rectangle[i]))return RF_RANGE;
    for(i=0;i<s->count;i++) {
        if(rooms[i].first>link_count || rooms[i].count>link_count-rooms[i].first)return RF_RANGE;
        for(j=0;j<rooms[i].count;j++) {
            uint32_t p=links[rooms[i].first+j];
            if(p>=portal_count || (portals[p].rooms[0]!=i && portals[p].rooms[1]!=i))return RF_RANGE;
        }
    }
    for(i=0;i<portal_count;i++) {
        if(portals[i].rooms[0]>=s->count || portals[i].rooms[1]>=s->count)return RF_RANGE;
        for(j=0;j<4;j++)if(!isfinite(portals[i].rectangle[j]))return RF_RANGE;
    }
    scratch[0].room=start;scratch[0].depth=0;scratch[0].cursor=UINT32_MAX;
    memcpy(scratch[0].rectangle,rectangle,16);
    for(;;) {
        rf_visibility_frame *f=scratch+top;const rf_visibility_room_links *r=rooms+f->room;
        if(f->cursor==UINT32_MAX) {
            int status;
            f->cursor=0;
            if(r->blocked)f->cursor=r->count;
            else {
                status=rf_visibility_visit(s,f->room,f->rectangle,f->depth);if(status)return status;
                if((flags&1u) || (r->detail && f->room!=special))f->cursor=r->count;
            }
        }
        if(f->cursor==r->count) {
            if(!top)return RF_OK;
            s->rooms[f->room].depth=255;--top;continue;
        }
        {
            const rf_visibility_portal *p=portals+links[r->first+f->cursor++];
            uint32_t next=p->rooms[p->rooms[0]==f->room?1:0];float clip[4];
            if((int32_t)s->rooms[f->room].depth>(int32_t)s->rooms[next].depth || p->rejected)continue;
            for(i=0;i<4;i++)clip[i]=i<2?
                (f->rectangle[i]>p->rectangle[i]?f->rectangle[i]:p->rectangle[i]):
                (f->rectangle[i]<p->rectangle[i]?f->rectangle[i]:p->rectangle[i]);
            if(!(clip[0]<clip[2] && clip[1]<clip[3]))continue;
            if(top==256)return RF_RANGE;
            scratch[top+1].room=next;scratch[top+1].depth=f->depth+1;scratch[top+1].cursor=UINT32_MAX;
            memcpy(scratch[++top].rectangle,clip,16);
        }
    }
}
