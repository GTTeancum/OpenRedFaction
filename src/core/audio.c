#include "rf/audio.h"
#include <string.h>
static uint32_t wave_u16(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8);}
static uint32_t wave_u32(const uint8_t *p){return wave_u16(p)|(wave_u16(p+2)<<16);}
int rf_wave_pcm_parse(const void *data,uint32_t size,rf_wave_pcm *result)
{
    const uint8_t *p=data;rf_wave_pcm value={0};uint32_t at=12,format=0,found_data=0,align=0;
    if(!p || !result)return RF_RANGE;
    if(size<12 || memcmp(p,"RIFF",4) || memcmp(p+8,"WAVE",4) || wave_u32(p+4)!=size-8)return RF_FORMAT;
    while(at<size) {
        uint32_t length,padded;
        if(size-at<8)return RF_FORMAT;
        length=wave_u32(p+at+4);if(length>size-at-8)return RF_FORMAT;
        if(length==UINT32_MAX)return RF_FORMAT;
        padded=length+(length&1u);if(padded>size-at-8)return RF_FORMAT;
        if(!memcmp(p+at,"fmt ",4)) {
            const uint8_t *f=p+at+8;uint32_t rate_bytes;
            if(format || length<16 || wave_u16(f)!=1)return RF_FORMAT;
            value.channels=wave_u16(f+2);value.rate=wave_u32(f+4);rate_bytes=wave_u32(f+8);
            align=wave_u16(f+12);value.bits=wave_u16(f+14);
            if((value.channels!=1 && value.channels!=2) || (value.bits!=8 && value.bits!=16) || !value.rate ||
               align!=value.channels*(value.bits/8) || (uint64_t)value.rate*align!=rate_bytes)return RF_FORMAT;
            if(length!=16 && (length<18 || wave_u16(f+16)>length-18))return RF_FORMAT;
            format=1;
        } else if(!memcmp(p+at,"data",4)) {
            if(found_data)return RF_FORMAT;
            found_data=1;value.samples=p+at+8;value.bytes=length;
        }
        at+=8+padded;
    }
    if(!format || !found_data || value.bytes%align)return RF_FORMAT;
    value.frames=value.bytes/align;*result=value;return RF_OK;
}
