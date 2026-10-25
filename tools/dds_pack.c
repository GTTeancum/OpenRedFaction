/* Build the Xbox texture pack (port-owned, not an original-game file).
 *
 * Reads every static texture in the installed map archives, in the game's
 * own search order (first archive holding a name wins), and writes each as a
 * DDS file with a full box-filtered mip chain compressed to DXT1 (opaque or
 * cutout alpha) or DXT5 (smooth alpha). The output is a VPP v1 archive whose
 * entries are named "<original name>.dds", so the Xbox loader finds them by
 * the material's own name. Each frame of an animated VBM is its own entry,
 * "<name>~<frame>.dds" (effect animations). Non-power-of-two and very small
 * images are left out and keep the ordinary decode path.
 *
 * DDS reserved1[0..2] carry "RFDX", the original engine source_format and
 * an alpha flag (1 cutout, 2 smooth), so the loader keeps alpha policy;
 * reserved1[4] holds the original entry size, which the loader matches.
 * The pack is derived from the user's installed files and must never be
 * committed. Usage: rf_dds_pack <Installed_Game dir> <output.vpp> */
#include "rf/image.h"
#include "rf/dxt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
typedef struct pack_entry {char name[61];uint32_t size;unsigned char *data;} pack_entry;
static pack_entry *entries;static uint32_t entry_count,entry_capacity;
static uint32_t counts[4]; /* dxt1, cutout, dxt5, skipped */
static double error_sum;static uint64_t error_pixels; /* level-0 RGB error */
static int same_name(const char *a,const char *b)
{
    while(*a && *b){if(tolower((unsigned char)*a)!=tolower((unsigned char)*b))return 0;a++;b++;}
    return !*a && !*b;
}
static int have(const char *name)
{
    uint32_t i;
    for(i=0;i<entry_count;i++)if(same_name(entries[i].name,name))return 1;
    return 0;
}
static void put32(unsigned char *p,uint32_t v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static uint32_t pow2(uint32_t v){return v && !(v&(v-1));}
/* Halve an RGBA8 level (alpha-weighted colour, plain alpha average). */
static void halve(const unsigned char *src,uint32_t w,uint32_t h,unsigned char *dst)
{
    uint32_t x,y,k,nw=w>1?w/2:1,nh=h>1?h/2:1;
    for(y=0;y<nh;y++)for(x=0;x<nw;x++) {
        uint32_t sx[2]={x*2,x*2+(w>1)},sy[2]={y*2,y*2+(h>1)},i,j,alpha=0;double c[3]={0};
        for(j=0;j<2;j++)for(i=0;i<2;i++) {
            const unsigned char *p=src+((size_t)sy[j]*w+sx[i])*4;
            alpha+=p[3];for(k=0;k<3;k++)c[k]+=(double)p[k]*(p[3]+1);
        }
        for(k=0;k<3;k++) {
            uint32_t weight=alpha+4;dst[((size_t)y*nw+x)*4+k]=(unsigned char)(c[k]/weight+.5);
        }
        dst[((size_t)y*nw+x)*4+3]=(unsigned char)((alpha+2)/4);
    }
}
/* Alpha class of an image: 0 opaque, 1 cutout, 2 smooth. */
static uint32_t alpha_class(const rf_image *image)
{
    uint32_t x,y,mode=0;
    for(y=0;y<image->height;y++)for(x=0;x<image->width;x++) {
        const unsigned char *p=rf_image_pixel(image,x,y);uint32_t a;
        if(rf_image_is_packed_1555(image))a=(p[1]&0x80)?255:0;else a=p[3];
        if(a!=255)mode=a==0 && mode!=2?1:2;
    }
    return mode;
}
/* minimum_mode: animation frames share one format (equal sizes per frame). */
static int compress_texture(const char *name,uint32_t source_size,const rf_image *image,uint32_t minimum_mode)
{
    uint32_t w=image->width,h=image->height,x,y,levels=0,mode=0,bytes=0,level,lw,lh,block;
    unsigned char *level_pixels,*next,*out;pack_entry *e;size_t at;
    if(!pow2(w) || !pow2(h) || w<4 || h<4 || strlen(name)+4>59){++counts[3];return 0;}
    level_pixels=malloc((size_t)w*h*4);if(!level_pixels)return -1;
    for(y=0;y<h;y++)for(x=0;x<w;x++) {
        const unsigned char *p=rf_image_pixel(image,x,y);unsigned char *d=level_pixels+((size_t)y*w+x)*4;
        if(rf_image_is_packed_1555(image)) {
            uint32_t v=p[0]|p[1]<<8;
            d[0]=(unsigned char)(((v>>10)&31)*255/31);d[1]=(unsigned char)(((v>>5)&31)*255/31);
            d[2]=(unsigned char)((v&31)*255/31);d[3]=(v&0x8000)?255:0;
        } else memcpy(d,p,4);
        if(d[3]!=255)mode=d[3]==0 && mode!=2?1:2;
    }
    if(minimum_mode>mode)mode=minimum_mode;
    block=mode==2?16:8;
    for(lw=w,lh=h;lw>=4 && lh>=4;lw/=2,lh/=2){bytes+=(lw/4)*(lh/4)*block;levels++;}
    out=calloc(1,128+bytes);if(!out){free(level_pixels);return -1;}
    memcpy(out,"DDS ",4);put32(out+4,124);put32(out+8,0x1|0x2|0x4|0x1000|0x20000|0x80000);
    put32(out+12,h);put32(out+16,w);put32(out+20,(w/4)*(h/4)*block);put32(out+28,levels);
    memcpy(out+32,"RFDX",4);put32(out+36,image->source_format);put32(out+40,mode);put32(out+48,source_size);
    put32(out+76,32);put32(out+80,0x4);memcpy(out+84,mode==2?"DXT5":"DXT1",4);
    put32(out+108,0x1000|0x8|0x400000);
    at=128;lw=w;lh=h;
    for(level=0;level<levels;level++) {
        uint32_t bx,by;
        for(by=0;by<lh/4;by++)for(bx=0;bx<lw/4;bx++) {
            unsigned char blockpx[64];uint32_t r;
            for(r=0;r<4;r++)memcpy(blockpx+r*16,level_pixels+(((size_t)by*4+r)*lw+bx*4)*4,16);
            if(mode==2)rf_dxt5_encode(blockpx,out+at);else rf_dxt1_encode(blockpx,mode==1,out+at);
            if(!level){unsigned char back[64];uint32_t q,k;
                if(mode==2)rf_dxt5_decode(out+at,back);else rf_dxt1_decode(out+at,back);
                for(q=0;q<16;q++)if(blockpx[q*4+3]>=128)for(k=0;k<3;k++){double d=(double)back[q*4+k]-blockpx[q*4+k];error_sum+=d*d;error_pixels++;}}
            at+=block;
        }
        if(level+1<levels) {
            next=malloc((size_t)(lw/2)*(lh/2)*4);if(!next){free(level_pixels);free(out);return -1;}
            halve(level_pixels,lw,lh,next);free(level_pixels);level_pixels=next;lw/=2;lh/=2;
        }
    }
    free(level_pixels);
    if(entry_count==entry_capacity) {
        uint32_t capacity=entry_capacity?entry_capacity*2:1024;pack_entry *grown=realloc(entries,capacity*sizeof(*entries));
        if(!grown){free(out);return -1;}entries=grown;entry_capacity=capacity;
    }
    e=entries+entry_count++;memset(e,0,sizeof(*e));snprintf(e->name,sizeof(e->name),"%s.dds",name);
    e->size=(uint32_t)(128+bytes);e->data=out;++counts[mode];return 0;
}
typedef struct visit_context {rf_vpp *archive;int failed;} visit_context;
static int visit(const rf_vpp_entry *entry,void *context)
{
    visit_context *c=context;const char *dot=strrchr(entry->name,'.');rf_image image={0};char packed[61];
    uint32_t frames=0,rate=0;int status;
    if(!dot || (!same_name(dot,".tga") && !same_name(dot,".vbm")))return RF_OK;
    snprintf(packed,sizeof(packed),"%s.dds",entry->name);
    if(have(packed))return RF_OK; /* An earlier archive already supplied this name. */
    snprintf(packed,sizeof(packed),"%s~0.dds",entry->name);
    if(have(packed))return RF_OK;
    if(same_name(dot,".vbm")) {
        status=rf_image_vbm_frame(&image,c->archive,entry,0,64u*1024u*1024u,&frames,&rate);
        if(status){rf_image_close(&image);++counts[3];return RF_OK;}
        if(frames>1) {
            /* Animated: every frame, or none (the loader needs a complete set). */
            uint32_t frame,first=entry_count,mode=0,m;char name[64];
            for(frame=0;frame<frames;frame++) {
                if(frame){rf_image_close(&image);
                    if(rf_image_vbm_frame(&image,c->archive,entry,frame,64u*1024u*1024u,NULL,NULL))break;}
                m=alpha_class(&image);if(m>mode)mode=m;
            }
            rf_image_close(&image);
            if(frame<frames || rf_image_vbm_frame(&image,c->archive,entry,0,64u*1024u*1024u,NULL,NULL)){++counts[3];return RF_OK;}
            for(frame=0;frame<frames && !c->failed;frame++) {
                if(frame){rf_image_close(&image);
                    if(rf_image_vbm_frame(&image,c->archive,entry,frame,64u*1024u*1024u,NULL,NULL))break;}
                snprintf(name,sizeof(name),"%s~%u",entry->name,(unsigned)frame);
                if(compress_texture(name,entry->size,&image,mode))c->failed=1;
                else if(entry_count==first+frame)break; /* skipped (size): drop the set */
            }
            if(entry_count!=first+frames){while(entry_count>first)free(entries[--entry_count].data);}
            rf_image_close(&image);
            return c->failed?RF_IO:RF_OK;
        }
    } else if(rf_image_open(&image,c->archive,entry,64u*1024u*1024u)){++counts[3];return RF_OK;}
    if(compress_texture(entry->name,entry->size,&image,0))c->failed=1;
    rf_image_close(&image);
    return c->failed?RF_IO:RF_OK;
}
int main(int argc,char **argv)
{
    static const char *archives[]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
    char path[1024];uint32_t i,offset;FILE *f;unsigned char header[2048]={0},zero[2048]={0};
    if(argc!=3){fprintf(stderr,"usage: rf_dds_pack <Installed_Game> <out.vpp>\n");return 2;}
    for(i=0;i<sizeof(archives)/sizeof(*archives);i++) {
        rf_vpp vpp={0};visit_context c={&vpp,0};
        snprintf(path,sizeof(path),"%s/%s",argv[1],archives[i]);
        if(rf_vpp_open(&vpp,path)){fprintf(stderr,"cannot open %s\n",path);return 1;}
        rf_vpp_visit(&vpp,visit,&c);rf_vpp_close(&vpp);
        if(c.failed){fprintf(stderr,"out of memory\n");return 1;}
    }
    f=fopen(argv[2],"wb");if(!f){fprintf(stderr,"cannot write %s\n",argv[2]);return 1;}
    offset=2048u+((entry_count*64u+2047u)&~2047u);
    for(i=0;i<entry_count;i++)offset+=(entries[i].size+2047u)&~2047u;
    put32(header,0x51890ace);put32(header+4,1);put32(header+8,entry_count);put32(header+12,offset);
    fwrite(header,1,2048,f);
    for(i=0;i<entry_count;i++){unsigned char row[64]={0};memcpy(row,entries[i].name,strlen(entries[i].name));put32(row+60,entries[i].size);fwrite(row,1,64,f);}
    fwrite(zero,1,((entry_count*64u+2047u)&~2047u)-entry_count*64u,f);
    for(i=0;i<entry_count;i++){uint32_t pad=((entries[i].size+2047u)&~2047u)-entries[i].size;fwrite(entries[i].data,1,entries[i].size,f);fwrite(zero,1,pad,f);}
    if(fclose(f)){fprintf(stderr,"write failed\n");return 1;}
    {double rms=sqrt(error_sum/(double)(error_pixels?error_pixels:1));
     printf("DDS_PACK %u textures: %u DXT1, %u DXT1 cutout, %u DXT5; %u skipped; %u bytes; RGB RMS %.2f (PSNR %.1f dB)\n",
        entry_count,counts[0],counts[1],counts[2],counts[3],offset,rms,20*log10(255/(rms+1e-9)));}
    return 0;
}
