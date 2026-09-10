/* Isolated NXDK backend evaluation; not linked into the game. */
#include "rf/audio.h"
#include "audio_internal.h"
#include <windows.h>
#include <hal/video.h>
volatile uint32_t rf_apu_probe[14]={0x52464150u};
extern void *g_hw_ac97_buffer; /* Exposed only by the isolated build adapter. */
uint16_t rf_apu_dma_snapshot[4096];
static uint8_t wav[65536];
static nxAudioVoice voice;
static uint32_t available(void)
{
    MM_STATISTICS s={0};s.Length=sizeof(s);MmQueryStatistics(&s);return s.AvailablePages;
}
int main(void)
{
    FILE *f;size_t bytes;rf_wave_pcm pcm;nxAudioBuffer buffer;
    nxAudioInitParams init={0};nxAudioFormat format={0};uint32_t begin;
    XVideoSetMode(640,480,32,REFRESH_DEFAULT);
    rf_apu_probe[1]=1;f=fopen("D:\\door.wav","rb");if(!f)goto fail;
    bytes=fread(wav,1,sizeof(wav),f);fclose(f);
    if(rf_wave_pcm_parse(wav,(uint32_t)bytes,&pcm))goto fail;
    rf_apu_probe[6]=pcm.rate;rf_apu_probe[7]=pcm.frames;
    rf_apu_probe[9]=HW_VOICE_ARRAY_SIZE+HW_VOICE_SSL_ARRAY_SIZE+HW_VOICE_NOTIFIERS_ARRAY_SIZE+
        MCPX_HW_GP_SGE_ARRAY_SIZE+HW_VOICE_SGE_ARRAY_SIZE+GP_FIFO_OUTPUT_SIZE+
        AC97_BUFFER_SIZE+MAX_DSP_PAYLOAD+HW_HRTF_TARGET_ARRAY_SIZE+HW_HRTF_CURRENT_ARRAY_SIZE;
    rf_apu_probe[3]=available();rf_apu_probe[1]=2;
    if(!nxAudioInit(&init))goto fail;
    rf_apu_probe[4]=available();rf_apu_probe[1]=3;
    format.sample_rate=pcm.rate;format.channels=(uint8_t)pcm.channels;
    format.bytes_per_sample=(uint8_t)(pcm.bits/8);format.codec=NX_AUDIO_CODEC_PCM;
    format.type=NX_VOICE_TYPE_2D_STATIC;
    if(!nxAudioVoiceCreate(&voice,&format))goto fail;
    if(!nxAudioBufferInitialize(&buffer,pcm.samples,pcm.bytes) || !nxAudioBufferSubmit(&voice,&buffer))goto fail;
    rf_apu_probe[1]=4;
    if(!nxAudioVoiceStart(&voice))goto fail;
    begin=GetTickCount();rf_apu_probe[10]=begin;
    do {
        const volatile int16_t *output=g_hw_ac97_buffer;uint32_t i,nonzero=0;
        for(i=0;i<4096;i++)if(output[i])++nonzero;
        if(nonzero && !rf_apu_probe[12]) {
            for(i=0;i<4096;i++)rf_apu_dma_snapshot[i]=(uint16_t)output[i];
            rf_apu_probe[12]=nonzero;
        }
        ++rf_apu_probe[13];
        rf_apu_probe[8]=nxAudioVoiceGetState(&voice);
        if(rf_apu_probe[8]==NX_STOPPED)break;
        Sleep(10);
    } while(GetTickCount()-begin<6000);
    rf_apu_probe[11]=GetTickCount();rf_apu_probe[1]=5;
    if(rf_apu_probe[8]!=NX_STOPPED)goto fail;
    nxAudioVoiceDestroy(&voice);rf_apu_probe[1]=6;
    nxAudioShutdown();rf_apu_probe[5]=available();rf_apu_probe[1]=9;
    for(;;)Sleep(100);
fail:
    rf_apu_probe[2]=1;for(;;)Sleep(100);
}
