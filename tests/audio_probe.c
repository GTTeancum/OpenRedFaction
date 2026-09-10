#include "rf/audio.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(void)
{
    uint32_t size;_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&size,4,1,stdin)==1) {
        uint8_t *data;rf_wave_pcm view,before;uint32_t out[7]={0};int status;
        if(size>8*1024*1024)return 2;data=malloc(size?size:1);if(!data)return 3;
        if(fread(data,1,size,stdin)!=size){free(data);return 4;}
        memset(&view,0xa5,sizeof(view));before=view;status=rf_wave_pcm_parse(data,size,&view);out[0]=(uint32_t)status;
        if(status){if(memcmp(&view,&before,sizeof(view))){free(data);return 5;}}
        else {out[1]=(uint32_t)(view.samples-data);out[2]=view.bytes;out[3]=view.frames;out[4]=view.rate;out[5]=view.channels;out[6]=view.bits;}
        free(data);if(fwrite(out,sizeof(out),1,stdout)!=1)return 6;
    }
    return ferror(stdin)?7:0;
}
