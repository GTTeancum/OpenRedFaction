#include "rf/liquid_damage.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
int rf_liquid_damage_prepare(const rf_liquid_damage_input *input,rf_liquid_damage_result *output)
{
    rf_liquid_damage_result value;float rate;
    if(!input || !output)return RF_RANGE;
    memset(&value,0,sizeof(value));
    if((input->rejection_4290d0&255u)==1 || !(input->actor_flags_810&0x3000u) || !input->room_present ||
       (input->liquid_type!=3 && !(input->liquid_type==2 && input->actor_kind_1fc==3))) {
        *output=value;return RF_OK;
    }
    rate=input->liquid_type==3?input->acid_per_second:input->lava_per_second;
    if(!isfinite(rate) || rate<0 || !isfinite(input->frame_seconds) || input->frame_seconds<0)return RF_RANGE;
    value.request.amount=(float)((double)rate*input->frame_seconds);
    if(!isfinite(value.request.amount))return RF_RANGE;
    value.emit=1;value.target=input->target;value.hit_region=-1;
    value.request.source=UINT32_MAX;value.request.kind=input->liquid_type==3?7:4;
    value.request.argument6=0;value.request.auxiliary_uid=UINT32_MAX;value.request.force=0;
    *output=value;return RF_OK;
}

void rf_liquid_rooms_close(rf_liquid_rooms *rooms)
{if(rooms){free(rooms->items);memset(rooms,0,sizeof(*rooms));}}
int rf_liquid_rooms_open(const rf_geometry *g,uint32_t budget,rf_liquid_rooms *out)
{
    rf_liquid_rooms value={0};uint32_t i;int status=RF_FORMAT;
    if(!g || !out || (g->rooms && (!g->data || !g->room_offsets)))return RF_RANGE;
    if(g->rooms>UINT32_MAX/sizeof(rf_liquid_room) || g->rooms*sizeof(rf_liquid_room)>budget)return RF_RANGE;
    value.bytes=g->rooms*sizeof(rf_liquid_room);value.count=g->rooms;
    if(value.bytes){value.items=malloc(value.bytes);if(!value.items)return RF_RANGE;}
    for(i=0;i<g->rooms;i++) {
        uint32_t at=g->room_offsets[i],n,left;const unsigned char *p;rf_liquid_room *v=value.items+i;
        v->minimum_y=v->depth=NAN;v->type=0;
        if(at>g->bytes || g->bytes-at<42)goto fail;
        p=g->data+at;left=g->bytes-at;n=(uint32_t)p[40]|((uint32_t)p[41]<<8);
        if(n>left-42)goto fail;
        if(!p[32])continue;
        memcpy(&v->minimum_y,p+8,4);p+=42+n;left-=42+n;
        if(left<10)goto fail;
        memcpy(&v->depth,p,4);n=(uint32_t)p[8]|((uint32_t)p[9]<<8);
        if(n>left-10 || left-10-n<37)goto fail;
        memcpy(&v->type,p+14+n,4);
        if(!isfinite(v->minimum_y) || !isfinite(v->depth) || !isfinite((float)((double)v->minimum_y+v->depth)))goto fail;
    }
    *out=value;return RF_OK;
fail:rf_liquid_rooms_close(&value);return status;
}
