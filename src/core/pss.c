#include "rf/pss.h"
#include "rf/vpp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PSS reader (see rf/pss.h). Program stream syntax per ISO/IEC 13818-1
 * 2.5.3 (pack header, PES packets). The audio layout was read from the
 * user's own PS2 disc (docs/FMV-PLAN.md): each private-stream-1 payload is
 * substream 0xFF plus three more bytes, then the SShd/SSbd stream. */
#define PSS_INPUT 65536u
#define PSS_VIDEO_LIMIT (2u*1024u*1024u)
#define PSS_AUDIO_FRAMES 96000u /* two seconds at 48 kHz */

struct rf_pss {
    FILE *file;uint32_t eof;
    uint8_t input[PSS_INPUT];uint32_t in_pos,in_len;
    uint8_t *video;uint32_t video_len,video_cap,video_start,video_scan;
    uint8_t audio_head[40];uint32_t audio_head_len,audio_ready;
    uint8_t *audio_block;uint32_t audio_block_len; /* one channels*interleave group */
    int16_t *ring;uint32_t ring_read,ring_count;
    rf_pss_info info;
};

int rf_pss_open(rf_pss **out,const char *path)
{
    rf_pss *pss;
    if(!out || !path)return RF_RANGE;
    *out=NULL;
    pss=calloc(1,sizeof(*pss));if(!pss)return RF_RANGE;
    pss->file=fopen(path,"rb");
    pss->video_cap=262144u;pss->video=malloc(pss->video_cap);
    pss->ring=malloc(PSS_AUDIO_FRAMES*2*sizeof(int16_t));
    if(!pss->file || !pss->video || !pss->ring){int missing=!pss->file;rf_pss_close(pss);return missing?RF_NOT_FOUND:RF_RANGE;}
    *out=pss;return RF_OK;
}
void rf_pss_close(rf_pss *pss)
{
    if(!pss)return;
    if(pss->file)fclose(pss->file);
    free(pss->video);free(pss->audio_block);free(pss->ring);free(pss);
}
const rf_pss_info *rf_pss_get_info(const rf_pss *pss){return pss?&pss->info:NULL;}
uint32_t rf_pss_audio_available(const rf_pss *pss){return pss?pss->ring_count:0;}

/* At least n bytes buffered at in_pos (fewer only at the end of the file). */
static void input_need(rf_pss *pss,uint32_t n)
{
    if(pss->in_len-pss->in_pos>=n || pss->eof)return;
    memmove(pss->input,pss->input+pss->in_pos,pss->in_len-pss->in_pos);
    pss->in_len-=pss->in_pos;pss->in_pos=0;
    while(pss->in_len<n && !pss->eof){
        size_t got=fread(pss->input+pss->in_len,1,PSS_INPUT-pss->in_len,pss->file);
        if(!got)pss->eof=1;
        pss->in_len+=(uint32_t)got;
    }
}

static int video_append(rf_pss *pss,const uint8_t *data,uint32_t bytes)
{
    if(pss->video_start && pss->video_len+bytes>pss->video_cap){
        memmove(pss->video,pss->video+pss->video_start,pss->video_len-pss->video_start);
        pss->video_len-=pss->video_start;pss->video_scan-=pss->video_start;pss->video_start=0;
    }
    if(pss->video_len+bytes>pss->video_cap){
        uint32_t cap=pss->video_cap*2;uint8_t *grown;
        while(cap<pss->video_len+bytes)cap*=2;
        if(cap>PSS_VIDEO_LIMIT)return RF_RANGE;
        grown=realloc(pss->video,cap);if(!grown)return RF_RANGE;
        pss->video=grown;pss->video_cap=cap;
    }
    memcpy(pss->video+pss->video_len,data,bytes);pss->video_len+=bytes;
    return RF_OK;
}

static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void audio_push(rf_pss *pss,int16_t left,int16_t right)
{
    uint32_t at;
    if(pss->ring_count>=PSS_AUDIO_FRAMES){++pss->info.audio_dropped;return;}
    at=(pss->ring_read+pss->ring_count)%PSS_AUDIO_FRAMES;
    pss->ring[at*2]=left;pss->ring[at*2+1]=right;++pss->ring_count;++pss->info.audio_frames;
}
static int audio_append(rf_pss *pss,const uint8_t *data,uint32_t bytes)
{
    while(bytes && !pss->audio_ready){
        uint32_t take=40-pss->audio_head_len;if(take>bytes)take=bytes;
        memcpy(pss->audio_head+pss->audio_head_len,data,take);pss->audio_head_len+=take;data+=take;bytes-=take;
        if(pss->audio_head_len==40){
            const uint8_t *h=pss->audio_head;
            if(memcmp(h,"SShd",4) || memcmp(h+32,"SSbd",4))return RF_FORMAT;
            pss->info.audio_format=le32(h+8);pss->info.audio_rate=le32(h+12);pss->info.audio_channels=le32(h+16);
            pss->info.audio_interleave=le32(h+20);pss->info.audio_bytes=le32(h+36);
            /* Format 1 is 16-bit PCM; one or two channels, even-sized blocks. */
            if(pss->info.audio_format!=1 || !pss->info.audio_channels || pss->info.audio_channels>2 ||
               !pss->info.audio_interleave || pss->info.audio_interleave>65536 || (pss->info.audio_interleave&1))return RF_FORMAT;
            pss->audio_block=malloc(pss->info.audio_interleave*pss->info.audio_channels);
            if(!pss->audio_block)return RF_RANGE;
            pss->audio_ready=1;
        }
    }
    while(bytes){
        uint32_t group=pss->info.audio_interleave*pss->info.audio_channels,take=group-pss->audio_block_len,i;
        if(take>bytes)take=bytes;
        memcpy(pss->audio_block+pss->audio_block_len,data,take);pss->audio_block_len+=take;data+=take;bytes-=take;
        if(pss->audio_block_len<group)break;
        for(i=0;i<pss->info.audio_interleave/2;i++){
            const uint8_t *l=pss->audio_block+i*2,*r=l+(pss->info.audio_channels==2?pss->info.audio_interleave:0);
            audio_push(pss,(int16_t)(l[0]|l[1]<<8),(int16_t)(r[0]|r[1]<<8));
        }
        pss->audio_block_len=0;
    }
    return RF_OK;
}

/* Reads the next pack or packet. RF_NOT_FOUND at the end of the stream. */
static int packet_read(rf_pss *pss)
{
    const uint8_t *p;uint32_t id,length;
    for(;;){
        input_need(pss,6);
        if(pss->in_len-pss->in_pos<4)return RF_NOT_FOUND;
        p=pss->input+pss->in_pos;
        if(!p[0] && !p[1] && p[2]==1)break;
        ++pss->in_pos; /* resynchronise on the next start code */
    }
    id=p[3];
    if(id==0xb9){pss->in_pos+=4;return RF_NOT_FOUND;}
    if(id==0xba){
        input_need(pss,14);p=pss->input+pss->in_pos;
        if(pss->in_len-pss->in_pos<14)return RF_NOT_FOUND;
        pss->in_pos+=(p[4]&0xc0)==0x40?14u+(p[13]&7u):12u; /* MPEG-2 or MPEG-1 pack header */
        return RF_OK;
    }
    if(pss->in_len-pss->in_pos<6)return RF_NOT_FOUND;
    length=(uint32_t)p[4]<<8|p[5];
    input_need(pss,6+length);p=pss->input+pss->in_pos;
    if(pss->in_len-pss->in_pos<6+length)return RF_NOT_FOUND;
    ++pss->info.packets;
    if((id==0xe0 || id==0xbd) && length>=3){
        const uint8_t *body=p+6;uint32_t header=3u+body[2];int status=RF_OK;
        if(header<=length){
            if(id==0xe0)status=video_append(pss,body+header,length-header);
            else if(length>=header+4 && body[header]==0xff)status=audio_append(pss,body+header+4,length-header-4);
        }
        if(status)return status;
    }
    pss->in_pos+=6+length;
    return RF_OK;
}

/* Start code positions in the video buffer from 'from'; returns the offset
 * of the first whose code satisfies want, or UINT32_MAX. */
static uint32_t video_find(const rf_pss *pss,uint32_t from,int boundary)
{
    uint32_t i;
    for(i=from;i+3<pss->video_len;i++){
        const uint8_t *p=pss->video+i;
        if(p[0] || p[1] || p[2]!=1)continue;
        if(!boundary){if(!p[3])return i;}
        else if(!p[3] || p[3]==0xb3 || p[3]==0xb7 || p[3]==0xb8)return i;
    }
    return UINT32_MAX;
}

int rf_pss_next_unit(rf_pss *pss,const uint8_t **unit,uint32_t *bytes)
{
    if(!pss || !unit || !bytes)return RF_RANGE;
    for(;;){
        uint32_t picture=video_find(pss,pss->video_start,0),end=UINT32_MAX;int status;
        if(picture!=UINT32_MAX){
            uint32_t from=pss->video_scan>picture+4?pss->video_scan:picture+4;
            end=video_find(pss,from,1);
            if(end==UINT32_MAX && pss->video_len>3)pss->video_scan=pss->video_len-3;
        }
        if(end!=UINT32_MAX){
            *unit=pss->video+pss->video_start;*bytes=end-pss->video_start;
            pss->video_start=end;pss->video_scan=end;++pss->info.units;return RF_OK;
        }
        status=packet_read(pss);
        if(status==RF_NOT_FOUND){
            if(picture==UINT32_MAX || pss->video_start>=pss->video_len)return RF_NOT_FOUND;
            *unit=pss->video+pss->video_start;*bytes=pss->video_len-pss->video_start;
            pss->video_start=pss->video_scan=pss->video_len;++pss->info.units;return RF_OK;
        }
        if(status)return status;
    }
}

uint32_t rf_pss_audio_read(rf_pss *pss,int16_t *frames,uint32_t max_frames)
{
    uint32_t n=0;
    if(!pss || !frames)return 0;
    while(n<max_frames && pss->ring_count){
        frames[n*2]=pss->ring[pss->ring_read*2];frames[n*2+1]=pss->ring[pss->ring_read*2+1];
        pss->ring_read=(pss->ring_read+1)%PSS_AUDIO_FRAMES;--pss->ring_count;++n;
    }
    return n;
}
