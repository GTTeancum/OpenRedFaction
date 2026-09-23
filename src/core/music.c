#include "rf/music.h"
#include <math.h>
#include <string.h>

static uint32_t u16(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8);}
static uint32_t u32(const unsigned char *p){return u16(p)|(u16(p+2)<<16);}
static int16_t s16(const unsigned char *p){uint32_t v=u16(p);return v<32768?(int16_t)v:(int16_t)((int32_t)v-65536);}
static int16_t clamp16(int64_t v){return (int16_t)(v>32767?32767:v< -32768?-32768:v);}

static int music_header(rf_music_stream *s)
{
    unsigned char header[12],chunk[8],fmt[50],fact[4];uint32_t at=12,seen_fmt=0,seen_fact=0,seen_data=0,i;
    int status=rf_vpp_read(s->archive,&s->entry,0,header,sizeof(header));if(status)return status;
    if(memcmp(header,"RIFF",4)||memcmp(header+8,"WAVE",4)||u32(header+4)!=s->entry.size-8)return RF_FORMAT;
    while(at<s->entry.size) {
        uint32_t length,padded;
        if(s->entry.size-at<8)return RF_FORMAT;
        status=rf_vpp_read(s->archive,&s->entry,at,chunk,8);if(status)return status;
        length=u32(chunk+4);padded=length+(length&1u);
        if(length==UINT32_MAX||padded> s->entry.size-at-8)return RF_FORMAT;
        if(!memcmp(chunk,"fmt ",4)) {
            if(seen_fmt++||length!=50)return RF_FORMAT;
            status=rf_vpp_read(s->archive,&s->entry,at+8,fmt,50);if(status)return status;
            if(u16(fmt)!=2||u16(fmt+2)!=2||u32(fmt+4)!=22050||
               u16(fmt+12)!=1024||u16(fmt+14)!=4||u16(fmt+16)!=32||
               u16(fmt+18)!=1012||u16(fmt+20)!=7)return RF_FORMAT;
            for(i=0;i<7;i++){
                s->coefficients[i][0]=s16(fmt+22+i*4);
                s->coefficients[i][1]=s16(fmt+24+i*4);
            }
        } else if(!memcmp(chunk,"fact",4)) {
            if(seen_fact++||length!=4)return RF_FORMAT;
            status=rf_vpp_read(s->archive,&s->entry,at+8,fact,4);if(status)return status;
            s->total_frames=u32(fact);
        } else if(!memcmp(chunk,"data",4)) {
            if(seen_data++)return RF_FORMAT;
            s->data_offset=at+8;s->data_bytes=length;
        }
        at+=8+padded;
    }
    if(!seen_fmt||!seen_fact||!seen_data||!s->total_frames||
       !s->data_bytes||s->data_bytes%1024||
       (uint64_t)s->total_frames>(uint64_t)(s->data_bytes/1024)*1012||
       s->total_frames<=((s->data_bytes/1024)-1)*1012)return RF_FORMAT;
    return RF_OK;
}

static int music_block(rf_music_stream *s)
{
    static const uint16_t adaptation[16]={230,230,230,230,307,409,512,614,768,614,512,409,307,230,230,230};
    unsigned char raw[1024];int32_t delta[2],one[2],two[2];uint32_t pred[2],i,c,frames,base;
    int status;
    if(s->block_index>=s->data_bytes/1024)return RF_NOT_FOUND;
    status=rf_vpp_read(s->archive,&s->entry,s->data_offset+s->block_index*1024,raw,1024);if(status)return status;
    ++s->block_index;
    for(c=0;c<2;c++){
        pred[c]=raw[c];if(pred[c]>=7)return RF_FORMAT;
        delta[c]=(int32_t)u16(raw+2+c*2);if(delta[c]<16)return RF_FORMAT;
        one[c]=s16(raw+6+c*2);two[c]=s16(raw+10+c*2);
        s->decoded[c]=(int16_t)two[c];s->decoded[2+c]=(int16_t)one[c];
    }
    for(i=0;i<1010;i++)for(c=0;c<2;c++){
        uint32_t nibble=c?(raw[14+i]&15u):(raw[14+i]>>4);
        int32_t signed_nibble=nibble<8?(int32_t)nibble:(int32_t)nibble-16;
        int32_t value=(one[c]*s->coefficients[pred[c]][0]+two[c]*s->coefficients[pred[c]][1])/256;
        value=clamp16((int64_t)value+signed_nibble*(int64_t)delta[c]);two[c]=one[c];one[c]=value;
        s->decoded[(i+2)*2+c]=(int16_t)value;
        {int64_t next=(int64_t)delta[c]*adaptation[nibble]/256;
         delta[c]=(int32_t)(next>268435455?268435455:next<16?16:next);}
    }
    base=(s->block_index-1)*1012;frames=s->total_frames-base;
    s->decoded_frames=frames<1012?frames:1012;s->decoded_at=0;return RF_OK;
}

static int music_next(rf_music_stream *s,int16_t out[2])
{
    int status;
    if(s->decoded_at>=s->decoded_frames){status=music_block(s);if(status)return status;}
    out[0]=s->decoded[s->decoded_at*2];out[1]=s->decoded[s->decoded_at*2+1];++s->decoded_at;
    return RF_OK;
}

int rf_music_start(rf_music_stream *stream,rf_vpp *archive,const char *name)
{
    rf_music_stream next={0};int status;
    if(!stream||!archive||!archive->stream||!name||!name[0])return RF_RANGE;
    next.archive=archive;status=rf_vpp_find(archive,name,&next.entry);if(status)return status;
    if(next.entry.size<90)return RF_FORMAT;
    status=music_header(&next);if(status)return status;
    status=music_next(&next,next.current);if(status)return status;
    status=music_next(&next,next.next);if(status==RF_NOT_FOUND)memcpy(next.next,next.current,sizeof(next.next));
    else if(status)return status;
    next.active=1;*stream=next;return RF_OK;
}

void rf_music_stop(rf_music_stream *stream,float seconds)
{
    uint32_t frames;
    if(!stream||!stream->active)return;
    if(!isfinite(seconds)||seconds<=0){stream->active=0;return;}
    if(seconds>30)seconds=30;
    frames=(uint32_t)(seconds*48000.f);if(!frames)frames=1;
    stream->fade_remaining=stream->fade_total=frames;
}

int rf_music_mix(rf_music_stream *stream,int16_t *stereo,uint32_t frames)
{
    uint32_t i,c;int status;
    if(!stream||!stereo)return RF_RANGE;
    if(!stream->active)return RF_OK;
    for(i=0;i<frames&&stream->active;i++){
        for(c=0;c<2;c++){
            int32_t sample=(int32_t)(((int64_t)stream->current[c]*(48000-stream->phase)+
                (int64_t)stream->next[c]*stream->phase)/48000);
            if(stream->fade_total)sample=(int32_t)((int64_t)sample*stream->fade_remaining/stream->fade_total);
            stereo[i*2+c]=clamp16((int32_t)stereo[i*2+c]+sample);
        }
        if(stream->fade_total&&!--stream->fade_remaining){stream->active=0;break;}
        stream->phase+=22050;
        if(stream->phase>=48000){
            stream->phase-=48000;
            memcpy(stream->current,stream->next,sizeof(stream->current));
            status=music_next(stream,stream->next);
            if(status==RF_NOT_FOUND){stream->active=0;break;}
            if(status){stream->active=0;return status;}
        }
    }
    return RF_OK;
}

void rf_music_reset(rf_music_stream *stream){if(stream)memset(stream,0,sizeof(*stream));}
