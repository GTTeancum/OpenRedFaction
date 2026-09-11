#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
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
    {uint8_t *borrowed=VirtualAlloc(NULL,48000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);DWORD old;
     rf_wave_pcm a={borrowed,48000,24000,48000,1,16};
     rf_wave_pcm b={(const uint8_t *)pcm,sizeof(pcm),24000,48000,1,16};
     if(!borrowed || rf_pc_audio_open())return 5;
     /* Zero-filled releasable sample; only the other voice can generate nonzero output. */
     rf_pc_audio_events.play(NULL,1001,&a,1,1);rf_pc_audio_events.play(NULL,1002,&b,0,0);Sleep(50);
     if(rf_pc_audio_release_voice(1001) || rf_pc_audio_release_voice(1001)!=RF_NOT_FOUND ||
        rf_pc_audio_release_voice(0xdeadbeef)!=RF_NOT_FOUND)return 6;
     if(!VirtualProtect(borrowed,48000,PAGE_NOACCESS,&old))return 7;
     rf_pc_audio_events.gain(NULL,1002,1,1);Sleep(100);
     if(rf_pc_audio_release_voice(1002))return 8;
     rf_pc_audio_close();VirtualFree(borrowed,0,MEM_RELEASE);
     if(rf_pc_audio_diagnostic[0] || rf_pc_audio_diagnostic[2]<4800 || !rf_pc_audio_diagnostic[3] ||
        rf_pc_audio_diagnostic[4]!=2 || rf_pc_audio_diagnostic[5] || rf_pc_audio_diagnostic[7])return 9;
     puts("PASS selective release with protected source pages");}
    {uint8_t *borrowed=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);DWORD old;
     rf_wave_pcm sample={borrowed,128,64,48000,1,16};uint32_t before;
     if(!borrowed)return 16;memcpy(borrowed,pcm,128);
     if(rf_pc_audio_events.play_mode(NULL,2001,&sample,1,1,1)!=RF_NOT_FOUND || rf_pc_audio_open())return 10;
     if(rf_pc_audio_events.play_mode(NULL,2001,&sample,.5f,.5f,1))return 11;
     Sleep(150);before=rf_pc_audio_diagnostic[3];Sleep(150);
     if(!before || rf_pc_audio_diagnostic[3]<=before)return 12;
     rf_pc_audio_events.stop(NULL,2001);
     if(!VirtualProtect(borrowed,4096,PAGE_NOACCESS,&old))return 17;
     Sleep(100);before=rf_pc_audio_diagnostic[3];Sleep(100);
     if(rf_pc_audio_diagnostic[3]!=before || rf_pc_audio_release_voice(2001)!=RF_NOT_FOUND)return 13;
     if(rf_pc_audio_events.play_mode(NULL,2002,&sample,1,1,2)!=RF_RANGE)return 14;
     rf_pc_audio_close();VirtualFree(borrowed,0,MEM_RELEASE);
     if(rf_pc_audio_diagnostic[4]!=1 || rf_pc_audio_diagnostic[5]!=1 || rf_pc_audio_diagnostic[7])return 15;
     puts("PASS static loop beyond endpoint, stop releases protected source, explicit mode errors");}
    return 0;
}
