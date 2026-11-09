/* Host check for the PSS reader and intra-only MPEG-2 decoder (development
 * only; not part of either game target). Decodes a PS2 movie, writes the
 * pictures as raw 4:2:0 frames (Y, U, V cropped to the picture size) and the
 * audio as a 16-bit stereo WAV, and prints counts and decode time.
 *   pss_check <movie.pss> <frames.yuv> <audio.wav> [max pictures] */
#include "rf/mpeg2_intra.h"
#include "rf/pss.h"
#include "rf/vpp.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void wav_header(FILE *f,uint32_t rate,uint32_t frames)
{
    uint32_t data=frames*4,v;
    fwrite("RIFF",1,4,f);v=36+data;fwrite(&v,4,1,f);fwrite("WAVEfmt ",1,8,f);
    v=16;fwrite(&v,4,1,f);v=1|(2u<<16);fwrite(&v,4,1,f);fwrite(&rate,4,1,f);
    v=rate*4;fwrite(&v,4,1,f);v=4|(16u<<16);fwrite(&v,4,1,f);fwrite("data",1,4,f);fwrite(&data,4,1,f);
}

int main(int argc,char **argv)
{
    rf_pss *pss;rf_mpeg2_intra decoder;const uint8_t *unit;uint32_t bytes,decoded,limit,audio_frames=0,i;
    FILE *frames,*audio;int status;clock_t spent=0;static int16_t samples[4096*2];
    if(argc<4){fprintf(stderr,"usage: pss_check movie.pss frames.yuv audio.wav [max]\n");return 2;}
    limit=argc>4?(uint32_t)strtoul(argv[4],NULL,10):UINT32_MAX;
    if(rf_pss_open(&pss,argv[1])){fprintf(stderr,"open failed\n");return 1;}
    if(rf_mpeg2_intra_open(&decoder,720,576)){fprintf(stderr,"decoder open failed\n");return 1;}
    frames=fopen(argv[2],"wb");audio=fopen(argv[3],"wb");
    if(!frames || !audio){fprintf(stderr,"output open failed\n");return 1;}
    wav_header(audio,48000,0);
    while(decoder.pictures<limit && !(status=rf_pss_next_unit(pss,&unit,&bytes))){
        clock_t start=clock();
        status=rf_mpeg2_intra_decode(&decoder,unit,bytes,&decoded);
        spent+=clock()-start;
        if(status){fprintf(stderr,"decode error %d at unit %u\n",status,rf_pss_get_info(pss)->units);break;}
        if(decoded){
            for(i=0;i<decoder.height;i++)fwrite(decoder.planes[0]+i*decoder.stride[0],1,decoder.width,frames);
            for(i=0;i<decoder.height/2;i++)fwrite(decoder.planes[1]+i*decoder.stride[1],1,decoder.width/2,frames);
            for(i=0;i<decoder.height/2;i++)fwrite(decoder.planes[2]+i*decoder.stride[2],1,decoder.width/2,frames);
        }
        for(;;){uint32_t n=rf_pss_audio_read(pss,samples,4096);if(!n)break;fwrite(samples,4,n,audio);audio_frames+=n;}
    }
    {const rf_pss_info *info=rf_pss_get_info(pss);
     printf("picture %ux%u pictures %u skipped %u errors %u units %u\n",decoder.width,decoder.height,decoder.pictures,decoder.skipped,decoder.errors,info->units);
     printf("audio format %u rate %u channels %u interleave %u frames %u dropped %u\n",info->audio_format,info->audio_rate,
         info->audio_channels,info->audio_interleave,audio_frames,info->audio_dropped);
     printf("decode %.3f s (%.2f ms per picture)\n",(double)spent/CLOCKS_PER_SEC,decoder.pictures?1000.0*spent/CLOCKS_PER_SEC/decoder.pictures:0.0);
     if(info->audio_rate)wav_header((fseek(audio,0,SEEK_SET),audio),info->audio_rate,audio_frames);}
    fclose(frames);fclose(audio);rf_mpeg2_intra_close(&decoder);rf_pss_close(pss);
    return 0;
}
