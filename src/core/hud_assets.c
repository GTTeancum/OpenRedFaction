#include "rf/hud_assets.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
/* Original51fb30 font layout,51f730 glyph atlas,437df0 HUD name suffix.
 * Only installed VF0 coverage/VF1 palette formats and zero kerning are owned.
 * No runtime archive references or generated asset files escape this loader. */
#define HUD_WIDTH 512u
#define HUD_HEIGHT 1024u
#define HUD_RECTS 512u
#define HUD_SCRATCH 8192u
typedef struct hud_rect {uint16_t x,y,w,h;} hud_rect;
typedef struct hud_font {uint32_t first,count,height;hud_rect glyphs[256];int32_t advance[256];} hud_font;
struct rf_hud_assets {
    rf_image atlas;hud_rect sprites[RF_HUD_SPRITE_COUNT];hud_font fonts[RF_HUD_FONT_COUNT];
    int32_t positions[48][2];uint32_t colors[RF_HUD_COLOR_COUNT],resident,peak;
};
typedef struct hud_source {rf_vpp *archive;rf_vpp_entry entry;} hud_source;
typedef struct hud_plan {
    hud_source sources[RF_HUD_SPRITE_COUNT];hud_rect *rects[HUD_RECTS];uint32_t count;
    unsigned char *font_data[RF_HUD_FONT_COUNT];uint32_t font_bytes[RF_HUD_FONT_COUNT],font_start[RF_HUD_FONT_COUNT],font_format[RF_HUD_FONT_COUNT];
} hud_plan;
static uint32_t u32(const unsigned char *p)
{return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t u16(const unsigned char *p){return p[0]|(uint32_t)p[1]<<8;}
static const char *const other_names[]={
    "reticle_0.tga","scope_ret_0.tga","reticle_rocket_0.tga","reticle_rocketlock_0.tga","reticle_apc_0.tga",
    "ammo_bar_0.tga","ammo_bar_power_0.tga","noclip_ammo_bar_0.tga","ammo_signal_red_0.tga","ammo_signal_green_0.tga",
    "bullet_icon_0.tga","bullet_icon_556_0.tga","bullet_icon_shotgun_0.tga","bullet_icon_50cal_0.tga",
    "bullet_icon_powercell_0.tga","bullet_icon_rocket_0.tga","bullet_icon_gascanister_0.tga","bullet_icon_aluminum_0.tga"
};
static const char *const font_names[]={"smallfont.vf","bigfont.vf","rfpc-medium.vf"};
static int source_find(hud_source *s,const char *name,rf_vpp *ui,rf_vpp *maps,uint32_t count)
{
    uint32_t i;int status=rf_vpp_find(ui,name,&s->entry);
    if(!status){s->archive=ui;return RF_OK;}if(status!=RF_NOT_FOUND)return status;
    for(i=0;i<count;i++){status=rf_vpp_find(maps+i,name,&s->entry);if(!status){s->archive=maps+i;return RF_OK;}if(status!=RF_NOT_FOUND)return status;}
    return RF_NOT_FOUND;
}
static int append_rect(hud_plan *p,hud_rect *r)
{
    if(!r->w)return RF_OK;
    if(!r->h || r->w+2u>HUD_WIDTH || r->h+2u>HUD_HEIGHT || p->count>=HUD_RECTS)return RF_RANGE;
    p->rects[p->count++]=r;return RF_OK;
}
static int pack(hud_plan *p)
{
    uint32_t i,j,x=0,y=0,row=0;
    for(i=1;i<p->count;i++){hud_rect *r=p->rects[i];j=i;while(j && p->rects[j-1]->h<r->h){p->rects[j]=p->rects[j-1];--j;}p->rects[j]=r;}
    for(i=0;i<p->count;i++){
        hud_rect *r=p->rects[i];uint32_t w=r->w+2u,h=r->h+2u;
        if(x+w>HUD_WIDTH){y+=row;x=row=0;}
        if(y+h>HUD_HEIGHT)return RF_RANGE;
        r->x=(uint16_t)(x+1);r->y=(uint16_t)(y+1);x+=w;if(h>row)row=h;
    }
    return RF_OK;
}
static int load_layout(rf_hud_assets *a,rf_vpp *tables,uint32_t allowance,uint32_t *bytes_used)
{
    static const char *const keys[]={"$HUD default color:","$HUD msg color:","$HUD msg bg color:","$HUD full color:","$HUD mid color:","$HUD low color:","$HUD countdown color:","$HUD body msg color:"};
    rf_vpp_entry e;char *text,*p;uint32_t i,seen[48]={0};int status=rf_vpp_find(tables,"hud.tbl",&e);
    if(status)return status;if(e.size>65536 || (uint64_t)e.size+1>allowance)return RF_RANGE;
    text=malloc((size_t)e.size+1);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&e,0,text,e.size);if(status)goto done;text[e.size]=0;*bytes_used=e.size+1;
    if(memchr(text,0,e.size)){status=RF_FORMAT;goto done;}
    for(i=0;i<RF_HUD_COLOR_COUNT;i++){
        int r=0,g=0,b=0,alpha=255,n;p=strstr(text,keys[i]);if(!p || !(p=strchr(p,'{'))){status=RF_FORMAT;goto done;}
        n=sscanf(p,"{ %d , %d , %d , %d",&r,&g,&b,&alpha);
        if(n<3 || r<0 || r>255 || g<0 || g>255 || b<0 || b>255 || alpha<0 || alpha>255){status=RF_FORMAT;goto done;}
        a->colors[i]=(uint32_t)alpha<<24|(uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b;
    }
    p=strstr(text,"#640x480");if(!p){status=RF_NOT_FOUND;goto done;}p=strchr(p,'\n');
    while(p && *p){unsigned id;int x,y;char *next;++p;if(!strncmp(p,"#End",4))break;next=strchr(p,'\n');
        if(next)*next=0;
        if(sscanf(p,"%u %d %d",&id,&x,&y)==3){
            if(id>=48 || seen[id] || x<-32768 || x>32767 || y<-32768 || y>32767){status=RF_FORMAT;goto done;}
            seen[id]=1;a->positions[id][0]=x;a->positions[id][1]=y;
        }if(next)*next='\n';p=next;
    }
    for(i=0;i<48;i++)if(!seen[i]){status=RF_FORMAT;goto done;}
    status=RF_OK;
 done:free(text);return status;
}
static int font_read(rf_hud_assets *a,hud_plan *p,uint32_t slot,rf_vpp *ui,uint32_t allowance)
{
    rf_vpp_entry e;unsigned char *b;hud_font *f=a->fonts+slot;uint32_t version,start,pixels,format,i;int status;
    status=rf_vpp_find(ui,font_names[slot],&e);if(status)return status;
    if(e.size<40 || e.size>131072 || e.size>allowance)return RF_RANGE;
    b=malloc(e.size);if(!b)return RF_IO;p->font_data[slot]=b;p->font_bytes[slot]=e.size;
    status=rf_vpp_read(ui,&e,0,b,e.size);if(status)return status;
    if(memcmp(b,"VFNT",4))return RF_FORMAT;version=u32(b+4);
    if(version==0){format=15;start=40;f->count=u32(b+8);f->first=u32(b+12);f->height=u32(b+20);pixels=u32(b+36);
        if(u32(b+24) || u32(b+28) || u32(b+32)!=(uint64_t)f->count*16)return RF_FORMAT;}
    else if(version==1){format=u32(b+8);start=36;f->count=u32(b+12);f->first=u32(b+16);f->height=u32(b+24);pixels=u32(b+32);
        if(format!=0xfffffff0u || u32(b+28))return RF_FORMAT;}
    else return RF_FORMAT;
    if(!f->count || f->count>256 || f->first>255 || f->count>256-f->first || !f->height || f->height>64 ||
       (uint64_t)start+f->count*16+pixels+(version?1024:0)!=e.size)return RF_FORMAT;
    for(i=0;i<f->count;i++){
        const unsigned char *g=b+start+i*16;uint32_t width=u32(g+4),offset=u32(g+8),advance=u32(g);
        if(width>256 || advance>256 || (uint64_t)offset+(uint64_t)width*f->height>pixels || u16(g+12)!=65535 || u16(g+14))return RF_FORMAT;
        f->advance[i]=(int32_t)advance;f->glyphs[i].w=(uint16_t)width;f->glyphs[i].h=(uint16_t)f->height;
        status=append_rect(p,f->glyphs+i);if(status)return status;
    }
    p->font_start[slot]=start;p->font_format[slot]=format;return RF_OK;
}
static void gutter(rf_image *image,const hud_rect *r)
{
    uint32_t x,y;unsigned char value[4];if(!r->w || !r->h)return;
    for(x=0;x<r->w;x++){
        memcpy(value,rf_image_pixel(image,r->x+x,r->y),4);memcpy(rf_image_pixel(image,r->x+x,r->y-1),value,4);
        memcpy(value,rf_image_pixel(image,r->x+x,r->y+r->h-1),4);memcpy(rf_image_pixel(image,r->x+x,r->y+r->h),value,4);
    }
    for(y=0;y<r->h+2u;y++){
        memcpy(value,rf_image_pixel(image,r->x,r->y-1+y),4);memcpy(rf_image_pixel(image,r->x-1,r->y-1+y),value,4);
        memcpy(value,rf_image_pixel(image,r->x+r->w-1,r->y-1+y),4);memcpy(rf_image_pixel(image,r->x+r->w,r->y-1+y),value,4);
    }
}
void rf_hud_assets_close(rf_hud_assets *a){if(a){rf_image_close(&a->atlas);free(a);}}
int rf_hud_assets_open(rf_hud_assets **out,rf_vpp *ui,rf_vpp *maps,uint32_t count,rf_vpp *tables,uint32_t budget)
{
    rf_hud_assets *a=NULL;hud_plan *p=NULL;uint64_t used;uint32_t i,j,k,layout_bytes=0;int status=RF_RANGE;
    if(!out || *out || !ui || !tables || (!maps && count) || count>16)return RF_RANGE;
    used=sizeof(*a)+sizeof(*p)+HUD_SCRATCH;if(used+(uint64_t)HUD_WIDTH*HUD_HEIGHT*4>budget)return RF_RANGE;
    a=calloc(1,sizeof(*a));p=calloc(1,sizeof(*p));if(!a || !p){status=RF_IO;goto done;}
    status=load_layout(a,tables,budget-(uint32_t)used,&layout_bytes);if(status)goto done;
    a->peak=(uint32_t)used+layout_bytes;
    for(i=0;i<RF_HUD_SPRITE_COUNT;i++){
        char name[64];unsigned char h[18];hud_rect *r=a->sprites+i;
        if(i<11)snprintf(name,sizeof(name),"health%u_0.tga",i*10);
        else if(i<22)snprintf(name,sizeof(name),"enviro%u_0.tga",(i-11)*10);
        else snprintf(name,sizeof(name),"%s",other_names[i-22]);
        status=source_find(p->sources+i,name,ui,maps,count);if(status)goto done;
        status=rf_vpp_read(p->sources[i].archive,&p->sources[i].entry,0,h,sizeof(h));if(status)goto done;
        r->w=(uint16_t)u16(h+12);r->h=(uint16_t)u16(h+14);
        if(!r->w || !r->h){status=RF_FORMAT;goto done;}status=append_rect(p,r);if(status)goto done;
    }
    for(i=0;i<RF_HUD_FONT_COUNT;i++){
        status=font_read(a,p,i,ui,budget-(uint32_t)used);if(status)goto done;used+=p->font_bytes[i];
    }
    status=pack(p);if(status)goto done;
    a->atlas.width=HUD_WIDTH;a->atlas.height=HUD_HEIGHT;a->atlas.bytes=HUD_WIDTH*HUD_HEIGHT*4;a->atlas.source_format=7;
    used+=a->atlas.bytes;if(used>budget){status=RF_RANGE;goto done;}
    if(used>a->peak)a->peak=(uint32_t)used;a->resident=(uint32_t)sizeof(*a)+a->atlas.bytes;
    status=rf_image_allocate_pixels(&a->atlas);if(status)goto done;memset(a->atlas.rgba,0,a->atlas.bytes);
    for(i=0;i<RF_HUD_SPRITE_COUNT;i++){
        const hud_rect *r=a->sprites+i;
        status=rf_image_tga_into(&a->atlas,r->x,r->y,r->w,r->h,p->sources[i].archive,&p->sources[i].entry);if(status)goto done;
        gutter(&a->atlas,r);
    }
    for(i=0;i<RF_HUD_FONT_COUNT;i++){
        const hud_font *f=a->fonts+i;const unsigned char *b=p->font_data[i];uint32_t start=p->font_start[i];
        const unsigned char *pixels=b+start+f->count*16,*palette=b+p->font_bytes[i]-1024;
        for(j=0;j<f->count;j++){
            const hud_rect *r=f->glyphs+j;uint32_t offset=u32(b+start+j*16+8);
            for(k=0;k<(uint32_t)r->w*r->h;k++){
                unsigned char *dst=rf_image_pixel(&a->atlas,r->x+k%r->w,r->y+k/r->w);uint32_t v=pixels[offset+k];
                if(p->font_format[i]==15){if(v>14)v=14;dst[0]=dst[1]=dst[2]=255;dst[3]=(unsigned char)(((v*255/14)>>4)*17);}
                else {const unsigned char *color=palette+v*4;dst[0]=(color[2]&15)*17;dst[1]=(color[1]&15)*17;dst[2]=(color[0]&15)*17;dst[3]=(color[3]&15)*17;}
            }gutter(&a->atlas,r);
        }
    }
    *out=a;a=NULL;status=RF_OK;
 done:if(p){for(i=0;i<RF_HUD_FONT_COUNT;i++)free(p->font_data[i]);free(p);}rf_hud_assets_close(a);return status;
}
static void sprite(const rf_hud_assets *a,const hud_rect *r,rf_hud_sprite *out)
{
    out->image=&a->atlas;out->width=r->w;out->height=r->h;
    out->uv[0]=(float)r->x/HUD_WIDTH;out->uv[1]=(float)r->y/HUD_HEIGHT;
    out->uv[2]=(float)(r->x+r->w)/HUD_WIDTH;out->uv[3]=(float)(r->y+r->h)/HUD_HEIGHT;
}
int rf_hud_assets_sprite(const rf_hud_assets *a,uint32_t id,rf_hud_sprite *out)
{if(!a || !out || id>=RF_HUD_SPRITE_COUNT)return RF_RANGE;sprite(a,a->sprites+id,out);return RF_OK;}
int rf_hud_assets_glyph(const rf_hud_assets *a,uint32_t font,uint32_t code,rf_hud_glyph *out)
{
    const hud_font *f;if(!a || !out || font>=RF_HUD_FONT_COUNT)return RF_RANGE;f=a->fonts+font;
    if(code<f->first || code-f->first>=f->count)code='?';
    if(code<f->first || code-f->first>=f->count)return RF_NOT_FOUND;code-=f->first;
    sprite(a,f->glyphs+code,&out->sprite);out->advance=f->advance[code];return RF_OK;
}
int rf_hud_assets_text_width(const rf_hud_assets *a,uint32_t font,const char *text,uint32_t *out)
{
    uint32_t width=0,maximum=0,i;int status;rf_hud_glyph g;
    if(!a || !text || !out || font>=RF_HUD_FONT_COUNT)return RF_RANGE;
    for(i=0;text[i];i++){
        if(i>=65536)return RF_RANGE;
        if(text[i]=='\n'){if(width>maximum)maximum=width;width=0;continue;}
        status=rf_hud_assets_glyph(a,font,(unsigned char)text[i],&g);if(status)return status;
        if((uint32_t)g.advance>UINT32_MAX-width)return RF_RANGE;width+=(uint32_t)g.advance;
    }
    *out=width>maximum?width:maximum;return RF_OK;
}
uint32_t rf_hud_assets_font_height(const rf_hud_assets *a,uint32_t font)
{return a && font<RF_HUD_FONT_COUNT?a->fonts[font].height:0;}
int rf_hud_assets_position(const rf_hud_assets *a,uint32_t row,int32_t xy[2])
{if(!a || !xy || row>=48)return RF_RANGE;memcpy(xy,a->positions[row],sizeof(a->positions[row]));return RF_OK;}
uint32_t rf_hud_assets_color(const rf_hud_assets *a,uint32_t index)
{return a && index<RF_HUD_COLOR_COUNT?a->colors[index]:0xffffffffu;}
uint32_t rf_hud_assets_resident_bytes(const rf_hud_assets *a){return a?a->resident:0;}
uint32_t rf_hud_assets_peak_bytes(const rf_hud_assets *a){return a?a->peak:0;}
