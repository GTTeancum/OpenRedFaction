#include "rf/liquid_wave.h"
#include "rf/vpp.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* 4e63f0: 256 floats, sin(i * 0.024543693f). */
static float liquid_sine[256];static int liquid_sine_ready;
static void liquid_sine_build(void)
{
    uint32_t i;if(liquid_sine_ready)return;
    for(i=0;i<256;i++)liquid_sine[i]=(float)sin((double)i*(double)0.024543693f);
    liquid_sine_ready=1;
}
static int liquid_pow2(uint32_t v){return v && v<=256 && !(v&(v-1));}
int rf_liquid_wave_open(rf_liquid_wave *wave,const rf_image *image)
{
    rf_liquid_wave value={0};
    if(!wave || wave->source || !image || !image->rgba)return RF_RANGE;
    if(!liquid_pow2(image->width) || !liquid_pow2(image->height))return RF_NOT_FOUND; /* 4e6550 refuses >256. */
    value.texel=image->bytes/(image->width*image->height);
    if(value.texel!=2 && value.texel!=4)return RF_FORMAT;
    value.source=malloc(image->bytes);if(!value.source)return RF_IO;
    memcpy(value.source,image->rgba,image->bytes);
    value.bytes=image->bytes;value.width=image->width;value.height=image->height;
    liquid_sine_build();*wave=value;return RF_OK;
}
/* x87 __ftol: truncation toward zero, stored to a byte table. */
static unsigned char liquid_byte(double v){return (unsigned char)(int32_t)v;}
int rf_liquid_wave_update(rf_liquid_wave *wave,rf_image *image)
{
    unsigned char t0[256],t1[256],t2[256];rf_image source;uint32_t x,y;
    float column=10.f*256.f/(float)wave->width,row_shift,row_source;
    if(!wave || !wave->source || !image || !image->rgba || image->width!=wave->width ||
       image->height!=wave->height || image->bytes!=wave->bytes)return RF_RANGE;
    row_shift=6.f*256.f/(float)wave->height;row_source=10.f*256.f/(float)wave->height;
    for(x=0;x<wave->width;x++)
        t0[x]=liquid_byte(1.3f*liquid_sine[(int32_t)((float)x*column+wave->phase_34)&255]);
    for(y=0;y<wave->height;y++) {
        t1[y]=liquid_byte(2.f*liquid_sine[(int32_t)((float)y*row_shift+wave->phase_2c)&255]);
        t2[y]=liquid_byte(5.f*liquid_sine[(int32_t)((float)y*row_source+wave->phase_30)&255]+(float)y);
    }
    wave->phase_2c+=12.8f;wave->phase_34+=3.8787879f;wave->phase_30+=4.2666669f;
    source=*image;source.rgba=wave->source;
    for(y=0;y<wave->height;y++)for(x=0;x<wave->width;x++) {
        uint32_t sx=(x+t1[y])&(wave->width-1),sy=((uint32_t)t0[x]+t2[y])&(wave->height-1);
        memcpy(rf_image_pixel(image,x,y),rf_image_pixel(&source,sx,sy),wave->texel);
    }
    return RF_OK;
}
void rf_liquid_wave_close(rf_liquid_wave *wave)
{if(wave){free(wave->source);memset(wave,0,sizeof(*wave));}}
