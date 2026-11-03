#include "rf/ps2_menu.h"
#include "rf/vpp.h"
#include <stdlib.h>
#include <string.h>

/* PS2 front-end asset owner (port-owned; see rf/ps2_menu.h). Picture names
 * are the PS2 sheet entries loaded by the PS2 menu code (SLUS-20073): main
 * menu buttons 1e5e60, titles/backdrop 1e40c0, options pieces 1e2xxx, paused
 * 1b0xxx. Font decode follows the PC VFNT reader in menu_assets.c; PS2
 * palettes are stored in the GS CSM1 order and are unswizzled here. */
#define PS2_CANVAS_WIDTH 1024u
#define PS2_FEATURE_WIDTH 512u
#define PS2_FEATURE_HEIGHT 256u
#define PS2_FONT_LIMIT 131072u
#define PS2_STRING_LIMIT 512u

typedef struct ps2_rect {uint16_t x,y,w,h;} ps2_rect;
typedef struct ps2_font {
    uint32_t first,count,height,sharp; /* sharp: 2x glyph strip, drawn at half size */
    uint32_t start,coverage; /* glyph table offset; coverage: VFNT v0 0-14 alpha, no palette */
    ps2_rect strip;rf_vpp_entry strip_entry;
    ps2_rect glyphs[256];
    int32_t advance[256];
} ps2_font;
struct rf_ps2_menu {
    rf_image canvas,feature;
    rf_image *backdrop;uint32_t backdrop_count;
    ps2_rect pictures[RF_PS2_PICTURE_COUNT];uint32_t loaded[RF_PS2_PICTURE_COUNT];
    ps2_font fonts[RF_PS2_FONT_COUNT];
    ps2_rect feature_rect;char feature_name[64];
    char *strings[PS2_STRING_LIMIT];char *string_data;
    rf_vpp archive;uint32_t archive_open,resident;
};
static const char *const picture_names[RF_PS2_PICTURE_COUNT]={
    "mainmenu_tplate_corner_on.tga","mainmenu_tplate_corner_off.tga",
    "mainmenu_tplate_middle_on.tga","mainmenu_tplate_middle_off.tga",
    "paused.tga","popup_strips.tga","options_titlebar1.tga","options_titlebar2.tga",
    "options_blackcorner.tga","options_blacktop.tga","options_stripscorner.tga",
    "options_stripstop.tga","slider_control.tga","slider_rail.tga",
    "redfaction_title.tga","loadingstrips.tga","ls_cardempty.tga","ls_cardsel.tga",
    "multi_letterstrips.tga",
    "multi_hud_health_ammo.tga","multi_hud_time.tga","winner_trim.tga"
};
static const char *const font_names[RF_PS2_FONT_COUNT]={"standard.vf","title.vf","smallfont.vf"};
/* Double-resolution copies made by the pack tool for 720p (*_2x.tga strip
 * plus *_2x.fnt metrics); preferred when present. */
static const char *const sharp_names[RF_PS2_FONT_COUNT][2]={
    {"standard_2x.fnt","standard_2x.tga"},{"title_2x.fnt","title_2x.tga"},{"smallfont_2x.fnt","smallfont_2x.tga"}};
/* Pictures and fonts a set loads (see rf_ps2_menu_open). */
static uint32_t set_picture(uint32_t flags,uint32_t i)
{
    if(flags&RF_PS2_MENU_MP_HUD)return i>=RF_PS2_FRONTEND_PICTURES;
    return i<((flags&RF_PS2_MENU_FRONTEND)?RF_PS2_FRONTEND_PICTURES:RF_PS2_INGAME_PICTURES);
}
static uint32_t set_font(uint32_t flags,uint32_t i)
{
    if(flags&RF_PS2_MENU_MP_HUD)return i!=RF_PS2_FONT_TITLE;
    return i==RF_PS2_FONT_STANDARD || (i==RF_PS2_FONT_TITLE && (flags&RF_PS2_MENU_FRONTEND));
}

static uint32_t u16(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8;}
static uint32_t u32(const unsigned char *p){return u16(p)|u16(p+2)<<16;}
static uint32_t csm1(uint32_t i){return (i&~0x18u)|((i&8u)<<1)|((i&16u)>>1);}
static int tga_size(rf_vpp *archive,const rf_vpp_entry *e,uint32_t *w,uint32_t *h)
{
    unsigned char t[18];int status;
    if(e->size<18)return RF_FORMAT;
    status=rf_vpp_read(archive,e,0,t,18);if(status)return status;
    *w=u16(t+12);*h=u16(t+14);
    return t[2]==2 && t[16]==32 && *w && *h && *w<=PS2_CANVAS_WIDTH ? RF_OK : RF_FORMAT;
}
static int allocate_image(rf_image *image,uint32_t width,uint32_t height)
{
    int status;memset(image,0,sizeof(*image));
    image->width=width;image->height=height;image->bytes=width*height*4;image->source_format=7;
    status=rf_image_allocate_pixels(image);if(status)return status;
    memset(image->rgba,0,image->bytes);return RF_OK;
}
static void sprite(const rf_image *image,const ps2_rect *r,rf_hud_sprite *out)
{
    out->image=image;out->width=r->w;out->height=r->h;
    out->uv[0]=(float)r->x/image->width;out->uv[1]=(float)r->y/image->height;
    out->uv[2]=(float)(r->x+r->w)/image->width;out->uv[3]=(float)(r->y+r->h)/image->height;
}
/* Shelf packing in input order, one texel of gutter on every side. */
typedef struct ps2_shelf {uint32_t x,y,h;} ps2_shelf;
static int place(ps2_shelf *s,ps2_rect *r,uint32_t w,uint32_t h)
{
    if(w+2>PS2_CANVAS_WIDTH)return RF_RANGE;
    if(s->x+w+2>PS2_CANVAS_WIDTH){s->y+=s->h;s->x=0;s->h=0;}
    r->x=(uint16_t)(s->x+1);r->y=(uint16_t)(s->y+1);r->w=(uint16_t)w;r->h=(uint16_t)h;
    s->x+=w+2;if(h+2>s->h)s->h=h+2;return RF_OK;
}
static int font_read(rf_ps2_menu *m,uint32_t slot,unsigned char **data,uint32_t *bytes)
{
    rf_vpp_entry e;unsigned char *b;ps2_font *f=m->fonts+slot;uint32_t pixels,i;int status;
    status=rf_vpp_find(&m->archive,font_names[slot],&e);if(status)return status;
    if(e.size<36 || e.size>PS2_FONT_LIMIT)return RF_FORMAT;
    b=malloc(e.size);if(!b)return RF_RANGE;*data=b;*bytes=e.size;
    status=rf_vpp_read(&m->archive,&e,0,b,e.size);if(status)return status;
    if(memcmp(b,"VFNT",4))return RF_FORMAT;
    if(u32(b+4)==0){ /* smallfont.vf: version 0 header, as menu_assets.c */
        f->start=40;f->coverage=1;f->count=u32(b+8);f->first=u32(b+12);f->height=u32(b+20);pixels=u32(b+36);
        if(u32(b+24) || u32(b+28) || u32(b+32)!=(uint64_t)f->count*16)return RF_FORMAT;
    }else if(u32(b+4)==1){
        if(u32(b+8)!=0xfffffff0u || u32(b+28))return RF_FORMAT;
        f->start=36;f->coverage=0;f->count=u32(b+12);f->first=u32(b+16);f->height=u32(b+24);pixels=u32(b+32);
    }else return RF_FORMAT;
    if(!f->count || f->count>256 || f->first>255 || !f->height || f->height>64 ||
       (uint64_t)f->start+f->count*16ull+pixels+(f->coverage?0:1024ull)!=e.size)return RF_FORMAT;
    for(i=0;i<f->count;i++){
        const unsigned char *g=b+f->start+i*16;uint32_t width=u32(g+4),offset=u32(g+8);
        if(width>256 || u32(g)>256 || (uint64_t)offset+(uint64_t)width*f->height>pixels)return RF_FORMAT;
        f->advance[i]=(int32_t)u32(g);f->glyphs[i].w=(uint16_t)width;f->glyphs[i].h=(uint16_t)f->height;
    }
    return RF_OK;
}
static int font_read_sharp(rf_ps2_menu *m,uint32_t slot)
{
    rf_vpp_entry e;unsigned char head[16],g[12];ps2_font *f=m->fonts+slot;uint32_t i,w,h;int status;
    if(rf_vpp_find(&m->archive,sharp_names[slot][0],&e) ||
       rf_vpp_find(&m->archive,sharp_names[slot][1],&f->strip_entry))return RF_NOT_FOUND;
    status=rf_vpp_read(&m->archive,&e,0,head,16);if(status)return status;
    if(memcmp(head,"RFG2",4))return RF_FORMAT;
    f->count=u32(head+4);f->first=u32(head+8);f->height=u32(head+12);
    if(!f->count || f->count>256 || f->first>255 || !f->height || f->height>128 ||
       16ull+f->count*12ull!=e.size)return RF_FORMAT;
    status=tga_size(&m->archive,&f->strip_entry,&w,&h);if(status)return status;
    f->strip.w=(uint16_t)w;f->strip.h=(uint16_t)h;
    for(i=0;i<f->count;i++){
        status=rf_vpp_read(&m->archive,&e,16+i*12,g,12);if(status)return status;
        f->glyphs[i].x=(uint16_t)u16(g);f->glyphs[i].y=(uint16_t)u16(g+2);
        f->glyphs[i].w=(uint16_t)u16(g+4);f->glyphs[i].h=(uint16_t)u16(g+6);f->advance[i]=(int32_t)u32(g+8);
        if((uint32_t)f->glyphs[i].x+f->glyphs[i].w>w || (uint32_t)f->glyphs[i].y+f->glyphs[i].h>h)return RF_FORMAT;
    }
    f->sharp=1;return RF_OK;
}
static void font_draw(rf_ps2_menu *m,uint32_t slot,const unsigned char *b,uint32_t bytes)
{
    const ps2_font *f=m->fonts+slot;const unsigned char *pixels=b+f->start+f->count*16,*palette=b+bytes-1024;
    uint32_t j,k;
    for(j=0;j<f->count;j++){
        const ps2_rect *r=f->glyphs+j;uint32_t offset=u32(b+f->start+j*16+8);
        for(k=0;k<(uint32_t)r->w*r->h;k++){
            unsigned char *dst=rf_image_pixel(&m->canvas,r->x+k%r->w,r->y+k/r->w);
            if(f->coverage){uint32_t v=pixels[offset+k];if(v>14)v=14;
                dst[0]=dst[1]=dst[2]=255;dst[3]=(unsigned char)(v*255/14);continue;}
            {const unsigned char *c=palette+csm1(pixels[offset+k])*4;
            dst[0]=c[0];dst[1]=c[1];dst[2]=c[2];dst[3]=c[3];}
        }
    }
}
/* strings.tbl: "#<id>" then "En: \"text\"" lines; \n escapes kept as newlines. */
static int strings_read(rf_ps2_menu *m)
{
    rf_vpp_entry e;char *t,*p,*w;int status;
    status=rf_vpp_find(&m->archive,"strings.tbl",&e);if(status)return status;
    t=malloc(e.size+1);if(!t)return RF_RANGE;m->string_data=t;
    status=rf_vpp_read(&m->archive,&e,0,t,e.size);if(status)return status;t[e.size]=0;
    for(p=t;(p=strchr(p,'#'))!=NULL;){
        char *end;unsigned long id=strtoul(p+1,&end,10);char *en;
        if(end==p+1){p++;continue;}
        en=strstr(end,"En:");p=end;
        if(!en || id>=PS2_STRING_LIMIT)continue;
        en=strchr(en,'"');if(!en)break;en++;
        {
            char *start=en,*stop;
            for(w=en;*en && *en!='"';en++){
                if(en[0]=='\\' && en[1]=='n'){*w++='\n';en++;}
                else *w++=*en;
            }
            stop=en;p=*stop?stop+1:stop;*w=0;
            if(!m->strings[id])m->strings[id]=start;
        }
    }
    return RF_OK;
}
void rf_ps2_menu_close(rf_ps2_menu *m)
{
    uint32_t i;
    if(!m)return;
    rf_image_close(&m->canvas);rf_image_close(&m->feature);
    for(i=0;i<m->backdrop_count;i++)rf_image_close(m->backdrop+i);
    free(m->backdrop);free(m->string_data);
    if(m->archive_open)(void)rf_vpp_close_checked(&m->archive);
    free(m);
}
int rf_ps2_menu_open(rf_ps2_menu **out,const char *path,uint32_t flags,uint32_t budget)
{
    rf_ps2_menu *m;ps2_shelf shelf={0,0,0};unsigned char *font_data[RF_PS2_FONT_COUNT]={0};
    uint32_t font_bytes[RF_PS2_FONT_COUNT]={0},sizes[RF_PS2_PICTURE_COUNT][2],count,i,j,height;
    rf_vpp_entry entries[RF_PS2_PICTURE_COUNT];uint64_t used=0;int status;
    if(!out || *out || !path)return RF_RANGE;
    m=calloc(1,sizeof(*m));if(!m)return RF_RANGE;
    status=rf_vpp_open_checked(&m->archive,path);if(status){free(m);return status;}
    m->archive_open=1;
    count=RF_PS2_PICTURE_COUNT;
    for(i=0;i<count;i++){
        if(!set_picture(flags,i))continue;
        status=rf_vpp_find(&m->archive,picture_names[i],entries+i);if(status)goto done;
        status=tga_size(&m->archive,entries+i,sizes[i],sizes[i]+1);if(status)goto done;
        status=place(&shelf,m->pictures+i,sizes[i][0],sizes[i][1]);if(status)goto done;
    }
    for(i=0;i<RF_PS2_FONT_COUNT;i++){
        /* In game the title font is left out (memory); titles use standard. */
        if(!set_font(flags,i))continue;
        status=font_read_sharp(m,i);
        if(!status){
            ps2_font *f=m->fonts+i;
            status=place(&shelf,&f->strip,f->strip.w,f->strip.h);if(status)goto done;
            for(j=0;j<f->count;j++){f->glyphs[j].x=(uint16_t)(f->glyphs[j].x+f->strip.x);f->glyphs[j].y=(uint16_t)(f->glyphs[j].y+f->strip.y);}
            continue;
        }
        if(status!=RF_NOT_FOUND)goto done;
        status=font_read(m,i,font_data+i,font_bytes+i);if(status)goto done;
        for(j=0;j<m->fonts[i].count;j++){
            ps2_rect *r=m->fonts[i].glyphs+j;
            status=place(&shelf,r,r->w?r->w:1,r->h);if(status)goto done;
        }
    }
    for(height=64;height<shelf.y+shelf.h;height<<=1){}
    if(height>1024){status=RF_RANGE;goto done;}
    used=(uint64_t)PS2_CANVAS_WIDTH*height*4;
    if(flags&RF_PS2_MENU_FRONTEND)used+=PS2_FEATURE_WIDTH*PS2_FEATURE_HEIGHT*4;
    if(used>budget){status=RF_RANGE;goto done;}
    status=allocate_image(&m->canvas,PS2_CANVAS_WIDTH,height);if(status)goto done;
    for(i=0;i<count;i++){
        const ps2_rect *r=m->pictures+i;
        if(!set_picture(flags,i))continue;
        status=rf_image_tga_into(&m->canvas,r->x,r->y,r->w,r->h,&m->archive,entries+i);if(status)goto done;
        m->loaded[i]=1;
    }
    for(i=0;i<RF_PS2_FONT_COUNT;i++){
        const ps2_font *f=m->fonts+i;
        if(f->sharp){
            status=rf_image_tga_into(&m->canvas,f->strip.x,f->strip.y,f->strip.w,f->strip.h,&m->archive,&f->strip_entry);
            if(status)goto done;
        }else if(font_data[i])font_draw(m,i,font_data[i],font_bytes[i]);
    }
    if(flags&RF_PS2_MENU_FRONTEND){
        status=allocate_image(&m->feature,PS2_FEATURE_WIDTH,PS2_FEATURE_HEIGHT);if(status)goto done;
    }
    status=strings_read(m);if(status)goto done;
    if(flags&RF_PS2_MENU_BACKDROP){
        rf_vpp_entry e;char name[32];
        m->backdrop=calloc(150,sizeof(rf_image));if(!m->backdrop){status=RF_RANGE;goto done;}
        for(i=0;i<150;i++){
            name[0]=0;strcpy(name,"plan-0000.dds");
            name[5]=(char)('0'+(i+1)/1000%10);name[6]=(char)('0'+(i+1)/100%10);
            name[7]=(char)('0'+(i+1)/10%10);name[8]=(char)('0'+(i+1)%10);
            if(rf_vpp_find(&m->archive,name,&e))break;
            if(used+65536>budget)break; /* keep the frames that fit */
            status=rf_image_dds(m->backdrop+i,&m->archive,&e,0,0,(uint32_t)(budget-used));if(status)goto done;
            used+=m->backdrop[i].bytes;++m->backdrop_count;
        }
    }
    m->resident=(uint32_t)used;
 done:
    for(i=0;i<RF_PS2_FONT_COUNT;i++)free(font_data[i]);
    if(status){rf_ps2_menu_close(m);return status;}
    *out=m;return RF_OK;
}
uint32_t rf_ps2_menu_resident_bytes(const rf_ps2_menu *m){return m?m->resident:0;}
int rf_ps2_menu_picture(const rf_ps2_menu *m,uint32_t id,rf_hud_sprite *out)
{
    if(!m || !out || id>=RF_PS2_PICTURE_COUNT)return RF_RANGE;
    if(!m->loaded[id])return RF_NOT_FOUND;
    sprite(&m->canvas,m->pictures+id,out);return RF_OK;
}
int rf_ps2_menu_feature(rf_ps2_menu *m,const char *name,rf_hud_sprite *out)
{
    rf_vpp_entry e;uint32_t w,h;int status;
    if(!m || !name || !out || !m->feature.rgba || strlen(name)>=sizeof(m->feature_name))return RF_RANGE;
    if(strcmp(m->feature_name,name)){
        m->feature_name[0]=0;
        status=rf_vpp_find(&m->archive,name,&e);if(status)return status;
        status=tga_size(&m->archive,&e,&w,&h);if(status)return status;
        if(w>PS2_FEATURE_WIDTH || h>PS2_FEATURE_HEIGHT)return RF_RANGE;
        status=rf_image_tga_into(&m->feature,0,0,w,h,&m->archive,&e);if(status)return status;
        m->feature_rect.x=0;m->feature_rect.y=0;m->feature_rect.w=(uint16_t)w;m->feature_rect.h=(uint16_t)h;
        strcpy(m->feature_name,name);
    }
    sprite(&m->feature,&m->feature_rect,out);return RF_OK;
}
int rf_ps2_menu_glyph(const rf_ps2_menu *m,uint32_t font,uint32_t code,rf_hud_glyph *out)
{
    const ps2_font *f;
    if(!m || !out || font>=RF_PS2_FONT_COUNT)return RF_RANGE;
    f=m->fonts+font;if(!f->count)f=m->fonts+RF_PS2_FONT_STANDARD;
    if(!f->count)return RF_NOT_FOUND;
    if(code<f->first || code-f->first>=f->count)code='?';
    if(code<f->first || code-f->first>=f->count)return RF_NOT_FOUND;
    code-=f->first;sprite(&m->canvas,f->glyphs+code,&out->sprite);out->advance=f->advance[code];
    if(f->sharp){out->sprite.width/=2;out->sprite.height/=2;}
    return RF_OK;
}
int rf_ps2_menu_text_width(const rf_ps2_menu *m,uint32_t font,const char *text,uint32_t *out)
{
    uint32_t width=0,maximum=0,i;rf_hud_glyph g;int status;
    if(!m || !text || !out)return RF_RANGE;
    for(i=0;text[i];i++){
        if(text[i]=='\n'){if(width>maximum)maximum=width;width=0;continue;}
        status=rf_ps2_menu_glyph(m,font,(unsigned char)text[i],&g);if(status)return status;
        width+=(uint32_t)g.advance;
    }
    *out=width>maximum?width:maximum;return RF_OK;
}
uint32_t rf_ps2_menu_font_height(const rf_ps2_menu *m,uint32_t font)
{
    const ps2_font *f;
    if(!m || font>=RF_PS2_FONT_COUNT)return 0;
    f=m->fonts[font].count?m->fonts+font:m->fonts+RF_PS2_FONT_STANDARD;
    return f->height>>f->sharp;
}
const char *rf_ps2_menu_string(const rf_ps2_menu *m,uint32_t id)
{return m && id<PS2_STRING_LIMIT && m->strings[id]?m->strings[id]:"";}
uint32_t rf_ps2_menu_backdrop_count(const rf_ps2_menu *m){return m?m->backdrop_count:0;}
const rf_image *rf_ps2_menu_backdrop(const rf_ps2_menu *m,uint32_t frame)
{return m && m->backdrop_count?m->backdrop+frame%m->backdrop_count:NULL;}
