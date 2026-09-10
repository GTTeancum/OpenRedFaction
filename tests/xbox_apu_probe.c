/* Isolated NXDK backend evaluation; not linked into the game. */
#include "rf/audio.h"
#include "audio_internal.h"
#include "audio.h"
#include <windows.h>
#include <hal/video.h>
volatile uint32_t rf_apu_probe[14]={0x52464150u};
extern void *g_hw_ac97_buffer; /* Exposed only by the isolated build adapter. */
uint16_t rf_apu_dma_snapshot[4096];
volatile uint32_t rf_apu_adapter[5]; /* full peak, rejected, reused plays, restored pages, status */
static uint8_t wav[65536];
static nxAudioVoice voice;
volatile uint32_t rf_apu_lifecycle[6]; /* replay ms, stopped voices, recreated, second init, restored pages, status */
static int wait_stopped(uint32_t timeout)
{
    uint32_t begin=GetTickCount();
    while(nxAudioVoiceGetState(&voice)!=NX_STOPPED) {
        if(GetTickCount()-begin>=timeout)return 0;
        Sleep(1);
    }
    return 1;
}
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
    /* Reuse the same retained static buffer after natural completion. */
    rf_apu_probe[1]=10;begin=GetTickCount();
    if(!nxAudioVoiceStart(&voice) || !wait_stopped(6000))goto fail;
    rf_apu_lifecycle[0]=GetTickCount()-begin;
    if(rf_apu_lifecycle[0]<2400 || rf_apu_lifecycle[0]>3200)goto fail;
    for(uint32_t n=0;n<8;n++) {
        if(!nxAudioVoiceStart(&voice))goto fail;
        Sleep(50);
        if(!nxAudioVoiceStop(&voice) || !wait_stopped(1000))goto fail;
        ++rf_apu_lifecycle[1];
        nxAudioVoiceDestroy(&voice);
        if(!nxAudioVoiceCreate(&voice,&format) || !nxAudioBufferSubmit(&voice,&buffer))goto fail;
        ++rf_apu_lifecycle[2];
    }
    nxAudioVoiceDestroy(&voice);rf_apu_probe[1]=6;
    nxAudioShutdown();rf_apu_probe[5]=available();
    if(rf_apu_probe[5]!=rf_apu_probe[3])goto fail;
    rf_apu_probe[1]=11;
    if(!nxAudioInit(&init))goto fail;
    rf_apu_lifecycle[3]=1;
    if(!nxAudioVoiceCreate(&voice,&format) || !nxAudioBufferSubmit(&voice,&buffer) || !nxAudioVoiceStart(&voice))goto fail;
    Sleep(50);
    if(!nxAudioVoiceStop(&voice) || !wait_stopped(1000))goto fail;
    nxAudioVoiceDestroy(&voice);nxAudioShutdown();
    rf_apu_lifecycle[4]=available();
    if(rf_apu_lifecycle[4]!=rf_apu_probe[3])goto fail;
    /* Exercise the production adapter with shared PCM page ownership. */
    rf_apu_probe[1]=12;
    if(rf_xbox_audio_open()!=RF_OK)goto fail;
    for(uint32_t n=0;n<16;n++)rf_xbox_audio_events.play(NULL,0x10000u+n,&pcm);
    Sleep(50);
    rf_xbox_audio_events.poll(NULL);
    rf_apu_adapter[0]=rf_xbox_audio_diagnostic[7];
    if(rf_apu_adapter[0]!=16 || rf_xbox_audio_diagnostic[1]!=16 || !rf_xbox_audio_diagnostic[5])goto fail;
    rf_xbox_audio_events.play(NULL,0x20000u,&pcm);
    rf_apu_adapter[1]=rf_xbox_audio_diagnostic[3];
    if(rf_apu_adapter[1]!=1)goto fail;
    rf_xbox_audio_events.stop(NULL,0x10000u);
    Sleep(100);
    rf_xbox_audio_events.play(NULL,0x30000u,&pcm);
    rf_apu_adapter[2]=rf_xbox_audio_diagnostic[1];
    if(rf_apu_adapter[2]!=17 || rf_xbox_audio_diagnostic[3]!=1)goto fail;
    /* The old logical handle must not stop its replacement. */
    rf_xbox_audio_events.stop(NULL,0x10000u);
    if(rf_xbox_audio_diagnostic[2]!=1)goto fail;
    rf_xbox_audio_events.reset(NULL);
    rf_apu_adapter[3]=available();
    if(rf_apu_adapter[3]!=rf_apu_probe[3] || rf_xbox_audio_diagnostic[11] ||
       rf_xbox_audio_diagnostic[0] || rf_xbox_audio_diagnostic[4]!=1)goto fail;
    rf_apu_probe[1]=9;
    for(;;)Sleep(100);
fail:
    rf_apu_adapter[4]=1;rf_apu_probe[2]=1;rf_apu_lifecycle[5]=1;for(;;)Sleep(100);
}
