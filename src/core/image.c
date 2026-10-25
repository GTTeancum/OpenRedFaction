#define RF_IMAGE_IMPLEMENTATION
#include "rf/image.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
const char *rf_image_audit_file;int rf_image_audit_line; /* RF_HEAP_AUDIT caller site */
#ifdef RF_IMAGE_XBOX_NATIVE
#include <xboxkrnl/xboxkrnl.h>
#include "../platform/xbox/contiguous_track.h"
#ifdef RF_HEAP_AUDIT
#define IMAGE_CONTIGUOUS(bytes) rf_contiguous_allocate((bytes),0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE,    rf_image_audit_file?rf_image_audit_file:__FILE__,rf_image_audit_file?rf_image_audit_line:__LINE__)
#else
#define IMAGE_CONTIGUOUS(bytes) MmAllocateContiguousMemoryEx((bytes),0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE)
#endif
#endif
int rf_image_is_packed_1555(const rf_image *image)
{
    return image && image->width && image->height && image->source_format==5 &&
        (uint64_t)image->width*image->height*2==image->bytes;
}
int rf_image_is_packed_4444(const rf_image *image)
{
    return image && image->width && image->height && image->source_format==RF_IMAGE_FORMAT_PACKED_4444 &&
        (uint64_t)image->width*image->height*2==image->bytes;
}
int rf_image_allocate_pixels(rf_image *image)
{
    if(!image || image->rgba || !image->width || !image->height || image->width>4096 || image->height>4096 ||
       ((uint64_t)image->width*image->height*4!=image->bytes && !rf_image_is_packed_1555(image) &&
        !rf_image_is_packed_4444(image)))return RF_RANGE;
#ifdef RF_IMAGE_XBOX_NATIVE
    if((image->width&(image->width-1)) || (image->height&(image->height-1)))return RF_FORMAT;
    image->rgba=IMAGE_CONTIGUOUS(image->bytes);
#else
    image->rgba=malloc(image->bytes);
#endif
    return image->rgba?RF_OK:RF_RANGE;
}
unsigned char *rf_image_pixel(const rf_image *image,uint32_t x,uint32_t y)
{
    /* Compressed blocks have no addressable texel; hand back scratch so a
     * stray reader cannot run past the smaller allocation. */
    static unsigned char compressed_scratch[4];
    if(image->compressed){memset(compressed_scratch,0,4);return compressed_scratch;}
#ifdef RF_IMAGE_XBOX_NATIVE
    uint32_t bit,index=0,destination=1;
    for(bit=1;bit<image->width || bit<image->height;bit<<=1) {
        if(bit<image->width){if(x&bit)index|=destination;destination<<=1;}
        if(bit<image->height){if(y&bit)index|=destination;destination<<=1;}
    }
#else
    uint32_t index=y*image->width+x;
#endif
    return image->rgba+index*(rf_image_is_packed_1555(image) || rf_image_is_packed_4444(image)?2u:4u);
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
/* Bulk pixel storage. Decoders convert whole rows into cached memory and
 * publish them with sequential 32-bit stores: row-major on PC, and on Xbox
 * one aligned band of rows at a time, written tile by tile in swizzled order
 * so write-combined texture memory receives full lines. The stored bytes are
 * identical to per-pixel rf_image_pixel writes; only the order of the stores
 * changes. Little-endian packing: byte0 red, byte3 alpha. */
#ifdef RF_IMAGE_XBOX_NATIVE
static uint32_t image_index(uint32_t width,uint32_t height,uint32_t x,uint32_t y)
{
    uint32_t bit,index=0,destination=1;
    for(bit=1;bit<width || bit<height;bit<<=1) {
        if(bit<width){if(x&bit)index|=destination;destination<<=1;}
        if(bit<height){if(y&bit)index|=destination;destination<<=1;}
    }
    return index;
}
/* Band height B<=min(width,height,8): every BxB tile of a band is one
 * contiguous run of B*B pixels whose low index bits interleave x0,y0,x1,... */
#define IMAGE_BAND_ROWS 8u
static const unsigned char image_tile_x[64]={
    0,1,0,1,2,3,2,3,0,1,0,1,2,3,2,3,4,5,4,5,6,7,6,7,4,5,4,5,6,7,6,7,
    0,1,0,1,2,3,2,3,0,1,0,1,2,3,2,3,4,5,4,5,6,7,6,7,4,5,4,5,6,7,6,7};
static const unsigned char image_tile_y[64]={
    0,0,1,1,0,0,1,1,2,2,3,3,2,2,3,3,0,0,1,1,0,0,1,1,2,2,3,3,2,2,3,3,
    4,4,5,5,4,4,5,5,6,6,7,7,6,6,7,7,4,4,5,5,4,4,5,5,6,6,7,7,6,6,7,7};
static uint32_t image_row_fallback[4096];
#endif
typedef struct image_rows {
    rf_image *image;uint32_t *band;uint32_t rows,filled,owned;
} image_rows;
static void image_rows_begin(image_rows *s,rf_image *image)
{
    memset(s,0,sizeof(*s));s->image=image;
#ifdef RF_IMAGE_XBOX_NATIVE
    s->rows=IMAGE_BAND_ROWS;
    if(s->rows>image->width)s->rows=image->width;
    if(s->rows>image->height)s->rows=image->height;
    s->band=malloc((size_t)image->width*s->rows*4u);
    if(s->band)s->owned=1;
    else {s->rows=1;s->band=image_row_fallback;}
#endif
}
static uint32_t *image_rows_target(image_rows *s,uint32_t y)
{
#ifdef RF_IMAGE_XBOX_NATIVE
    return s->band+(size_t)(y&(s->rows-1))*s->image->width;
#else
    return (uint32_t *)(void *)(s->image->rgba+(size_t)y*s->image->width*4u);
#endif
}
static void image_rows_commit(image_rows *s,uint32_t y)
{
#ifdef RF_IMAGE_XBOX_NATIVE
    uint32_t width=s->image->width,n=s->rows*s->rows,top,tx,i;
    if(++s->filled<s->rows)return;
    s->filled=0;top=y&~(s->rows-1);
    for(tx=0;tx<width;tx+=s->rows) {
        uint32_t *dst=(uint32_t *)(void *)s->image->rgba+image_index(width,s->image->height,tx,top);
        const uint32_t *src=s->band+tx;
        for(i=0;i<n;i++)dst[i]=src[(size_t)image_tile_y[i]*width+image_tile_x[i]];
    }
#else
    (void)s;(void)y;
#endif
}
static void image_rows_end(image_rows *s)
{
    if(s->owned)free(s->band);
    memset(s,0,sizeof(*s));
}

/* Scoped decoded-image sharing: refcounted pixel storage keyed by archive
 * entry identity. Only images explicitly published are ever shared; every
 * holder keeps an ordinary rf_image header and closes it normally. */
#define IMAGE_SHARE_SLOTS 64
typedef struct image_share {
    rf_image image;uint32_t refs,length,offset,size;char name[61];
} image_share;
static image_share image_shares[IMAGE_SHARE_SLOTS];
static int image_share_name(const char *a,const char *b)
{
    for(;;) {
        unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+='a'-'A';
        if(y>='A' && y<='Z')y+='a'-'A';
        if(x!=y)return 0;
        if(!x)return 1;
    }
}
int rf_image_share_find(const rf_vpp *archive,const rf_vpp_entry *entry,rf_image *image)
{
    uint32_t i;
    if(!archive || !entry || !image || !memchr(entry->name,0,sizeof(entry->name)))return RF_RANGE;
    for(i=0;i<IMAGE_SHARE_SLOTS;++i) {
        image_share *s=image_shares+i;
        if(!s->refs || s->length!=archive->length || s->offset!=entry->offset || s->size!=entry->size ||
           !image_share_name(s->name,entry->name))continue;
        ++s->refs;*image=s->image;return RF_OK;
    }
    return RF_NOT_FOUND;
}
void rf_image_share_publish(const rf_vpp *archive,const rf_vpp_entry *entry,const rf_image *image)
{
    uint32_t i;image_share *free_slot=NULL;
    if(!archive || !entry || !image || !image->rgba || !memchr(entry->name,0,sizeof(entry->name)))return;
    for(i=0;i<IMAGE_SHARE_SLOTS;++i) {
        if(image_shares[i].refs && image_shares[i].image.rgba==image->rgba)return;
        if(!image_shares[i].refs && !free_slot)free_slot=image_shares+i;
    }
    if(!free_slot)return; /* Unshared: the image remains an ordinary owner. */
    free_slot->image=*image;free_slot->refs=1;free_slot->length=archive->length;
    free_slot->offset=entry->offset;free_slot->size=entry->size;
    memcpy(free_slot->name,entry->name,sizeof(free_slot->name));
}
/* Returns nonzero when another holder still references the pixels. */
static int image_share_release(const unsigned char *rgba)
{
    uint32_t i;
    for(i=0;i<IMAGE_SHARE_SLOTS;++i)if(image_shares[i].refs && image_shares[i].image.rgba==rgba) {
        if(--image_shares[i].refs)return 1;
        memset(image_shares+i,0,sizeof(image_shares[i]));return 0;
    }
    return 0;
}

uint32_t rf_image_hash_pixels(uint32_t hash,const rf_image *image,uint32_t pixel_bytes)
{
    uint32_t x,y,k,element;
    if(!image || !image->rgba || !image->width || !image->height)return hash;
    if(image->compressed){for(k=0;k<image->bytes;k++)hash=(hash^image->rgba[k])*16777619u;return hash;} /* Block bytes. */
    element=rf_image_is_packed_1555(image)?2u:4u;
    if(pixel_bytes==element && image->width<=4096 && image->height<=4096 &&
       (uint64_t)image->width*image->height*element==image->bytes) {
#ifdef RF_IMAGE_XBOX_NATIVE
        /* Copy one band of swizzled tiles into cached memory with word reads
         * (texture memory is write-combined, so every CPU read is uncached),
         * then hash the band in logical row order. */
        uint32_t rows=IMAGE_BAND_ROWS,n,top,tx,i,row_bytes=image->width*element;unsigned char *band;
        if(rows>image->width)rows=image->width;
        if(rows>image->height)rows=image->height;
        n=rows*rows;band=malloc((size_t)row_bytes*rows);
        if(band && !(image->width&(image->width-1)) && !(image->height&(image->height-1))) {
            for(top=0;top<image->height;top+=rows) {
                for(tx=0;tx<image->width;tx+=rows) {
                    const unsigned char *src=image->rgba+(size_t)image_index(image->width,image->height,tx,top)*element;
                    if(element==4) {
                        for(i=0;i<n;i++) {
                            uint32_t word;memcpy(&word,src+(size_t)i*4,4);
                            memcpy(band+(size_t)image_tile_y[i]*row_bytes+(size_t)(tx+image_tile_x[i])*4,&word,4);
                        }
                    } else {
                        for(i=0;i<n;i+=2) {
                            uint32_t word;unsigned char *a,*b;
                            if(n==1){memcpy(band+(size_t)tx*2,src,2);break;}
                            memcpy(&word,src+(size_t)i*2,4);
                            a=band+(size_t)image_tile_y[i]*row_bytes+(size_t)(tx+image_tile_x[i])*2;
                            b=band+(size_t)image_tile_y[i+1]*row_bytes+(size_t)(tx+image_tile_x[i+1])*2;
                            a[0]=(unsigned char)word;a[1]=(unsigned char)(word>>8);
                            b[0]=(unsigned char)(word>>16);b[1]=(unsigned char)(word>>24);
                        }
                    }
                }
                for(i=0;i<row_bytes*rows;i++)hash=(hash^band[i])*16777619u;
            }
            free(band);return hash;
        }
        free(band);
#else
        {
            const unsigned char *p=image->rgba;uint32_t i,total=image->bytes;
            for(i=0;i<total;i++)hash=(hash^p[i])*16777619u;
            return hash;
        }
#endif
    }
    for(y=0;y<image->height;++y)for(x=0;x<image->width;++x) {
        const unsigned char *p=rf_image_pixel(image,x,y);
        for(k=0;k<pixel_bytes;k++)hash=(hash^p[k])*16777619u;
    }
    return hash;
}

int rf_image_dds(rf_image *image,rf_vpp *archive,const rf_vpp_entry *entry,uint32_t source_size,uint32_t limit,uint32_t budget)
{
    unsigned char h[128];uint32_t w,ht,levels,block,skip=0,offset=128,bytes=0,k,lw,lh;int status;
    if(!image)return RF_RANGE;
    memset(image,0,sizeof(*image));
    if(!archive || !entry || entry->size<128)return RF_FORMAT;
    status=rf_vpp_read(archive,entry,0,h,128);if(status)return status;
#define DDS32(o) ((uint32_t)h[o]|(uint32_t)h[(o)+1]<<8|(uint32_t)h[(o)+2]<<16|(uint32_t)h[(o)+3]<<24)
    if(memcmp(h,"DDS ",4) || DDS32(4)!=124 || memcmp(h+32,"RFDX",4))return RF_FORMAT;
    if(source_size && DDS32(48) && DDS32(48)!=source_size)return RF_NOT_FOUND;
    ht=DDS32(12);w=DDS32(16);levels=DDS32(28);
    block=!memcmp(h+84,"DXT5",4)?16:!memcmp(h+84,"DXT1",4)?8:0;
    if(!block || w<4 || ht<4 || w>4096 || ht>4096 || (w&(w-1)) || (ht&(ht-1)) || !levels || levels>13)return RF_FORMAT;
    for(k=0,lw=w,lh=ht;k<levels;k++,lw/=2,lh/=2) {
        uint32_t size;
        if(lw<4 || lh<4)return RF_FORMAT;
        size=(lw/4)*(lh/4)*block;
        if(limit && (lw>limit || lh>limit) && k+1<levels){offset+=size;++skip;continue;}
        bytes+=size;
    }
    if((uint64_t)offset+bytes>entry->size)return RF_FORMAT;
    if(bytes>budget)return RF_RANGE;
#ifdef RF_IMAGE_XBOX_NATIVE
    image->rgba=IMAGE_CONTIGUOUS(bytes);
#else
    image->rgba=malloc(bytes);
#endif
    if(!image->rgba)return RF_RANGE;
    image->width=w>>skip;image->height=ht>>skip;image->bytes=bytes;image->source_format=DDS32(36);
    image->compressed=block==16?5:1;image->mip_levels=levels-skip;image->alpha_mode=DDS32(40);
#undef DDS32
    status=rf_vpp_read(archive,entry,offset,image->rgba,bytes);
    if(status){rf_image_close(image);return status;}
    return RF_OK;
}
void rf_image_close(rf_image *image)
{
    if (!image)return;
    if(image->rgba && image_share_release(image->rgba)){memset(image,0,sizeof(*image));return;}
#ifdef RF_IMAGE_XBOX_NATIVE
    /* The renderer defers the free while queued GPU commands may read it. */
    {void rf_xbox_gpu_free(void *);if(image->rgba)rf_xbox_gpu_free(image->rgba);}
#else
    free(image->rgba);
#endif
    memset(image,0,sizeof(*image));
}
int rf_image_reduce(rf_image *image,uint32_t limit,uint32_t budget)
{
    rf_image next={0};uint32_t x,y,dx,dy,sx,sy,packed;int status;
    if(image && image->compressed)return RF_FORMAT; /* Packs carry their own mip levels. */
    if(!image || !image->rgba || !limit || limit>4096 || (limit&(limit-1)) ||
       !image->width || !image->height || image->width>4096 || image->height>4096 ||
       (image->width&(image->width-1)) || (image->height&(image->height-1)))return RF_RANGE;
    packed=rf_image_is_packed_1555(image);
    if(image->bytes!=(uint64_t)image->width*image->height*(packed?2:4) || image->bytes>budget)return RF_RANGE;
    if(image->width<=limit && image->height<=limit)return RF_OK;
    next.width=image->width;next.height=image->height;next.source_format=image->source_format;
    while(next.width>limit || next.height>limit){if(next.width>1)next.width/=2;if(next.height>1)next.height/=2;}
    next.bytes=next.width*next.height*4;
    if((uint64_t)next.bytes+image->bytes>budget)return RF_RANGE;
    status=rf_image_allocate_pixels(&next);if(status)return status;
    sx=image->width/next.width;sy=image->height/next.height;
    for(y=0;y<next.height;y++)for(x=0;x<next.width;x++) {
        uint32_t sum[4]={0},k,n=sx*sy;unsigned char *out=rf_image_pixel(&next,x,y);
        for(dy=0;dy<sy;dy++)for(dx=0;dx<sx;dx++) {
            const unsigned char *p=rf_image_pixel(image,x*sx+dx,y*sy+dy);
            /* One load per source pixel: native texture memory is
             * write-combined, so each CPU read is an uncached bus read. */
            if(packed){uint16_t h;uint32_t v;memcpy(&h,p,2);v=h;sum[0]+=((v>>10)&31)*255/31;sum[1]+=((v>>5)&31)*255/31;sum[2]+=(v&31)*255/31;sum[3]+=(v&0x8000)?255:0;}
            else {uint32_t w;memcpy(&w,p,4);sum[0]+=w&255;sum[1]+=(w>>8)&255;sum[2]+=(w>>16)&255;sum[3]+=w>>24;}
        }
        for(k=0;k<4;k++)out[k]=(unsigned char)((sum[k]+n/2)/n);
    }
    rf_image_close(image);*image=next;return RF_OK;
}

int rf_image_missing(rf_image *image,uint32_t budget)
{
    uint32_t x,y;int status;
    if(!image)return RF_RANGE;
    memset(image,0,sizeof(*image));
    if(budget<4096)return RF_RANGE;
    image->width=32;image->height=32;image->bytes=4096;image->source_format=6;
    status=rf_image_allocate_pixels(image);
    if(status){memset(image,0,sizeof(*image));return status;}
    for(y=0;y<32;++y)for(x=0;x<32;++x) {
        unsigned char *pixel=rf_image_pixel(image,x,y);
        int interior=(x&7) && (y&7);
        pixel[0]=pixel[1]=interior?0x79:0x40;
        pixel[2]=interior?0x8e:0x40;pixel[3]=255;
    }
    return RF_OK;
}

int rf_image_tga_limited(rf_image *image,rf_vpp *archive,const rf_vpp_entry *entry,uint32_t limit,uint32_t budget)
{
    rf_image value={0};reader r={0};unsigned char h[18],pixel[4],id[255];
    uint32_t sums[128][4],width,height,total,at=0,stride,alpha,sx,sy,samples,row_pixels;
    int status;
    if(!image || image->rgba || !archive || !entry || !limit || limit>128 || (limit&(limit-1)))return RF_RANGE;
    r.archive=archive;r.entry=entry;status=read_bytes(&r,h,sizeof(h));if(status)return status;
    width=h[12]|(uint32_t)h[13]<<8;height=h[14]|(uint32_t)h[15]<<8;alpha=h[17]&15;
    if(h[1] || (h[2]!=2 && h[2]!=10) || (h[16]!=24 && h[16]!=32) ||
       (h[17]&0xc0) || (alpha!=0 && alpha!=8) || (h[16]==24 && alpha) ||
       !width || !height || width>4096 || height>4096 ||
       (width&(width-1)) || (height&(height-1)))return RF_FORMAT;
    value.width=width;value.height=height;value.source_format=rf_image_tga_format(h[16]);
    while(value.width>limit || value.height>limit){if(value.width>1)value.width/=2;if(value.height>1)value.height/=2;}
    value.bytes=value.width*value.height*4;if(value.bytes>budget)return RF_RANGE;
    status=read_bytes(&r,id,h[0]);if(status)return status;
    status=rf_image_allocate_pixels(&value);if(status)return status;
    total=width*height;stride=h[16]/8;sx=width/value.width;sy=height/value.height;
    samples=sx*sy;row_pixels=width*sy;
    while(at<total){
        uint32_t count=1,repeat=0,i;
        if(h[2]==10){unsigned char packet;status=read_bytes(&r,&packet,1);if(status)goto failed;
            count=(packet&127)+1;repeat=packet&128;}
        if(count>total-at){status=RF_FORMAT;goto failed;}
        for(i=0;i<count;i++,at++){
            uint32_t column=(at%width)/sx;
            if(!(at%row_pixels))memset(sums,0,sizeof(sums));
            if(!repeat || !i){status=read_bytes(&r,pixel,stride);if(status)goto failed;}
            sums[column][0]+=pixel[2];sums[column][1]+=pixel[1];sums[column][2]+=pixel[0];
            sums[column][3]+=alpha==8?pixel[3]:255;
            if(!((at+1)%row_pixels)){
                uint32_t x,y=at/row_pixels,k;if(!(h[17]&32))y=value.height-1-y;
                for(x=0;x<value.width;x++){
                    unsigned char *out=rf_image_pixel(&value,(h[17]&16)?value.width-1-x:x,y);
                    for(k=0;k<4;k++)out[k]=(unsigned char)((sums[x][k]+samples/2)/samples);
                }
            }
        }
    }
    *image=value;return RF_OK;
failed:
    rf_image_close(&value);return status;
}
int rf_image_tga(rf_image *image, rf_vpp *archive, const rf_vpp_entry *entry, uint32_t budget)
{
    unsigned char h[18], id[255];
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
    /* Row-batched form of the per-pixel packet loop: identical packet
     * validation (count > remaining pixels), packet spans across rows,
     * orientation bits and channel order; reads are batched per run. */
    {
        image_rows rows;unsigned char chunk[256*4];
        uint32_t row,left=0,value=0,flip=(h[17]&16)!=0,rle=h[2]==10;
        image_rows_begin(&rows,image);
        for(row=0;row<height;++row) {
            uint32_t y=(h[17]&32)?row:height-1-row,x=0,*out=image_rows_target(&rows,y);
            while(x<width) {
                uint32_t n=width-x,i;
                if(rle && !left) {
                    unsigned char packet;
                    result=read_bytes(&r,&packet,1);if(result)goto rows_fail;
                    left=(packet&127)+1;
                    if(left>total-at){result=RF_FORMAT;goto rows_fail;}
                    if(packet&128) {
                        unsigned char p[4];
                        result=read_bytes(&r,p,stride);if(result)goto rows_fail;
                        value=(uint32_t)p[2]|(uint32_t)p[1]<<8|(uint32_t)p[0]<<16|(uint32_t)(alpha==8?p[3]:255)<<24;
                        left|=0x80000000u;
                    }
                }
                if(rle && n>(left&0x7fffffffu))n=left&0x7fffffffu;
                if(rle && (left&0x80000000u)) {
                    for(i=0;i<n;i++)out[flip?width-1-(x+i):x+i]=value;
                } else {
                    const unsigned char *p=chunk;
                    if(n>256)n=256;
                    result=read_bytes(&r,chunk,n*stride);if(result)goto rows_fail;
                    for(i=0;i<n;i++,p+=stride)
                        out[flip?width-1-(x+i):x+i]=(uint32_t)p[2]|(uint32_t)p[1]<<8|(uint32_t)p[0]<<16|
                            (uint32_t)(alpha==8?p[3]:255)<<24;
                }
                x+=n;at+=n;
                if(rle){left-=n;if(!(left&0x7fffffffu))left=0;}
            }
            image_rows_commit(&rows,y);
        }
        image_rows_end(&rows);
        return RF_OK;
rows_fail:
        image_rows_end(&rows);
    }
    rf_image_close(image);
    return result;
}

static uint32_t image_u32(const unsigned char *p)
{
    return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
int rf_image_tga_into(rf_image *atlas,uint32_t left,uint32_t top,uint32_t width,uint32_t height,
    rf_vpp *archive,const rf_vpp_entry *entry)
{
    reader r;unsigned char h[18],pixel[4],id[255];uint32_t at=0,total,stride,alpha;int status;
    if(!atlas || !atlas->rgba || !archive || !entry || !width || !height ||
       !atlas->width || !atlas->height || atlas->width>4096 || atlas->height>4096 ||
       (atlas->width&(atlas->width-1)) || (atlas->height&(atlas->height-1)) ||
       (uint64_t)atlas->width*atlas->height*4!=atlas->bytes ||
       (uint64_t)left+width>atlas->width || (uint64_t)top+height>atlas->height)return RF_RANGE;
    memset(&r,0,sizeof(r));r.archive=archive;r.entry=entry;
    status=read_bytes(&r,h,sizeof(h));if(status)return status;
    alpha=h[17]&15;
    if(h[1] || (h[2]!=2 && h[2]!=10) || (h[16]!=24 && h[16]!=32) ||
       (h[17]&0xc0) || (alpha!=0 && alpha!=8) || (h[16]==24 && alpha) ||
       (h[12]|(uint32_t)h[13]<<8)!=width || (h[14]|(uint32_t)h[15]<<8)!=height)return RF_FORMAT;
    status=read_bytes(&r,id,h[0]);if(status)return status;
    stride=h[16]/8;total=width*height;
    while(at<total) {
        uint32_t count=1,repeat=0,i;
        if(h[2]==10) {
            unsigned char packet;status=read_bytes(&r,&packet,1);if(status)return status;
            count=(packet&127)+1;repeat=packet&128;
        }
        if(count>total-at)return RF_FORMAT;
        for(i=0;i<count;++i,++at) {
            uint32_t x=at%width,y=at/width;unsigned char *dst;
            if(!repeat || !i){status=read_bytes(&r,pixel,stride);if(status)return status;}
            if(h[17]&16)x=width-1-x;if(!(h[17]&32))y=height-1-y;
            dst=rf_image_pixel(atlas,left+x,top+y);
            dst[0]=pixel[2];dst[1]=pixel[1];dst[2]=pixel[0];dst[3]=alpha==8?pixel[3]:255;
        }
    }
    return RF_OK;
}
static int vbm_decode(rf_image *image,rf_vpp *archive,const rf_vpp_entry *entry,uint32_t budget,
    uint32_t frame,int static_only,uint32_t *frame_count,uint32_t *frame_rate)
{
    reader r; unsigned char h[32];
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
    /* Row-batched; channel expansion tables hold the exact integer results of
     * c*255/31 and c*255/63 used by the per-pixel form. */
    {
        image_rows rows;unsigned char chunk[512*2],lut5[32],lut6[64];uint32_t y,x,n;
        /* 511200 calls 511410 for static format 5 with version below 2. */
        uint32_t flip=(format==0 && (frames>1 || image_u32(h+4)==1))?0x8000u:0u;
        for(i=0;i<32;i++)lut5[i]=(unsigned char)(i*255/31);
        for(i=0;i<64;i++)lut6[i]=(unsigned char)(i*255/63);
        image_rows_begin(&rows,image);
        for(y=0;y<height;y++) {
            uint32_t *out=image_rows_target(&rows,y);
            for(x=0;x<width;x+=n) {
                const unsigned char *p=chunk;
                n=width-x;if(n>512)n=512;
                status=read_bytes(&r,chunk,n*2);if(status){image_rows_end(&rows);rf_image_close(image);return status;}
                for(i=0;i<n;i++,p+=2) {
                    uint32_t v=((uint32_t)p[0]|(uint32_t)p[1]<<8)^flip,red,green,blue,alpha;
                    if(format==1) {
                        blue=(v&15)*17;green=((v>>4)&15)*17;red=((v>>8)&15)*17;alpha=(v>>12)*17;
                    } else if(format==2) {
                        blue=lut5[v&31];green=lut6[(v>>5)&63];red=lut5[(v>>11)&31];alpha=255;
                    } else {
                        blue=lut5[v&31];green=lut5[(v>>5)&31];red=lut5[(v>>10)&31];alpha=(v&32768)?255:0;
                    }
                    out[x+i]=red|green<<8|blue<<16|alpha<<24;
                }
            }
            image_rows_commit(&rows,y);
        }
        image_rows_end(&rows);
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
    /* Generic material loads use frame zero until their animation owner binds. */
    return !memcmp(magic,".vbm",4)?rf_image_vbm_frame(image,archive,entry,0,budget,NULL,NULL):
        rf_image_tga(image,archive,entry,budget);
}

int rf_image_sample_locked(const rf_image_sample_surface *surface,float u,float v,uint32_t *color)
{
    double x,y;uint32_t ix,iy,bpp,r,g,b,a,word;uint64_t offset;const unsigned char *pixel;
    if(!surface || !color || !surface->width || !surface->height || surface->width>INT32_MAX || surface->height>INT32_MAX)return RF_RANGE;
    if(!isfinite(u) || !isfinite(v))return RF_FORMAT;
    x=fmod((double)u,1);y=fmod((double)v,1);if(x<0)x+=1;if(y<0)y+=1;
    ix=(uint32_t)(x*surface->width+.5);iy=(uint32_t)(y*surface->height+.5);
    switch(surface->format){case 2:bpp=1;break;case 4:case 5:bpp=2;break;case 7:bpp=4;break;default:*color=0x00ffffff;return RF_OK;}
    if(!surface->pixels || (uint64_t)surface->width*bpp>surface->pitch)return RF_RANGE;
    offset=(uint64_t)iy*surface->pitch+(uint64_t)ix*bpp;
    if(offset+bpp>surface->bytes)return RF_RANGE;pixel=surface->pixels+(uint32_t)offset;
    if(surface->format==2){r=g=b=255;a=pixel[0];}
    else if(surface->format==7){r=pixel[2];g=pixel[1];b=pixel[0];a=pixel[3];}
    else {
        word=pixel[0]|(uint32_t)pixel[1]<<8;
        if(surface->format==4){r=((word>>8)&15)<<4;g=((word>>4)&15)<<4;b=(word&15)<<4;a=((word>>12)&15)<<4;}
        else {r=((word>>10)&31)<<3;g=((word>>5)&31)<<3;b=(word&31)<<3;a=(word&0x8000)?255:0;}
    }
    *color=r|(g<<8)|(b<<16)|(a<<24);return RF_OK;
}

int rf_image_sample_owned(const rf_image *image,float u,float v,uint32_t *color)
{
    double x,y;uint32_t ix,iy,packed,r,g,b,a,word;uint64_t index,total;const unsigned char *pixel;
    if(!image || !color || !image->rgba || !image->width || !image->height || image->width>4096 || image->height>4096)return RF_RANGE;
    packed=rf_image_is_packed_1555(image);total=(uint64_t)image->width*image->height;
    if(image->bytes!=total*(packed?2u:4u))return RF_RANGE;
    if((image->width&(image->width-1)) || (image->height&(image->height-1)))return RF_FORMAT;
    if(!isfinite(u) || !isfinite(v))return RF_FORMAT;
    x=fmod((double)u,1);y=fmod((double)v,1);if(x<0)x+=1;if(y<0)y+=1;
    ix=(uint32_t)(x*image->width+.5);iy=(uint32_t)(y*image->height+.5);
    if(image->source_format!=2 && image->source_format!=4 && image->source_format!=5 && image->source_format!=7){*color=0x00ffffff;return RF_OK;}
    index=(uint64_t)iy*image->width+ix;if(index>=total)return RF_RANGE;
    pixel=rf_image_pixel(image,(uint32_t)index%image->width,(uint32_t)index/image->width);
    if(packed) {
        word=pixel[0]|(uint32_t)pixel[1]<<8;r=((word>>10)&31)<<3;g=((word>>5)&31)<<3;b=(word&31)<<3;a=(word&0x8000)?255:0;
    } else {
        r=pixel[0];g=pixel[1];b=pixel[2];a=pixel[3];
        if(image->source_format==2)r=g=b=255;
        else if(image->source_format==4){r&=0xf0;g&=0xf0;b&=0xf0;a&=0xf0;}
        else if(image->source_format==5){r&=0xf8;g&=0xf8;b&=0xf8;a=(a&128)?255:0;}
    }
    *color=r|(g<<8)|(b<<16)|(a<<24);return RF_OK;
}
