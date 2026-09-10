/* Port-owned bounded Windows output; shared integer mixer, device-driven refill. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include "audio.h"
#include "rf/audio.h"
#define BLOCKS 4
#define FRAMES 512
static HWAVEOUT device;
static HANDLE event,worker;
static CRITICAL_SECTION lock;
static int lock_ready;
static volatile LONG stopping;
static WAVEHDR headers[BLOCKS];
static int16_t samples[BLOCKS][FRAMES*2];
static rf_audio_mixer mixer;
static uint32_t handles[RF_AUDIO_VOICES];
unsigned int rf_pc_audio_diagnostic[8];
static DWORD WINAPI refill(void *unused)
{
    unsigned i,j;(void)unused;
    while(!InterlockedCompareExchange(&stopping,0,0)) {
        for(i=0;i<BLOCKS;i++)if(!(headers[i].dwFlags&WHDR_INQUEUE)) {
            EnterCriticalSection(&lock);
            rf_audio_mix(&mixer,samples[i],FRAMES);
            LeaveCriticalSection(&lock);
            for(j=0;j<FRAMES*2;j++)if(samples[i][j])++rf_pc_audio_diagnostic[3];
            if(waveOutWrite(device,headers+i,sizeof(headers[i]))!=MMSYSERR_NOERROR) {
                rf_pc_audio_diagnostic[7]=1;return 1;
            }
            ++rf_pc_audio_diagnostic[1];rf_pc_audio_diagnostic[2]+=FRAMES;
        }
        WaitForSingleObject(event,100);
    }
    return 0;
}
void rf_pc_audio_close(void)
{
    unsigned i;
    InterlockedExchange(&stopping,1);
    if(event)SetEvent(event);
    if(worker){WaitForSingleObject(worker,INFINITE);CloseHandle(worker);worker=NULL;}
    if(device) {
        if(waveOutReset(device)!=MMSYSERR_NOERROR)rf_pc_audio_diagnostic[7]=1;
        for(i=0;i<BLOCKS;i++)if(headers[i].dwFlags&WHDR_PREPARED)
            if(waveOutUnprepareHeader(device,headers+i,sizeof(headers[i]))!=MMSYSERR_NOERROR)rf_pc_audio_diagnostic[7]=1;
        if(waveOutClose(device)!=MMSYSERR_NOERROR)rf_pc_audio_diagnostic[7]=1;
        device=NULL;
    }
    if(event){CloseHandle(event);event=NULL;}
    if(lock_ready){DeleteCriticalSection(&lock);lock_ready=0;}
    rf_audio_mixer_init(&mixer);memset(handles,0,sizeof(handles));rf_pc_audio_diagnostic[0]=0;
}
int rf_pc_audio_open(void)
{
    WAVEFORMATEX format={WAVE_FORMAT_PCM,2,48000,192000,4,16,0};unsigned i;
    if(lock_ready)return RF_RANGE;
    memset(rf_pc_audio_diagnostic,0,sizeof(rf_pc_audio_diagnostic));memset(headers,0,sizeof(headers));
    rf_pc_audio_diagnostic[6]=sizeof(headers)+sizeof(samples)+sizeof(mixer)+sizeof(handles);
    InitializeCriticalSection(&lock);lock_ready=1;rf_audio_mixer_init(&mixer);
    event=CreateEventW(NULL,FALSE,FALSE,NULL);if(!event)goto fail;
    if(waveOutOpen(&device,WAVE_MAPPER,&format,(DWORD_PTR)event,0,CALLBACK_EVENT)!=MMSYSERR_NOERROR)goto fail;
    for(i=0;i<BLOCKS;i++) {
        headers[i].lpData=(LPSTR)samples[i];headers[i].dwBufferLength=sizeof(samples[i]);
        if(waveOutPrepareHeader(device,headers+i,sizeof(headers[i]))!=MMSYSERR_NOERROR)goto fail;
    }
    InterlockedExchange(&stopping,0);worker=CreateThread(NULL,0,refill,NULL,0,NULL);if(!worker)goto fail;
    rf_pc_audio_diagnostic[0]=1;return RF_OK;
fail:
    rf_pc_audio_diagnostic[7]=1;rf_pc_audio_close();return RF_IO;
}
static void play(void *context,uint32_t handle,const rf_wave_pcm *pcm,float left,float right)
{
    uint32_t internal;(void)context;if(!device)return;
    if(!(left>=0 && left<=1 && right>=0 && right<=1)){++rf_pc_audio_diagnostic[5];return;}
    EnterCriticalSection(&lock);
    if(rf_audio_voice_start(&mixer,pcm,(uint32_t)(left*32768),(uint32_t)(right*32768),0,&internal))++rf_pc_audio_diagnostic[5];
    else {handles[internal&0xffff]=handle;++rf_pc_audio_diagnostic[4];}
    LeaveCriticalSection(&lock);
}
static void stop(void *context,uint32_t handle)
{
    unsigned i;(void)context;if(!device)return;EnterCriticalSection(&lock);
    for(i=0;i<RF_AUDIO_VOICES;i++)if(handles[i]==handle && mixer.voices[i].active)
        rf_audio_voice_stop(&mixer,mixer.voices[i].handle);
    LeaveCriticalSection(&lock);
}
static void gain(void *context,uint32_t handle,float left,float right)
{
    unsigned i;(void)context;if(!device)return;
    if(!(left>=0 && left<=1 && right>=0 && right<=1)){++rf_pc_audio_diagnostic[5];return;}
    EnterCriticalSection(&lock);
    for(i=0;i<RF_AUDIO_VOICES;i++)if(handles[i]==handle && mixer.voices[i].active)
        rf_audio_voice_gain(&mixer,mixer.voices[i].handle,(uint32_t)(left*32768),(uint32_t)(right*32768));
    LeaveCriticalSection(&lock);
}
static void reset(void *context){(void)context;rf_pc_audio_close();}
const rf_scene_audio_events rf_pc_audio_events={play,stop,NULL,reset,gain};
