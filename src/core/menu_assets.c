#include "rf/menu_assets.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Menu-only ownership. Original image dispatch 50ebd0/511200/511410 and
 * font layout 51fb30/atlas 51f730: keep the existing decoder's channel and
 * alpha rules, but stream into padded native storage instead of allocating
 * source-sized panoramas or a second page. No original asset bytes here. */
#define MENU_CANVAS_WIDTH 1024u
#define MENU_CANVAS_HEIGHT 512u
#define MENU_WIDTH 640u
#define MENU_HEIGHT 480u
#define MENU_ATLAS_WIDTH 512u
#define MENU_ATLAS_HEIGHT 512u
#define MENU_RECT_LIMIT (RF_MENU_FONT_COUNT*256u+RF_MENU_SPRITE_COUNT)
#define MENU_PATH_LIMIT 1024u
/* Includes the decoder's 4096-byte reader, shelf arrays, VPP stdio buffers,
 * archive handles and nested scalar call frames, not a heap allocation. */
#define MENU_SCRATCH_RESERVE 32768u
#define MENU_FONT_LIMIT 131072u

typedef struct menu_rect {uint16_t x,y,w,h;} menu_rect;
typedef struct menu_font {
    uint32_t first,count,height;
    menu_rect glyphs[256];
    int32_t advance[256];
} menu_font;
typedef struct menu_source {rf_vpp *archive;rf_vpp_entry entry;} menu_source;
typedef struct menu_plan {
    menu_source sources[RF_MENU_SPRITE_COUNT];
    menu_rect *rects[MENU_RECT_LIMIT];
    uint32_t count;
    unsigned char *font_data[RF_MENU_FONT_COUNT];
    uint32_t font_bytes[RF_MENU_FONT_COUNT];
    uint32_t font_start[RF_MENU_FONT_COUNT],font_format[RF_MENU_FONT_COUNT];
} menu_plan;
typedef struct menu_reader {
    const menu_source *source;
    uint32_t offset,cursor,count;
    unsigned char buffer[4096];
} menu_reader;
typedef struct menu_image_info {
    uint32_t width,height,vbm,version,format,mips;
} menu_image_info;
struct rf_menu_assets {
    rf_image canvas,atlas,bars[2];
    menu_rect sprites[RF_MENU_SPRITE_COUNT];
    menu_font fonts[RF_MENU_FONT_COUNT];
    char stock_path[MENU_PATH_LIMIT],overlay_path[MENU_PATH_LIMIT];
    uint32_t resident,peak,budget,page,ready;
};
static const char *const sprite_names[RF_MENU_SPRITE_COUNT]={
    "mars_background.tga","button.tga","button_more.tga","button_selected.tga",
    "lit_button_selected.tga","lit_button_small_selected.tga",
    "checkbox.tga","checkbox_checked.tga","checkbox_selected.tga",
    "slider_bar.tga","slider_bar_on.tga","indicator_selected.tga",
    "cyclebutton_l.tga","cyclebutton_r.tga","cyclebutton_l_on.tga","cyclebutton_r_on.tga",
    "popup_button_select.tga","bar_left.vbm","bar_right.vbm"
};
static const char *const font_names[RF_MENU_FONT_COUNT]={
    "rfpc-small.vf","rfpc-medium.vf","rfpc-large.vf"
};
static uint32_t u16(const unsigned char *p)
{return p[0]|(uint32_t)p[1]<<8;}
static uint32_t u32(const unsigned char *p)
{return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}

/* Both alternatives in an overlay take precedence over either stock name.
 * Only NOT_FOUND permits fallback. A present corrupt override is an error. */
static int source_find(menu_source *s,rf_vpp *stock,rf_vpp *overlay,
    const char *name,const char *alternate)
{
    rf_vpp *archives[2];uint32_t i;int status;
    archives[0]=overlay;archives[1]=stock;
    for(i=0;i<2;i++)if(archives[i] && archives[i]->stream){
        status=rf_vpp_find(archives[i],name,&s->entry);
        if(!status){s->archive=archives[i];return RF_OK;}
        if(status!=RF_NOT_FOUND)return status;
        if(alternate){
            status=rf_vpp_find(archives[i],alternate,&s->entry);
            if(!status){s->archive=archives[i];return RF_OK;}
            if(status!=RF_NOT_FOUND)return status;
        }
    }
    return RF_NOT_FOUND;
}
static int archives_open(const rf_menu_assets *a,rf_vpp *stock,rf_vpp *overlay)
{
    int status=rf_vpp_open_checked(stock,a->stock_path);
    if(status)return status;
    if(a->overlay_path[0])return rf_vpp_open_checked(overlay,a->overlay_path);
    return RF_OK;
}
static int archives_close(rf_vpp *stock,rf_vpp *overlay,int status)
{
    int right=rf_vpp_close_checked(overlay),left=rf_vpp_close_checked(stock);
    return right || left?RF_IO:status;
}
static int read_bytes(menu_reader *r,unsigned char *out,uint32_t size)
{
    while(size){
        uint32_t n;int status;
        if(r->cursor==r->count){
            if(r->offset>r->source->entry.size)return RF_FORMAT;
            r->count=r->source->entry.size-r->offset;
            if(!r->count)return RF_FORMAT;
            if(r->count>sizeof(r->buffer))r->count=sizeof(r->buffer);
            status=rf_vpp_read(r->source->archive,&r->source->entry,
                r->offset,r->buffer,r->count);
            if(status)return status;
            r->offset+=r->count;r->cursor=0;
        }
        n=r->count-r->cursor;if(n>size)n=size;
        memcpy(out,r->buffer+r->cursor,n);r->cursor+=n;out+=n;size-=n;
    }
    return RF_OK;
}
static int image_info(const menu_source *s,menu_image_info *out)
{
    unsigned char h[32];uint32_t w,height,i,frames,alpha;uint64_t payload=0;int status;
    if(s->entry.size<18)return RF_FORMAT;
    memset(out,0,sizeof(*out));memset(h,0,sizeof(h));
    status=rf_vpp_read(s->archive,&s->entry,0,h,s->entry.size<32?18:32);
    if(status)return status;
    if(!memcmp(h,".vbm",4)){
        if(s->entry.size<32)return RF_FORMAT;
        out->vbm=1;out->version=u32(h+4);out->width=u32(h+8);
        out->height=u32(h+12);out->format=u32(h+16);out->mips=u32(h+28);
        frames=u32(h+24);
        if((out->version!=1 && out->version!=2) || out->format>2 ||
           frames!=1 || !out->width || !out->height || out->mips>12)return RF_FORMAT;
        if(out->width>4096 || out->height>4096)return RF_RANGE;
        w=out->width;height=out->height;
        for(i=0;i<=out->mips;i++){
            payload+=(uint64_t)w*height*2;
            if(i<out->mips && w==1 && height==1)return RF_FORMAT;
            if(w>1)w/=2;if(height>1)height/=2;
        }
        if(payload+32!=s->entry.size)return RF_FORMAT;
    }else{
        alpha=h[17]&15;out->width=u16(h+12);out->height=u16(h+14);
        if(h[1] || (h[2]!=2 && h[2]!=10) || (h[16]!=24 && h[16]!=32) ||
           (h[17]&0xc0) || (alpha!=0 && alpha!=8) || (h[16]==24 && alpha) ||
           !out->width || !out->height)return RF_FORMAT;
        if(out->width>4096 || out->height>4096)return RF_RANGE;
        if((uint64_t)18+h[0]>s->entry.size)return RF_FORMAT;
        if(h[2]==2 && (uint64_t)18+h[0]+(uint64_t)out->width*out->height*(h[16]/8)>s->entry.size)
            return RF_FORMAT;
    }
    return RF_OK;
}
/* The target is either a transparent atlas (replace) or an opaque page
 * (source-over). Source coordinates are never stretched or content-cropped;
 * only the fixed 640x480 viewport clips the authored source canvas. */
static void write_pixel(rf_image *image,uint32_t left,uint32_t top,
    uint32_t limit_width,uint32_t limit_height,uint32_t x,uint32_t y,
    const unsigned char rgba[4],int blend)
{
    unsigned char *dst;uint32_t c,alpha=rgba[3];
    if(x>=limit_width || y>=limit_height || (blend && !alpha))return;
    dst=rf_image_pixel(image,left+x,top+y);
    /* Avoid write-combined framebuffer reads for opaque/transparent pixels. */
    if(!blend || alpha==255){memcpy(dst,rgba,4);return;}
    for(c=0;c<3;c++)dst[c]=(unsigned char)((rgba[c]*alpha+dst[c]*(255-alpha)+127)/255);
    dst[3]=255;
}
static int image_into(rf_image *image,uint32_t left,uint32_t top,
    uint32_t limit_width,uint32_t limit_height,const menu_source *source,
    uint32_t expected_width,uint32_t expected_height,int blend)
{
    menu_reader r;menu_image_info info;unsigned char h[32],p[4],rgba[4],id[255];
    uint32_t at=0,total,stride,alpha;int status;
    if(!image || !image->rgba || image->bytes!=(uint64_t)image->width*image->height*4 ||
       left>image->width || top>image->height || limit_width>image->width-left ||
       limit_height>image->height-top)return RF_RANGE;
    status=image_info(source,&info);if(status)return status;
    if(info.width!=expected_width || info.height!=expected_height)return RF_FORMAT;
    memset(&r,0,sizeof(r));r.source=source;total=info.width*info.height;
    if(info.vbm){
        r.offset=32;
        while(at<total){
            uint32_t v,format=info.format;
            status=read_bytes(&r,p,2);if(status)return status;v=u16(p);
            /* VBM disk format1 maps to original ARGB4444 format4. V1
             * disk format0 maps to1555 and flips alpha, including0xffff. */
            if(format==0 && info.version==1)v^=0x8000;
            if(format==1){
                rgba[0]=(unsigned char)(((v>>8)&15)*17);rgba[1]=(unsigned char)(((v>>4)&15)*17);
                rgba[2]=(unsigned char)((v&15)*17);rgba[3]=(unsigned char)((v>>12)*17);
            }else{
                rgba[0]=(unsigned char)(((v>>(format==2?11:10))&31)*255/31);
                rgba[1]=(unsigned char)(((v>>5)&(format==2?63:31))*255/(format==2?63:31));
                rgba[2]=(unsigned char)((v&31)*255/31);rgba[3]=(unsigned char)(format==0?(v&32768?255:0):255);
            }
            write_pixel(image,left,top,limit_width,limit_height,at%info.width,at/info.width,rgba,blend);++at;
        }
        return RF_OK;
    }
    status=read_bytes(&r,h,18);if(status)return status;
    status=read_bytes(&r,id,h[0]);if(status)return status;
    stride=h[16]/8;alpha=h[17]&15;
    while(at<total){
        uint32_t count=1,repeat=0,i;
        if(h[2]==10){
            unsigned char packet;status=read_bytes(&r,&packet,1);if(status)return status;
            count=(packet&127)+1;repeat=packet&128;
        }
        if(count>total-at)return RF_FORMAT;
        for(i=0;i<count;i++,at++){
            uint32_t x=at%info.width,y=at/info.width;
            if(!repeat || !i){status=read_bytes(&r,p,stride);if(status)return status;}
            if(h[17]&16)x=info.width-1-x;if(!(h[17]&32))y=info.height-1-y;
            rgba[0]=p[2];rgba[1]=p[1];rgba[2]=p[0];rgba[3]=alpha==8?p[3]:255;
            write_pixel(image,left,top,limit_width,limit_height,x,y,rgba,blend);
        }
    }
    return RF_OK;
}
static int append_rect(menu_plan *p,menu_rect *r)
{
    if(!r->w)return RF_OK;
    if(!r->h || r->w+2u>MENU_ATLAS_WIDTH || r->h+2u>MENU_ATLAS_HEIGHT ||
       p->count>=MENU_RECT_LIMIT)return RF_RANGE;
    p->rects[p->count++]=r;return RF_OK;
}
static int pack(menu_plan *p)
{
    uint16_t row_x[MENU_RECT_LIMIT],row_y[MENU_RECT_LIMIT];uint32_t i,j,rows=0,height=0;
    for(i=1;i<p->count;i++){
        menu_rect *r=p->rects[i];j=i;
        while(j && p->rects[j-1]->h<r->h){p->rects[j]=p->rects[j-1];--j;}p->rects[j]=r;
    }
    for(i=0;i<p->count;i++){
        menu_rect *r=p->rects[i];uint32_t w=r->w+2u,h=r->h+2u;
        for(j=0;j<rows;j++)if((uint32_t)row_x[j]+w<=MENU_ATLAS_WIDTH)break;
        if(j==rows){
            if(rows>=MENU_RECT_LIMIT || height+h>MENU_ATLAS_HEIGHT)return RF_RANGE;
            row_x[rows]=0;row_y[rows]=(uint16_t)height;height+=h;++rows;
        }
        r->x=(uint16_t)(row_x[j]+1);r->y=(uint16_t)(row_y[j]+1);row_x[j]=(uint16_t)(row_x[j]+w);
    }
    return RF_OK;
}
static void gutter(rf_image *image,const menu_rect *r)
{
    uint32_t x,y;if(!r->w || !r->h)return;
    for(x=0;x<r->w;x++){
        memcpy(rf_image_pixel(image,r->x+x,r->y-1),rf_image_pixel(image,r->x+x,r->y),4);
        memcpy(rf_image_pixel(image,r->x+x,r->y+r->h),rf_image_pixel(image,r->x+x,r->y+r->h-1),4);
    }
    for(y=0;y<r->h+2u;y++){
        memcpy(rf_image_pixel(image,r->x-1,r->y-1+y),rf_image_pixel(image,r->x,r->y-1+y),4);
        memcpy(rf_image_pixel(image,r->x+r->w,r->y-1+y),rf_image_pixel(image,r->x+r->w-1,r->y-1+y),4);
    }
}
static int font_read(rf_menu_assets *a,menu_plan *p,uint32_t slot,
    rf_vpp *stock,rf_vpp *overlay,uint32_t allowance)
{
    menu_source source;unsigned char *b;menu_font *f=a->fonts+slot;
    uint32_t version,start,pixels,format,i;int status;
    status=source_find(&source,stock,overlay,font_names[slot],NULL);if(status)return status;
    if(source.entry.size<40)return RF_FORMAT;
    if(source.entry.size>MENU_FONT_LIMIT || source.entry.size>allowance)return RF_RANGE;
    b=malloc(source.entry.size);if(!b)return RF_IO;
    p->font_data[slot]=b;p->font_bytes[slot]=source.entry.size;
    status=rf_vpp_read(source.archive,&source.entry,0,b,source.entry.size);if(status)return status;
    if(memcmp(b,"VFNT",4))return RF_FORMAT;version=u32(b+4);
    if(version==0){
        format=15;start=40;f->count=u32(b+8);f->first=u32(b+12);f->height=u32(b+20);pixels=u32(b+36);
        if(u32(b+24) || u32(b+28) || u32(b+32)!=(uint64_t)f->count*16)return RF_FORMAT;
    }else if(version==1){
        format=u32(b+8);start=36;f->count=u32(b+12);f->first=u32(b+16);f->height=u32(b+24);pixels=u32(b+32);
        if(format!=0xfffffff0u || u32(b+28))return RF_FORMAT;
    }else return RF_FORMAT;
    /* rfpc-small has230 glyph records starting32, including six records
     * beyond the byte-code range. Retain the source table without truncation. */
    if(!f->count || f->count>256 || f->first>255 || !f->height || f->height>64 ||
       (uint64_t)start+f->count*16+pixels+(version?1024:0)!=source.entry.size)return RF_FORMAT;
    for(i=0;i<f->count;i++){
        const unsigned char *g=b+start+i*16;uint32_t width=u32(g+4),offset=u32(g+8),advance=u32(g);
        if(width>256 || advance>256 || (uint64_t)offset+(uint64_t)width*f->height>pixels ||
           u16(g+12)!=65535 || u16(g+14))return RF_FORMAT;
        f->advance[i]=(int32_t)advance;f->glyphs[i].w=(uint16_t)width;f->glyphs[i].h=(uint16_t)f->height;
        status=append_rect(p,f->glyphs+i);if(status)return status;
    }
    p->font_start[slot]=start;p->font_format[slot]=format;return RF_OK;
}
static uint32_t padded(uint32_t n)
{uint32_t p=1;while(p<n)p<<=1;return p;}
static int allocate_image(rf_image *image,uint32_t width,uint32_t height)
{
    int status;image->width=width;image->height=height;image->bytes=width*height*4;image->source_format=7;
    status=rf_image_allocate_pixels(image);if(status)return status;memset(image->rgba,0,image->bytes);return RF_OK;
}
static void opaque_black(rf_image *image)
{
    uint32_t i;for(i=0;i<image->bytes;i+=4){image->rgba[i]=image->rgba[i+1]=image->rgba[i+2]=0;image->rgba[i+3]=255;}
}
static void black_slab(rf_image *image,uint32_t x,uint32_t y,uint32_t w,uint32_t h)
{
    uint32_t xx,yy;static const unsigned char rgba[4]={0,0,0,128};
    for(yy=0;yy<h && y+yy<MENU_HEIGHT;yy++)for(xx=0;xx<w && x+xx<MENU_WIDTH;xx++)
        write_pixel(image,x,y,MENU_WIDTH-x,MENU_HEIGHT-y,xx,yy,rgba,1);
}
static int composite_named(rf_menu_assets *a,rf_vpp *stock,rf_vpp *overlay,
    const char *name,const char *alternate,uint32_t x,uint32_t y)
{
    menu_source source;menu_image_info info;int status=source_find(&source,stock,overlay,name,alternate);
    if(status)return status;status=image_info(&source,&info);if(status)return status;
    return image_into(&a->canvas,x,y,MENU_WIDTH-x,MENU_HEIGHT-y,&source,info.width,info.height,1);
}
static int prepare_open_page(rf_menu_assets *a,rf_vpp *stock,rf_vpp *overlay,uint32_t page)
{
    const char *panel=NULL,*alternate=NULL;int status;
    a->ready=0;opaque_black(&a->canvas);
    status=composite_named(a,stock,overlay,"mars_background.tga",NULL,0,0);if(status)return status;
    if(page==RF_MENU_PAGE_MAIN)return composite_named(a,stock,overlay,"main_title.vbm","main_title.tga",125,58);
    if(page==RF_MENU_PAGE_EXIT){
        /* Source455c00/4561b0: two buttons move the bottom picture to y148.
         * Original455f20 draws backing, bottom, top, then text/buttons. */
        status=composite_named(a,stock,overlay,"main_title.vbm","main_title.tga",125,58);if(status)return status;
        black_slab(&a->canvas,130,158,380,163);
        status=composite_named(a,stock,overlay,"popup_buttons.vbm","popup_buttons.tga",120,148);if(status)return status;
        return composite_named(a,stock,overlay,"popup_top.vbm","popup_top.tga",120,120);
    }
    /* Original44f065/44f707: titlebar y63c070 is zero, before body44f7ca..803.
     * Source title text at265,12 never overlaps the body beginning y58. */
    status=composite_named(a,stock,overlay,"titlebar.vbm","titlebar.tga",125,0);if(status)return status;
    if(page==RF_MENU_PAGE_LOAD){
        /* Source44011f/440177 settles the599x366 panel at x33, not125. */
        black_slab(&a->canvas,43,68,599,346);
        return composite_named(a,stock,overlay,"loadgame_panel.tga",NULL,33,58);
    }
    switch(page){
    case RF_MENU_PAGE_GAME:panel="gamesettings_panel.vbm";alternate="gamesettings_panel.tga";break;
    case RF_MENU_PAGE_CONTROLS:case RF_MENU_PAGE_BUTTONS:
        black_slab(&a->canvas,230,68,512,346);panel="controls_panel.vbm";alternate="controls_panel.tga";break;
    case RF_MENU_PAGE_AUDIO:
        black_slab(&a->canvas,230,123,512,107);panel="audio_panel.vbm";alternate="audio_panel.tga";break;
    default:break;
    }
    if(panel){status=composite_named(a,stock,overlay,panel,alternate,125,58);if(status)return status;}
    return RF_OK;
}
int rf_menu_assets_prepare_page(rf_menu_assets *a,uint32_t page)
{
    rf_vpp stock={0},overlay={0};int status;
    if(!a || page>=RF_MENU_PAGE_COUNT)return RF_RANGE;
    if(a->ready && a->page==page)return RF_OK;
    a->ready=0;
    status=archives_open(a,&stock,&overlay);
    if(!status)status=prepare_open_page(a,&stock,&overlay,page);
    status=archives_close(&stock,&overlay,status);
    if(!status){a->page=page;a->ready=1;}
    return status;
}
void rf_menu_assets_close(rf_menu_assets *a)
{
    if(!a)return;rf_image_close(&a->canvas);rf_image_close(&a->atlas);
    rf_image_close(a->bars);rf_image_close(a->bars+1);free(a);
}
int rf_menu_assets_open(rf_menu_assets **out,const char *stock_path,const char *overlay_path,uint32_t budget)
{
    rf_menu_assets *a=NULL;menu_plan *p=NULL;rf_vpp stock={0},overlay={0};
    uint64_t used,resident;uint32_t i,j,k;int status=RF_RANGE;
    if(!out || *out || !stock_path || !*stock_path || strlen(stock_path)>=MENU_PATH_LIMIT ||
       (overlay_path && (!*overlay_path || strlen(overlay_path)>=MENU_PATH_LIMIT)))return RF_RANGE;
    used=sizeof(*a)+sizeof(*p)+MENU_SCRATCH_RESERVE+
        (uint64_t)MENU_CANVAS_WIDTH*MENU_CANVAS_HEIGHT*4+(uint64_t)MENU_ATLAS_WIDTH*MENU_ATLAS_HEIGHT*4;
    if(used>budget)return RF_RANGE;
    a=calloc(1,sizeof(*a));p=calloc(1,sizeof(*p));if(!a || !p){status=RF_IO;goto done;}
    a->budget=budget;strcpy(a->stock_path,stock_path);if(overlay_path)strcpy(a->overlay_path,overlay_path);
    status=archives_open(a,&stock,&overlay);if(status)goto done;
    resident=sizeof(*a)+(uint64_t)MENU_CANVAS_WIDTH*MENU_CANVAS_HEIGHT*4+
        (uint64_t)MENU_ATLAS_WIDTH*MENU_ATLAS_HEIGHT*4;
    for(i=1;i<RF_MENU_SPRITE_COUNT;i++){
        menu_rect *r=a->sprites+i;menu_image_info info;const char *alternate=NULL;
        if(i==RF_MENU_BAR_LEFT)alternate="bar_left.tga";
        if(i==RF_MENU_BAR_RIGHT)alternate="bar_right.tga";
        status=source_find(p->sources+i,&stock,&overlay,sprite_names[i],alternate);if(status)goto done;
        status=image_info(p->sources+i,&info);if(status)goto done;
        r->w=(uint16_t)info.width;r->h=(uint16_t)info.height;
        if(i>=RF_MENU_BAR_LEFT){
            uint32_t width=padded(info.width),height=padded(info.height);
            uint64_t bytes=(uint64_t)width*height*4;
            if(bytes>budget-used){status=RF_RANGE;goto done;}used+=bytes;resident+=bytes;
        }else{status=append_rect(p,r);if(status)goto done;}
    }
    for(i=0;i<RF_MENU_FONT_COUNT;i++){
        status=font_read(a,p,i,&stock,&overlay,budget-(uint32_t)used);if(status)goto done;used+=p->font_bytes[i];
    }
    status=pack(p);if(status)goto done;
    a->peak=(uint32_t)used;a->resident=(uint32_t)resident;
    status=allocate_image(&a->canvas,MENU_CANVAS_WIDTH,MENU_CANVAS_HEIGHT);if(status)goto done;
    status=allocate_image(&a->atlas,MENU_ATLAS_WIDTH,MENU_ATLAS_HEIGHT);if(status)goto done;
    for(i=1;i<RF_MENU_SPRITE_COUNT;i++){
        const menu_rect *r=a->sprites+i;rf_image *image=&a->atlas;
        if(i>=RF_MENU_BAR_LEFT){
            image=a->bars+i-RF_MENU_BAR_LEFT;
            status=allocate_image(image,padded(r->w),padded(r->h));if(status)goto done;
        }
        status=image_into(image,r->x,r->y,r->w,r->h,p->sources+i,r->w,r->h,0);if(status)goto done;
        if(i<RF_MENU_BAR_LEFT)gutter(image,r);
    }
    for(i=0;i<RF_MENU_FONT_COUNT;i++){
        const menu_font *f=a->fonts+i;const unsigned char *b=p->font_data[i];uint32_t start=p->font_start[i];
        const unsigned char *pixels=b+start+f->count*16;
        const unsigned char *palette=p->font_format[i]==15?NULL:b+p->font_bytes[i]-1024;
        for(j=0;j<f->count;j++){
            const menu_rect *r=f->glyphs+j;uint32_t offset=u32(b+start+j*16+8);
            for(k=0;k<(uint32_t)r->w*r->h;k++){
                unsigned char *dst=rf_image_pixel(&a->atlas,r->x+k%r->w,r->y+k/r->w);uint32_t v=pixels[offset+k];
                if(p->font_format[i]==15){
                    if(v>14)v=14;dst[0]=dst[1]=dst[2]=255;dst[3]=(unsigned char)(((v*255/14)>>4)*17);
                }else{
                    const unsigned char *color=palette+v*4;
                    dst[0]=(color[2]&15)*17;dst[1]=(color[1]&15)*17;dst[2]=(color[0]&15)*17;dst[3]=(color[3]&15)*17;
                }
            }
            gutter(&a->atlas,r);
        }
    }
    status=prepare_open_page(a,&stock,&overlay,RF_MENU_PAGE_MAIN);
 done:
    /* A failed checked close prevents publication and latches further opens.
     * Each owned handle is closed exactly once even after the first failure. */
    status=archives_close(&stock,&overlay,status);
    if(p){for(i=0;i<RF_MENU_FONT_COUNT;i++)free(p->font_data[i]);free(p);}
    if(!status){a->page=RF_MENU_PAGE_MAIN;a->ready=1;*out=a;a=NULL;}
    rf_menu_assets_close(a);return status;
}
static void sprite(const rf_image *image,const menu_rect *r,rf_hud_sprite *out)
{
    out->image=image;out->width=r->w;out->height=r->h;
    out->uv[0]=(float)r->x/image->width;out->uv[1]=(float)r->y/image->height;
    out->uv[2]=(float)(r->x+r->w)/image->width;out->uv[3]=(float)(r->y+r->h)/image->height;
}
int rf_menu_assets_sprite(const rf_menu_assets *a,uint32_t id,rf_hud_sprite *out)
{
    static const menu_rect background={0,0,MENU_WIDTH,MENU_HEIGHT};
    if(!a || !out || id>=RF_MENU_SPRITE_COUNT)return RF_RANGE;
    if(id==RF_MENU_BACKGROUND){if(!a->ready)return RF_NOT_FOUND;sprite(&a->canvas,&background,out);}
    else if(id>=RF_MENU_BAR_LEFT)sprite(a->bars+id-RF_MENU_BAR_LEFT,a->sprites+id,out);
    else sprite(&a->atlas,a->sprites+id,out);
    return RF_OK;
}
int rf_menu_assets_glyph(const rf_menu_assets *a,uint32_t font,uint32_t code,rf_hud_glyph *out)
{
    const menu_font *f;if(!a || !out || font>=RF_MENU_FONT_COUNT)return RF_RANGE;f=a->fonts+font;
    if(code<f->first || code-f->first>=f->count)code='?';
    if(code<f->first || code-f->first>=f->count)return RF_NOT_FOUND;code-=f->first;
    sprite(&a->atlas,f->glyphs+code,&out->sprite);out->advance=f->advance[code];return RF_OK;
}
int rf_menu_assets_text_width(const rf_menu_assets *a,uint32_t font,const char *text,uint32_t *out)
{
    uint32_t width=0,maximum=0,i;int status;rf_hud_glyph g;
    if(!a || !text || !out || font>=RF_MENU_FONT_COUNT)return RF_RANGE;
    for(i=0;text[i];i++){
        if(i>=65536)return RF_RANGE;
        if(text[i]=='\n'){if(width>maximum)maximum=width;width=0;continue;}
        status=rf_menu_assets_glyph(a,font,(unsigned char)text[i],&g);if(status)return status;
        if((uint32_t)g.advance>UINT32_MAX-width)return RF_RANGE;width+=(uint32_t)g.advance;
    }
    *out=width>maximum?width:maximum;return RF_OK;
}
uint32_t rf_menu_assets_font_height(const rf_menu_assets *a,uint32_t font)
{return a && font<RF_MENU_FONT_COUNT?a->fonts[font].height:0;}
uint32_t rf_menu_assets_resident_bytes(const rf_menu_assets *a){return a?a->resident:0;}
uint32_t rf_menu_assets_peak_bytes(const rf_menu_assets *a){return a?a->peak:0;}
