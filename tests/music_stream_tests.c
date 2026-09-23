#include "rf/music.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"music check failed: %s at %d\n",#x,__LINE__);return 1;}} while(0)

int main(int argc,char **argv)
{
    static const struct {uint32_t frame;int16_t left,right;} reference[]={
        {2024,20,0},{2025,15,0},{2026,10,0},{2027,5,0},
        {2030,1,0},{2040,7,0},{2050,124,0},{2100,-14,-8},
        {2200,22,-14},{2300,5,105},{2500,9,150},{3000,17,92}
    };
    rf_vpp archive={0};rf_music_stream stream={0};int16_t mixed[800*2];uint32_t i,blocks=0,heard=0;
    CHECK(argc==2);CHECK(rf_vpp_open(&archive,argv[1])==RF_OK);
    CHECK(rf_music_start(&stream,&archive,"Tribes_Ext.wav")==RF_OK);
    CHECK(sizeof(stream)<5000 && stream.total_frames>2000000);
    while(stream.block_index<3 && blocks<20) {
        memset(mixed,0,sizeof(mixed));CHECK(rf_music_mix(&stream,mixed,800)==RF_OK);
        for(i=0;i<1600;i++)heard+=(mixed[i]!=0);
        ++blocks;
    }
    CHECK(stream.block_index==3 && heard>0);
    /* Independent ffmpeg 8.1.1 decode of installed Tribes_Ext.wav at native
     * 22050 Hz; compare the decoded block, before our 48 kHz resampler. */
    for(i=0;i<sizeof(reference)/sizeof(reference[0]);i++) {
        uint32_t at=reference[i].frame-2024;
        CHECK(stream.decoded[at*2]==reference[i].left);
        CHECK(stream.decoded[at*2+1]==reference[i].right);
    }
    rf_music_stop(&stream,.1f);
    for(i=0;i<6;i++){memset(mixed,0,sizeof(mixed));CHECK(rf_music_mix(&stream,mixed,800)==RF_OK);}
    CHECK(stream.active==0 && stream.fade_remaining==0);
    rf_music_reset(&stream);rf_vpp_close(&archive);
    puts("music stream PASS");return 0;
}
