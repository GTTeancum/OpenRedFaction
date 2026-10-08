#include "rf/hud_scope.h"
#include <stdlib.h>
#include <string.h>

struct rf_hud_scope_assets {rf_image images[RF_HUD_SCOPE_COUNT];uint32_t resident,peak;};
void rf_hud_scope_assets_close(rf_hud_scope_assets *a)
{
    uint32_t i;if(!a)return;
    for(i=0;i<RF_HUD_SCOPE_COUNT;i++)rf_image_close(a->images+i);
    free(a);
}
int rf_hud_scope_assets_open(rf_hud_scope_assets **out,rf_vpp *maps,uint32_t map_count,uint32_t budget)
{
    static const char *const names[]={"scope_zoom_corner256_red.tga","ass2_scope_circle.tga"};
    rf_hud_scope_assets *a;uint32_t i,j;uint64_t required=sizeof(*a)+8192u+2u*128u*128u*4u;int status=RF_RANGE;
    if(!out || *out || !maps || !map_count || map_count>16 || required>budget)return RF_RANGE;
    a=calloc(1,sizeof(*a));if(!a)return RF_IO;a->resident=sizeof(*a);
    for(i=0;i<RF_HUD_SCOPE_COUNT;i++){
        rf_vpp_entry entry;unsigned char header[18];status=RF_NOT_FOUND;
        for(j=0;j<map_count;j++){status=rf_vpp_find(maps+j,names[i],&entry);if(status!=RF_NOT_FOUND)break;}
        if(status)goto fail;
        status=rf_vpp_read(maps+j,&entry,0,header,sizeof(header));if(status)goto fail;
        /* Bound this owner to the inspected original scope-quarter assets. */
        if(header[12]!=128 || header[13] || header[14]!=128 || header[15] || header[16]!=32){status=RF_FORMAT;goto fail;}
        status=rf_image_tga(a->images+i,maps+j,&entry,budget-a->resident-8192u);if(status)goto fail;
        a->resident+=a->images[i].bytes;
    }
    a->peak=a->resident+8192u;*out=a;return RF_OK;
 fail:rf_hud_scope_assets_close(a);return status;
}
uint32_t rf_hud_scope_resident_bytes(const rf_hud_scope_assets *a){return a?a->resident:0;}
uint32_t rf_hud_scope_peak_bytes(const rf_hud_scope_assets *a){return a?a->peak:0;}
static void solid(rf_hud_scope_quad *q,float x,float y,float w,float h,uint32_t argb)
{q->xywh[0]=x;q->xywh[1]=y;q->xywh[2]=w;q->xywh[3]=h;q->argb=argb;}
int rf_hud_scope_compose(const rf_hud_scope_assets *a,uint32_t kind,uint32_t width,uint32_t height,
    rf_hud_scope_quad *out,uint32_t capacity,uint32_t *count)
{
    rf_hud_scope_quad q[RF_HUD_SCOPE_QUADS]={{0}};uint32_t i;float cx,cy,left;
    if(!a || !out || !count || kind>=RF_HUD_SCOPE_COUNT || capacity<RF_HUD_SCOPE_QUADS ||
       !width || !height || width>4096 || height>4096 || width<height || ((width|height)&1u) ||
       !a->images[kind].rgba || a->images[kind].width!=128 || a->images[kind].height!=128)return RF_RANGE;
    cx=(float)(width/2);cy=(float)(height/2);left=cx-cy;
    /* 4ac447/4ac83f: original tint RGB180 on the selected channel, alpha90. */
    solid(q,0,0,(float)width,(float)height,kind==RF_HUD_SCOPE_SNIPER?0x5ab40000u:0x5a00b400u);
    for(i=0;i<4;i++){
        uint32_t right=i&1u,bottom=i>>1;rf_hud_scope_quad *r=q+1+i;
        r->image=a->images+kind;
        solid(r,right?cx:left,bottom?cy:0,cy,cy,kind==RF_HUD_SCOPE_SNIPER?0xffffffffu:0xff00ff00u);
        r->uv[0]=right?1.f:0.f;r->uv[1]=bottom?1.f:0.f;
        r->uv[2]=right?0.f:1.f;r->uv[3]=bottom?0.f:1.f;
    }
    /* 4ac562..4ac599 and4ac94e onward mask the unused horizontal margins. */
    solid(q+5,0,0,left,(float)height,0xff000000u);
    solid(q+6,cx+cy,0,left,(float)height,0xff000000u);
    if(kind==RF_HUD_SCOPE_SNIPER){
        /* 4ac5dc..4ac615: two-pixel aiming axes, black alpha64. */
        solid(q+7,cx-1,0,2,(float)height,0x40000000u);
        solid(q+8,0,cy-1,(float)width,2,0x40000000u);
    } else {
        /* 4ac98a..4aca17: source viewport-scaled one-pixel axes. The
         * decimal constants are binary32 operands in the original x87 path. */
        float y=(float)(int32_t)((double)cy-(double)height*0.35f);
        float h=(float)(int32_t)((double)height*0.7f);
        float x=(float)(int32_t)((double)cx-(double)width*0.1f);
        float w=(float)(int32_t)((double)width*0.2f);
        solid(q+7,cx,y,1,h,0x80000000u);
        solid(q+8,x,cy,w,1,0x80000000u);
    }
    memcpy(out,q,sizeof(q));*count=RF_HUD_SCOPE_QUADS;return RF_OK;
}
