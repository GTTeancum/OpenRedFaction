#include "rf/image.h"
#include <stdlib.h>
#include <string.h>
#ifdef RF_IMAGE_XBOX_NATIVE
#include <xboxkrnl/xboxkrnl.h>
#endif
int rf_image_allocate_pixels(rf_image *image)
{
    if(!image || image->rgba || !image->width || !image->height || image->width>4096 || image->height>4096 ||
       (uint64_t)image->width*image->height*4!=image->bytes)return RF_RANGE;
#ifdef RF_IMAGE_XBOX_NATIVE
    if((image->width&(image->width-1)) || (image->height&(image->height-1)))return RF_FORMAT;
    image->rgba=MmAllocateContiguousMemoryEx(image->bytes,0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
#else
    image->rgba=malloc(image->bytes);
#endif
    return image->rgba?RF_OK:RF_RANGE;
}
unsigned char *rf_image_pixel(const rf_image *image,uint32_t x,uint32_t y)
{
#ifdef RF_IMAGE_XBOX_NATIVE
    uint32_t bit,index=0,destination=1;
    for(bit=1;bit<image->width || bit<image->height;bit<<=1) {
        if(bit<image->width){if(x&bit)index|=destination;destination<<=1;}
        if(bit<image->height){if(y&bit)index|=destination;destination<<=1;}
    }
#else
    uint32_t index=y*image->width+x;
#endif
    return image->rgba+index*4;
}
uint32_t rf_image_tga_format(uint32_t bits)
{
    switch(bits) { case 8:return 1;case 16:return 5;case 24:return 6;case 32:return 7;default:return 0; }
}
int rf_image_format_has_alpha(uint32_t format)
{
    return format==4 || format==5 || format==7;
}
typedef struct reader {
    rf_vpp *archive;
    const rf_vpp_entry *entry;
    uint32_t offset, cursor, count;
    unsigned char buffer[4096];
} reader;
static int read_bytes(reader *r, unsigned char *out, uint32_t size)
{
    while (size) {
        uint32_t n;
        if (r->cursor == r->count) {
            int result;
            r->count = r->entry->size - r->offset;
            if (!r->count) return RF_FORMAT;
            if (r->count > sizeof(r->buffer)) r->count = sizeof(r->buffer);
            result = rf_vpp_read(r->archive, r->entry, r->offset, r->buffer, r->count);
            if (result) return result;
            r->offset += r->count; r->cursor = 0;
        }
        n = r->count - r->cursor;
        if (n > size) n = size;
        memcpy(out, r->buffer + r->cursor, n);
        r->cursor += n; out += n; size -= n;
    }
    return RF_OK;
}
void rf_image_close(rf_image *image)
{
    if (!image)return;
#ifdef RF_IMAGE_XBOX_NATIVE
    if(image->rgba)MmFreeContiguousMemory(image->rgba);
#else
    free(image->rgba);
#endif
    memset(image,0,sizeof(*image));
}
int rf_image_tga(rf_image *image, rf_vpp *archive, const rf_vpp_entry *entry, uint32_t budget)
{
    unsigned char h[18], pixel[4], id[255];
    uint32_t width, height, total, at = 0, stride, alpha;
    reader r;
    int result;
    if (!image) return RF_RANGE;
    memset(image, 0, sizeof(*image));
    if (!archive || !entry) return RF_RANGE;
    memset(&r, 0, sizeof(r)); r.archive = archive; r.entry = entry;
    result = read_bytes(&r, h, sizeof(h));
    if (result) return result;
    width = h[12] | (uint32_t)h[13] << 8;
    height = h[14] | (uint32_t)h[15] << 8;
    alpha = h[17] & 15;
    if (h[1] || (h[2] != 2 && h[2] != 10) || (h[16] != 24 && h[16] != 32) ||
        (h[17] & 0xc0) || (alpha != 0 && alpha != 8) || (h[16] == 24 && alpha) || !width || !height)
        return RF_FORMAT;
    /* Hardware texture dimensions and arithmetic stay bounded on both targets. */
    if (width > 4096 || height > 4096 || (uint64_t)width * height * 4 > budget) return RF_RANGE;
    total = width * height; stride = h[16] / 8;
    result = read_bytes(&r, id, h[0]);
    if (result) return result;
    image->width = width; image->height = height; image->bytes = total * 4;
    result=rf_image_allocate_pixels(image);if(result){rf_image_close(image);return result;}
    image->source_format=rf_image_tga_format(h[16]);
    while (at < total) {
        uint32_t count = 1, repeat = 0, i;
        if (h[2] == 10) {
            unsigned char packet;
            result = read_bytes(&r, &packet, 1);
            if (result) goto fail;
            count = (packet & 127) + 1; repeat = packet & 128;
        }
        if (count > total - at) { result = RF_FORMAT; goto fail; }
        for (i = 0; i < count; ++i, ++at) {
            uint32_t x = at % width, y = at / width; unsigned char *dst;
            if (!repeat || !i) {
                result = read_bytes(&r, pixel, stride);
                if (result) goto fail;
            }
            if (h[17] & 16) x = width - 1 - x;
            if (!(h[17] & 32)) y = height - 1 - y;
            dst=rf_image_pixel(image,x,y);
            dst[0]=pixel[2];dst[1]=pixel[1];dst[2]=pixel[0];
            dst[3]=alpha==8?pixel[3]:255;
        }
    }
    return RF_OK;
fail:
    rf_image_close(image);
    return result;
}

static uint32_t image_u32(const unsigned char *p)
{
    return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static int vbm_decode(rf_image *image,rf_vpp *archive,const rf_vpp_entry *entry,uint32_t budget,
    uint32_t frame,int static_only,uint32_t *frame_count,uint32_t *frame_rate)
{
    reader r; unsigned char h[32], pixel[2];
    uint32_t width,height,format,mips,w,hg,i,total,frames; uint64_t payload=0;
    int status;
    if(!image)return RF_RANGE;
    memset(image,0,sizeof(*image));
    if(!archive || !entry)return RF_RANGE;
    memset(&r,0,sizeof(r));r.archive=archive;r.entry=entry;
    status=read_bytes(&r,h,sizeof(h));if(status)return status;
    width=image_u32(h+8);height=image_u32(h+12);format=image_u32(h+16);mips=image_u32(h+28);
    frames=image_u32(h+24);
    /* Original 50ebd0 header mapping; 511200 uses consecutive frame data. */
    if(memcmp(h,".vbm",4) || (image_u32(h+4)!=1 && image_u32(h+4)!=2) || format>2 ||
       !frames || (static_only && frames!=1) || (frames>1 && mips) || !width || !height || mips>12)return RF_FORMAT;
    if(frame>=frames)return RF_RANGE;
    if(width>4096 || height>4096 || (uint64_t)width*height*4>budget)return RF_RANGE;
    w=width;hg=height;
    for(i=0;i<=mips;++i) {
        payload+=(uint64_t)w*hg*2;
        if(i<mips && w==1 && hg==1)return RF_FORMAT;
        if(w>1)w/=2;if(hg>1)hg/=2;
    }
    if(payload*frames+32!=entry->size)return RF_FORMAT;
    r.offset=32+(uint32_t)(payload*frame);r.cursor=r.count=0;
    total=width*height;
    image->width=width;image->height=height;image->bytes=total*4;
    status=rf_image_allocate_pixels(image);if(status){rf_image_close(image);return status;}
    image->source_format=format==0?5:format==1?4:3;
    /* 55dd20 encodes BGR(A) into 565, 4444 and 1555. Expand normalized
       channels to RGBA8; retain only the base mip in the current renderer. */
    for(i=0;i<total;++i) {
        uint32_t v,red,green,blue,alpha=255;unsigned char *dst;
        status=read_bytes(&r,pixel,2);if(status){rf_image_close(image);return status;}
        v=pixel[0]|(uint32_t)pixel[1]<<8;
        /* 511200 calls 511410 for static format 5 with version below 2. */
        if(format==0 && (frames>1 || image_u32(h+4)==1))v^=0x8000;
        if(format==1) {
            blue=(v&15)*17;green=((v>>4)&15)*17;red=((v>>8)&15)*17;alpha=(v>>12)*17;
        } else {
            blue=(v&31)*255/31;
            green=((v>>5)&(format==2?63:31))*255/(format==2?63:31);
            red=((v>>(format==2?11:10))&31)*255/31;
            if(format==0)alpha=(v&32768)?255:0;
        }
        dst=rf_image_pixel(image,i%width,i/width);
        dst[0]=(unsigned char)red;dst[1]=(unsigned char)green;
        dst[2]=(unsigned char)blue;dst[3]=(unsigned char)alpha;
    }
    if(frame_count)*frame_count=frames;
    if(frame_rate)*frame_rate=image_u32(h+20);
    return RF_OK;
}
int rf_image_vbm(rf_image *image,rf_vpp *archive,const rf_vpp_entry *entry,uint32_t budget)
{return vbm_decode(image,archive,entry,budget,0,1,NULL,NULL);}
int rf_image_vbm_frame(rf_image *image,rf_vpp *archive,const rf_vpp_entry *entry,
    uint32_t frame,uint32_t budget,uint32_t *frame_count,uint32_t *frame_rate)
{return vbm_decode(image,archive,entry,budget,frame,0,frame_count,frame_rate);}
int rf_image_open(rf_image *image,rf_vpp *archive,const rf_vpp_entry *entry,uint32_t budget)
{
    unsigned char magic[4];int status;
    if(!image)return RF_RANGE;
    memset(image,0,sizeof(*image));
    if(!archive || !entry)return RF_RANGE;
    if(entry->size<4)return RF_FORMAT;
    status=rf_vpp_read(archive,entry,0,magic,4);if(status)return status;
    return !memcmp(magic,".vbm",4)?rf_image_vbm(image,archive,entry,budget):
        rf_image_tga(image,archive,entry,budget);
}
