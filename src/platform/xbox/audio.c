/* Port APU adapter; samples are borrowed until reset returns. */
#include "audio.h"
#include "rf/audio.h"
#include <nxaudio.h>
#include <windows.h>
#include <string.h>
#define VOICES 16u
typedef struct audio_slot {nxAudioVoice voice;nxAudioBuffer buffer;uint32_t handle,created;} audio_slot;
static audio_slot slots[VOICES];
static int initialized;
extern void *g_hw_ac97_buffer;
uint32_t rf_xbox_audio_diagnostic[12],rf_xbox_audio_close_phase;
uint16_t rf_xbox_audio_snapshot[4096];
static uint32_t available(void)
{MM_STATISTICS s={0};s.Length=sizeof(s);MmQueryStatistics(&s);return s.AvailablePages;}
static int stopped(audio_slot *slot,uint32_t timeout)
{
    uint32_t begin=GetTickCount();
    while(nxAudioVoiceGetState(&slot->voice)!=NX_STOPPED) {
        if(GetTickCount()-begin>=timeout)return 0;
        Sleep(1);
    }
    return 1;
}
void rf_xbox_audio_close(void)
{
    uint32_t i,j;int drained=1;
    if(!initialized)return;
    rf_xbox_audio_close_phase=1;
    for(i=0;i<VOICES;i++)if(slots[i].created)nxAudioVoiceStop(&slots[i].voice);
    for(i=0;i<VOICES;i++)if(slots[i].created && !stopped(slots+i,1000)){drained=0;break;}
    rf_xbox_audio_close_phase=2;
    if(drained) {
        for(i=0;i<VOICES;i++)if(slots[i].created)nxAudioVoiceDestroy(&slots[i].voice);
        nxAudioShutdown();
    } else {
        /* Stop hardware before releasing PCM on a failed idle notification. */
        rf_xbox_audio_diagnostic[11]=1;nxAudioShutdown();
        for(i=0;i<VOICES;i++)if(slots[i].created)for(j=0;j<2;j++) {
            const nxAudioBuffer *b=slots[i].voice.buffers_hardware[j];
            if(b)MmLockUnlockBufferPages((PVOID)b->buffer,b->size_bytes,TRUE);
        }
    }
    memset(slots,0,sizeof(slots));initialized=0;
    rf_xbox_audio_diagnostic[10]=available();rf_xbox_audio_diagnostic[0]=0;
    ++rf_xbox_audio_diagnostic[4];rf_xbox_audio_close_phase=4;
}
int rf_xbox_audio_open(void)
{
    nxAudioInitParams init={0};
    if(initialized)return RF_RANGE;
    memset(rf_xbox_audio_diagnostic,0,sizeof(rf_xbox_audio_diagnostic));
    memset(rf_xbox_audio_snapshot,0,sizeof(rf_xbox_audio_snapshot));
    rf_xbox_audio_close_phase=0;rf_xbox_audio_diagnostic[6]=sizeof(slots)+sizeof(rf_xbox_audio_snapshot);
    rf_xbox_audio_diagnostic[9]=available();
    if(!nxAudioInit(&init)){rf_xbox_audio_diagnostic[0]=(uint32_t)RF_IO;return RF_IO;}
    initialized=1;rf_xbox_audio_diagnostic[0]=1;rf_xbox_audio_diagnostic[8]=available();return RF_OK;
}
static void play(void *context,uint32_t handle,const rf_wave_pcm *pcm)
{
    uint32_t i;nxAudioFormat format={0};audio_slot *slot;(void)context;
    if(!initialized)return;
    for(i=0;i<VOICES;i++)if(!slots[i].created || nxAudioVoiceGetState(&slots[i].voice)==NX_STOPPED)break;
    if(i==VOICES){++rf_xbox_audio_diagnostic[3];return;}
    slot=slots+i;
    if(slot->created){nxAudioVoiceDestroy(&slot->voice);memset(slot,0,sizeof(*slot));}
    format.sample_rate=pcm->rate;format.channels=(uint8_t)pcm->channels;
    format.bytes_per_sample=(uint8_t)(pcm->bits/8);format.codec=NX_AUDIO_CODEC_PCM;format.type=NX_VOICE_TYPE_2D_STATIC;
    if(!nxAudioVoiceCreate(&slot->voice,&format))goto fail;
    slot->created=1;slot->handle=handle;
    if(!nxAudioBufferInitialize(&slot->buffer,pcm->samples,pcm->bytes) ||
       !nxAudioBufferSubmit(&slot->voice,&slot->buffer) || !nxAudioVoiceStart(&slot->voice))goto fail;
    ++rf_xbox_audio_diagnostic[1];return;
fail:
    ++rf_xbox_audio_diagnostic[3];
    if(slot->created){nxAudioVoiceDestroy(&slot->voice);memset(slot,0,sizeof(*slot));}
}
static void stop(void *context,uint32_t handle)
{
    uint32_t i;(void)context;if(!initialized)return;
    for(i=0;i<VOICES;i++)if(slots[i].created && slots[i].handle==handle) {
        nxAudioVoiceStop(&slots[i].voice);++rf_xbox_audio_diagnostic[2];break;
    }
}
static void poll(void *context)
{
    uint32_t i,active=0,nonzero=0;const volatile int16_t *output=g_hw_ac97_buffer;
    (void)context;if(!initialized)return;
    for(i=0;i<VOICES;i++)if(slots[i].created && nxAudioVoiceGetState(&slots[i].voice)!=NX_STOPPED)++active;
    if(active>rf_xbox_audio_diagnostic[7])rf_xbox_audio_diagnostic[7]=active;
    if(!rf_xbox_audio_diagnostic[5]) {
        for(i=0;i<4096;i++)if(output[i])++nonzero;
        if(nonzero) {
            for(i=0;i<4096;i++)rf_xbox_audio_snapshot[i]=(uint16_t)output[i];
            rf_xbox_audio_diagnostic[5]=nonzero;
        }
    }
}
static void reset(void *context){(void)context;rf_xbox_audio_close();}
const rf_scene_audio_events rf_xbox_audio_events={play,stop,poll,reset};
