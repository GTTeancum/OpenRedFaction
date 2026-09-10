#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "rf/audio.h"
#include "audio.h"
static int16_t pcm[24000];
int main(void)
{
    unsigned i,pass;
    for(i=0;i<24000;i++)pcm[i]=(i&32)?32:-32;
    for(pass=0;pass<2;pass++) {
        rf_wave_pcm sample={(const uint8_t *)pcm,sizeof(pcm),24000,48000,1,16};
        if(rf_pc_audio_open())return 1;
        rf_pc_audio_events.play(NULL,123,&sample,1,1);
        rf_pc_audio_events.gain(NULL,123,.25f,.5f);
        Sleep(200);
        rf_pc_audio_events.stop(NULL,123);rf_pc_audio_events.reset(NULL);
        if(rf_pc_audio_diagnostic[0] || rf_pc_audio_diagnostic[2]<4800 || !rf_pc_audio_diagnostic[3] ||
           rf_pc_audio_diagnostic[4]!=1 || rf_pc_audio_diagnostic[5] || rf_pc_audio_diagnostic[7])return 2;
        printf("PASS open=%u frames=%u nonzero=%u bytes=%u\n",pass,rf_pc_audio_diagnostic[2],rf_pc_audio_diagnostic[3],rf_pc_audio_diagnostic[6]);
    }
    {rf_wave_pcm sample={(const uint8_t *)pcm,sizeof(pcm),24000,48000,1,16};
     if(rf_pc_audio_open())return 3;
     rf_pc_audio_events.play(NULL,999,&sample,0,0);Sleep(100);rf_pc_audio_events.reset(NULL);
     if(rf_pc_audio_diagnostic[0] || rf_pc_audio_diagnostic[2]<2400 || rf_pc_audio_diagnostic[3] ||
        rf_pc_audio_diagnostic[4]!=1 || rf_pc_audio_diagnostic[5] || rf_pc_audio_diagnostic[7])return 4;
     puts("PASS muted start");}
    return 0;
}
