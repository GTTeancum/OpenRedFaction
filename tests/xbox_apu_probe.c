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
uint32_t rf_apu_fail_allocation,rf_apu_allocation_index;
volatile uint32_t rf_apu_allocation_failures;
volatile uint32_t rf_apu_channel_counts[6]; /* left-only, right-only, mute: interleaved L/R nonzero counts */
static int16_t calibration_pcm[48000];
volatile uint32_t rf_apu_gain_sums[10],rf_apu_muted_start;
volatile uint32_t rf_apu_residency[5]; /* cycles, loaded bytes, unloaded bytes, PCM hash, DSP cycles */
volatile uint32_t rf_apu_single_release;
uint32_t rf_apu_fail_stopped;
volatile uint32_t rf_apu_release_retry;
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
    for(uint32_t phase=0;phase<3;phase++) {
        if(!nxAudioVoiceSetChannelGain(&voice,phase==0?1:0,phase==1?1:0,0,0,0,0))goto fail;
        Sleep(150); /* More than three complete 8192-byte stereo DMA rings. */
        const volatile int16_t *output=g_hw_ac97_buffer;
        for(uint32_t n=0;n<4096;n++)if(output[n])++rf_apu_channel_counts[phase*2+(n&1)];
        if(nxAudioVoiceGetState(&voice)==NX_STOPPED)goto fail;
    }
    if(!rf_apu_channel_counts[0] || rf_apu_channel_counts[1] || rf_apu_channel_counts[2] ||
       !rf_apu_channel_counts[3] || rf_apu_channel_counts[4] || rf_apu_channel_counts[5])goto fail;
    if(!nxAudioVoiceStop(&voice) || !wait_stopped(1000))goto fail;
    nxAudioVoiceDestroy(&voice);nxAudioShutdown();
    rf_apu_lifecycle[4]=available();
    if(rf_apu_lifecycle[4]!=rf_apu_probe[3])goto fail;
    /* Every partial-init allocation boundary must release all earlier pages. */
    rf_apu_probe[1]=13;
    for(uint32_t n=1;n<=10;n++) {
        rf_apu_fail_allocation=n;rf_apu_allocation_index=0;
        if(rf_xbox_audio_open()!=RF_IO || rf_apu_allocation_index!=n)goto fail;
        rf_xbox_audio_close();
        if(available()!=rf_apu_probe[3])goto fail;
        ++rf_apu_allocation_failures;
    }
    rf_apu_fail_allocation=0;
    /* Exercise the production adapter with shared PCM page ownership. */
    rf_apu_probe[1]=12;
    if(rf_xbox_audio_open()!=RF_OK)goto fail;
    for(uint32_t n=0;n<16;n++)rf_xbox_audio_events.play(NULL,0x10000u+n,&pcm,1,1);
    Sleep(50);
    rf_xbox_audio_events.poll(NULL);
    rf_apu_adapter[0]=rf_xbox_audio_diagnostic[7];
    if(rf_apu_adapter[0]!=16 || rf_xbox_audio_diagnostic[1]!=16 || !rf_xbox_audio_diagnostic[5])goto fail;
    rf_xbox_audio_events.play(NULL,0x20000u,&pcm,1,1);
    rf_apu_adapter[1]=rf_xbox_audio_diagnostic[3];
    if(rf_apu_adapter[1]!=1)goto fail;
    rf_xbox_audio_events.stop(NULL,0x10000u);
    Sleep(100);
    rf_xbox_audio_events.play(NULL,0x30000u,&pcm,1,1);
    rf_apu_adapter[2]=rf_xbox_audio_diagnostic[1];
    if(rf_apu_adapter[2]!=17 || rf_xbox_audio_diagnostic[3]!=1)goto fail;
    /* The old logical handle must not stop its replacement. */
    rf_xbox_audio_events.stop(NULL,0x10000u);
    if(rf_xbox_audio_diagnostic[2]!=1)goto fail;
    rf_xbox_audio_events.reset(NULL);
    rf_apu_adapter[3]=available();
    if(rf_apu_adapter[3]!=rf_apu_probe[3] || rf_xbox_audio_diagnostic[11] ||
       rf_xbox_audio_diagnostic[0] || rf_xbox_audio_diagnostic[4]!=1)goto fail;
    /* Periodic synthetic PCM makes equal-size DMA ring energies comparable. */
    rf_apu_probe[1]=14;
    for(uint32_t n=0;n<48000;n++)calibration_pcm[n]=(n&32)?8192:-8192;
    rf_wave_pcm calibration={(const uint8_t *)calibration_pcm,sizeof(calibration_pcm),48000,48000,1,16};
    if(rf_xbox_audio_open()!=RF_OK)goto fail;
    rf_xbox_audio_events.play(NULL,0x50000u,&calibration,1,1);
    for(uint32_t phase=0;phase<5;phase++) {
        static const int32_t settings[5][2]={{0,0},{-600,0},{0,1000},{0,-1000},{-2000,0}};
        float gains[2];

        if(rf_audio_device_gains(settings[phase][0],settings[phase][1],gains))goto fail;
        rf_xbox_audio_events.gain(NULL,0x50000u,gains[0],gains[1]);
        Sleep(150);
        const volatile int16_t *output=g_hw_ac97_buffer;
        for(uint32_t n=0;n<4096;n++) {
            int32_t sample=output[n];
            rf_apu_gain_sums[phase*2+(n&1)]+=(uint32_t)(sample<0?-sample:sample);
        }

    }

    rf_xbox_audio_events.reset(NULL);
    if(rf_xbox_audio_diagnostic[1]!=1 || rf_xbox_audio_diagnostic[3] || rf_xbox_audio_diagnostic[11] || available()!=rf_apu_probe[3])goto fail;
    if(rf_xbox_audio_open()!=RF_OK)goto fail;
    rf_xbox_audio_events.play(NULL,0x60000u,&calibration,0,0);Sleep(150);
    {const volatile int16_t *output=g_hw_ac97_buffer;
     for(uint32_t n=0;n<4096;n++)if(output[n])goto fail;}
    rf_xbox_audio_events.reset(NULL);
    if(rf_xbox_audio_diagnostic[1]!=1 || rf_xbox_audio_diagnostic[3] || rf_xbox_audio_diagnostic[11] || available()!=rf_apu_probe[3])goto fail;
    rf_apu_muted_start=1;
    rf_apu_probe[1]=15;
    {rf_vpp archive;rf_audio_bank bank={0};uint32_t index;
     uint32_t budget=(uint32_t)(sizeof(bank)+sizeof(rf_audio_sample)+bytes);
     if(rf_vpp_open(&archive,"D:\\bank.vpp") || rf_audio_bank_open(&archive,1,budget-(uint32_t)bytes,&bank) ||
        rf_audio_bank_declare(&bank,"DoorOpen_07.wav",5,.5f,1,&index) ||
        rf_audio_bank_sample(&bank,index) || bank.bytes!=budget-bytes ||
        rf_audio_bank_reload(&bank,&archive,index)!=RF_RANGE)goto fail;
     bank.budget=budget;
     if(rf_audio_bank_reload(&bank,&archive,index))goto fail;
     rf_audio_parameters parameters=*rf_audio_bank_parameters(&bank,index);
     rf_apu_residency[1]=bank.bytes;
     for(uint32_t cycle=0;cycle<3;cycle++) {
        const rf_wave_pcm *resident=rf_audio_bank_sample(&bank,index);uint32_t hash=2166136261u;
        if(!resident)goto fail;
        for(uint32_t n=0;n<resident->bytes;n++)hash=(hash^resident->samples[n])*16777619u;
        if(cycle && hash!=rf_apu_residency[3])goto fail;
        rf_apu_residency[3]=hash;rf_vpp_close(&archive);
        if(rf_xbox_audio_open()!=RF_OK)goto fail;
        rf_xbox_audio_events.play(NULL,0x70000u+cycle,resident,.5f,0);
        rf_xbox_audio_events.play(NULL,0x80000u+cycle,&calibration,0,.5f);Sleep(150);
        rf_xbox_audio_events.poll(NULL);
        if(rf_xbox_audio_diagnostic[1]!=2 || !rf_xbox_audio_diagnostic[5] || rf_xbox_audio_diagnostic[3])goto fail;
        ++rf_apu_residency[4];
        /* Only release the bank borrower; the independent right-channel voice continues. */
        rf_apu_fail_stopped=1;
        if(rf_xbox_audio_release_voice(0x70000u+cycle)!=RF_IO ||
           rf_audio_bank_sample(&bank,index)!=resident || bank.bytes!=budget)goto fail;
        rf_apu_fail_stopped=0;
        /* A successful retry requires the failed call to retain the logical slot. */
        if(rf_xbox_audio_release_voice(0x70000u+cycle) ||
           rf_xbox_audio_release_voice(0x70000u+cycle)!=RF_NOT_FOUND ||
           rf_xbox_audio_release_voice(0xdeadbeefu)!=RF_NOT_FOUND)goto fail;
        ++rf_apu_release_retry;
        if(rf_audio_bank_unload(&bank,index) ||
           rf_audio_bank_unload(&bank,index) || bank.bytes!=budget-bytes || rf_audio_bank_sample(&bank,index) ||
           memcmp(&parameters,rf_audio_bank_parameters(&bank,index),sizeof(parameters)))goto fail;
        Sleep(150);
        {const volatile int16_t *output=g_hw_ac97_buffer;uint32_t right=0;
         for(uint32_t n=0;n<4096;n+=2){if(output[n])goto fail;if(output[n+1])++right;}
         if(!right || rf_xbox_audio_diagnostic[4] || rf_xbox_audio_diagnostic[0]!=1)goto fail;}
        ++rf_apu_single_release;
        rf_xbox_audio_events.reset(NULL);
        if(rf_xbox_audio_diagnostic[11])goto fail;
        rf_apu_residency[2]=bank.bytes;++rf_apu_residency[0];
        if(cycle<2) {
            if(rf_vpp_open(&archive,"D:\\bank.vpp"))goto fail;
            bank.budget=budget-1;
            if(rf_audio_bank_reload(&bank,&archive,index)!=RF_RANGE || bank.bytes!=budget-bytes)goto fail;
            bank.budget=budget;
            if(rf_audio_bank_reload(&bank,&archive,index) || bank.bytes!=budget)goto fail;
        }
     }
     rf_audio_bank_close(&bank);}
    rf_apu_probe[1]=9;
    for(;;)Sleep(100);
fail:
    rf_apu_adapter[4]=1;rf_apu_probe[2]=1;rf_apu_lifecycle[5]=1;for(;;)Sleep(100);
}
