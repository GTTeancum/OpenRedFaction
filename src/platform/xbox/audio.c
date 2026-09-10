/* Port output adapter; uses the installed NXDK SDL audio driver, not Miles. */
#include "audio.h"
#include <SDL.h>
#include <string.h>
#define QUEUE_FRAMES 3200u
static int16_t queue[QUEUE_FRAMES*2];
static uint32_t read_at,queued;
static SDL_AudioDeviceID device;
static int initialized,started;
uint32_t rf_xbox_audio_diagnostic[8];
uint32_t rf_xbox_audio_close_phase;
static void consume(void *context,Uint8 *output,int bytes)
{
    uint32_t frames=(uint32_t)bytes/4,i,available;
    (void)context;memset(output,0,(size_t)bytes);
    available=frames<queued?frames:queued;
    for(i=0;i<available;i++) {
        const uint8_t *source=(const uint8_t *)(queue+read_at*2);uint32_t b;
        memcpy(output+i*4,source,4);
        if(queue[read_at*2] || queue[read_at*2+1])++rf_xbox_audio_diagnostic[5];
        for(b=0;b<4;b++)rf_xbox_audio_diagnostic[6]=(rf_xbox_audio_diagnostic[6]^source[b])*16777619u;
        read_at=(read_at+1)%QUEUE_FRAMES;
    }
    queued-=available;rf_xbox_audio_diagnostic[2]+=available;
    rf_xbox_audio_diagnostic[4]+=frames-available;
}
int rf_xbox_audio_open(void)
{
    SDL_AudioSpec wanted={0},obtained;
    if(initialized)return RF_RANGE;
    memset(rf_xbox_audio_diagnostic,0,sizeof(rf_xbox_audio_diagnostic));
    rf_xbox_audio_diagnostic[6]=2166136261u;rf_xbox_audio_diagnostic[7]=sizeof(queue);
    read_at=queued=0;started=0;
    rf_xbox_audio_close_phase=0;
    if(SDL_InitSubSystem(SDL_INIT_AUDIO))goto failure;
    initialized=1;wanted.freq=48000;wanted.format=AUDIO_S16LSB;
    wanted.channels=2;wanted.samples=1024;wanted.callback=consume;
    device=SDL_OpenAudioDevice(NULL,0,&wanted,&obtained,0);
    if(!device)goto failure;
    if(obtained.freq!=48000 || obtained.format!=AUDIO_S16LSB || obtained.channels!=2)goto failure;
    rf_xbox_audio_diagnostic[0]=1;return RF_OK;
failure:
    rf_xbox_audio_close();rf_xbox_audio_diagnostic[0]=(uint32_t)RF_IO;return RF_IO;
}
void rf_xbox_audio_submit(void *context,const int16_t *stereo,uint32_t frames)
{
    uint32_t i,write_at;int start=0;(void)context;
    if(!device || !stereo)return;
    SDL_LockAudioDevice(device);
    if(frames>QUEUE_FRAMES-queued)rf_xbox_audio_diagnostic[3]+=frames;
    else {
        write_at=(read_at+queued)%QUEUE_FRAMES;
        for(i=0;i<frames;i++) {
            memcpy(queue+write_at*2,stereo+i*2,4);write_at=(write_at+1)%QUEUE_FRAMES;
        }
        queued+=frames;rf_xbox_audio_diagnostic[1]+=frames;
        if(!started && queued){started=1;start=1;}
    }
    SDL_UnlockAudioDevice(device);
    if(start)SDL_PauseAudioDevice(device,0);
}
void rf_xbox_audio_close(void)
{
    rf_xbox_audio_close_phase=1;
    if(device) {
        /* Bounded drain; no lock held while waiting for the SDL consumer. */
        uint32_t begin=SDL_GetTicks(),remaining;
        do {
            SDL_LockAudioDevice(device);remaining=queued;SDL_UnlockAudioDevice(device);
            if(!remaining || SDL_GetTicks()-begin>=250)break;
            SDL_Delay(1);
        } while(started);
        /* Two native 1024-frame buffers plus the SDL pending buffer. */
        if(started && !remaining)SDL_Delay(64);
        rf_xbox_audio_close_phase=2;SDL_CloseAudioDevice(device);device=0;
    }
    rf_xbox_audio_close_phase=3;
    if(initialized){SDL_QuitSubSystem(SDL_INIT_AUDIO);initialized=0;}
    if(rf_xbox_audio_diagnostic[0]==1)rf_xbox_audio_diagnostic[0]=0;
    rf_xbox_audio_close_phase=4;
}
