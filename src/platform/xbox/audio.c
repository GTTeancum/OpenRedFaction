/* Port APU adapter; samples are borrowed until voice release or reset returns. */
#include "audio.h"
#include "rf/audio.h"
#include "rf/music.h"
#include <nxaudio.h>
#include <windows.h>
#include <string.h>
#define VOICES RF_AUDIO_ORDINARY_SLOTS
typedef struct audio_slot {nxAudioVoice voice;nxAudioBuffer buffer;uint32_t handle,created;} audio_slot;
static audio_slot slots[VOICES];
static int initialized;
static uint32_t audio_paused,paused_slots,paused_music;
uint32_t rf_xbox_audio_pause_diagnostic[8];
#define MUSIC_FRAMES 4096u
/* Exactly 32 KiB of PCM; descriptors and pages remain stable until completion.
 * This voice is independent of the thirty ordinary static SFX/VO slots. */
static int16_t music_pcm[2][MUSIC_FRAMES*2] __attribute__((aligned(4096)));
static struct {
    nxAudioVoice voice;
    nxAudioBuffer buffers[2];
    uint32_t created,revision,next_buffer,started,failed;
} music;
volatile uint32_t rf_xbox_music_diagnostic[12];
extern void *g_hw_ac97_buffer;
uint32_t rf_xbox_audio_diagnostic[12],rf_xbox_audio_close_phase;
uint16_t rf_xbox_audio_snapshot[4096];
static uint32_t available(void)
{MM_STATISTICS s={0};s.Length=sizeof(s);MmQueryStatistics(&s);return s.AvailablePages;}
static int stopped(nxAudioVoice *voice,uint32_t timeout)
{
    uint32_t begin=GetTickCount();
    while(nxAudioVoiceGetState(voice)!=NX_STOPPED) {
        if(GetTickCount()-begin>=timeout)return 0;
        Sleep(1);
    }
    return 1;
}
int rf_xbox_audio_set_paused(uint32_t paused)
{
    uint32_t i;int status=RF_OK;KIRQL irql;
    if(paused>1)return RF_RANGE;
    if(!initialized || paused==audio_paused)return RF_OK;
    /* Pause/Start perform their own nested DPC exclusion and issue PIO only.
     * Keep the state test, operation and ownership mark together: completion
     * must not turn an already-finished one-shot into a new paused borrower. */
    irql=KeRaiseIrqlToDpcLevel();
    if(paused) {
        paused_slots=paused_music=0;
        for(i=0;i<VOICES;i++)if(slots[i].created &&
            nxAudioVoiceGetState(&slots[i].voice)==NX_PLAYING) {
            if(nxAudioVoicePause(&slots[i].voice))paused_slots|=1u<<i;
            else status=RF_IO;
        }
        if(music.created && nxAudioVoiceGetState(&music.voice)==NX_PLAYING) {
            if(nxAudioVoicePause(&music.voice))paused_music=1;
            else status=RF_IO;
        }
        ++rf_xbox_audio_pause_diagnostic[1];
        rf_xbox_audio_pause_diagnostic[3]=0;
        for(i=0;i<VOICES;i++)rf_xbox_audio_pause_diagnostic[3]+=!!(paused_slots&(1u<<i));
        rf_xbox_audio_pause_diagnostic[4]=paused_music;
    } else {
        for(i=0;i<VOICES;i++)if(paused_slots&(1u<<i)) {
            if(slots[i].created && nxAudioVoiceGetState(&slots[i].voice)==NX_PAUSED) {
                if(!nxAudioVoiceStart(&slots[i].voice))status=RF_IO;
            } else ++rf_xbox_audio_pause_diagnostic[5];
        }
        if(paused_music) {
            if(music.created && nxAudioVoiceGetState(&music.voice)==NX_PAUSED) {
                if(music.voice.buffers_hardware[0] || music.voice.buffers_hardware[1]) {
                    if(!nxAudioVoiceStart(&music.voice))status=RF_IO;
                } else {
                    /* A pending completion DPC can finish removing lists that
                     * drained just before hardware accepted Pause. Start
                     * rejects an empty voice. Leave started/revision/decoder
                     * intact for music_poll's existing drained-queue path. */
                    ++rf_xbox_audio_pause_diagnostic[6];
                }
            } else ++rf_xbox_audio_pause_diagnostic[5];
        }
        paused_slots=paused_music=0;++rf_xbox_audio_pause_diagnostic[2];
    }
    audio_paused=paused;rf_xbox_audio_pause_diagnostic[0]=paused;
    KfLowerIrql(irql);
    if(status)++rf_xbox_audio_pause_diagnostic[7];return status;
}
int rf_xbox_audio_release_voice(uint32_t handle)
{
    uint32_t i;
    if(!initialized)return RF_NOT_FOUND;
    for(i=0;i<VOICES;i++)if(slots[i].created && slots[i].handle==handle) {
        if(!nxAudioVoiceStop(&slots[i].voice) || !stopped(&slots[i].voice,1000))return RF_IO;
        nxAudioVoiceDestroy(&slots[i].voice);memset(slots+i,0,sizeof(slots[i]));
        paused_slots&=~(1u<<i);return RF_OK;
    }
    return RF_NOT_FOUND;
}
int rf_xbox_audio_release_idle_sample(const uint8_t *samples)
{
    uint32_t i;
    if(!samples)return RF_RANGE;
    if(!initialized)return RF_OK;
    for(i=0;i<VOICES;i++)if(slots[i].created && slots[i].buffer.buffer==samples &&
        (slots[i].voice.looping || nxAudioVoiceGetState(&slots[i].voice)!=NX_STOPPED))return RF_RANGE;
    for(i=0;i<VOICES;i++)if(slots[i].created && slots[i].buffer.buffer==samples) {
        nxAudioVoiceDestroy(&slots[i].voice);memset(slots+i,0,sizeof(slots[i]));
        paused_slots&=~(1u<<i);
    }
    return RF_OK;
}
void rf_xbox_audio_close(void)
{
    uint32_t i,j;int drained=1;
    if(!initialized)return;
    rf_xbox_audio_close_phase=1;
    for(i=0;i<VOICES;i++)if(slots[i].created)nxAudioVoiceStop(&slots[i].voice);
    if(music.created)nxAudioVoiceStop(&music.voice);
    for(i=0;i<VOICES;i++)if(slots[i].created && !stopped(&slots[i].voice,1000)){drained=0;break;}
    if(music.created && !stopped(&music.voice,1000))drained=0;
    rf_xbox_audio_close_phase=2;
    if(drained) {
        for(i=0;i<VOICES;i++)if(slots[i].created)nxAudioVoiceDestroy(&slots[i].voice);
        if(music.created)nxAudioVoiceDestroy(&music.voice);
        nxAudioShutdown();
    } else {
        /* Stop hardware before releasing PCM on a failed idle notification. */
        rf_xbox_audio_diagnostic[11]=1;nxAudioShutdown();
        for(i=0;i<VOICES;i++)if(slots[i].created)for(j=0;j<2;j++) {
            const nxAudioBuffer *b=slots[i].voice.buffers_hardware[j];
            if(b)MmLockUnlockBufferPages((PVOID)b->buffer,b->size_bytes,TRUE);
        }
        if(music.created)for(j=0;j<2;j++) {
            const nxAudioBuffer *b=music.voice.buffers_hardware[j];
            if(b)MmLockUnlockBufferPages((PVOID)b->buffer,b->size_bytes,TRUE);
        }
    }
    memset(slots,0,sizeof(slots));memset(&music,0,sizeof(music));initialized=0;
    audio_paused=paused_slots=paused_music=0;rf_xbox_audio_pause_diagnostic[0]=0;
    rf_xbox_music_diagnostic[0]=0;rf_xbox_music_diagnostic[9]=0;
    rf_xbox_audio_diagnostic[10]=available();rf_xbox_audio_diagnostic[0]=0;
    ++rf_xbox_audio_diagnostic[4];rf_xbox_audio_close_phase=4;
}
int rf_xbox_audio_open(void)
{
    nxAudioInitParams init={0};
    if(initialized)return RF_RANGE;
    audio_paused=paused_slots=paused_music=0;
    memset(rf_xbox_audio_pause_diagnostic,0,sizeof(rf_xbox_audio_pause_diagnostic));
    memset(rf_xbox_audio_diagnostic,0,sizeof(rf_xbox_audio_diagnostic));
    memset(rf_xbox_audio_snapshot,0,sizeof(rf_xbox_audio_snapshot));
    memset(&music,0,sizeof(music));
    for(uint32_t i=0;i<12;i++)rf_xbox_music_diagnostic[i]=0;
    rf_xbox_music_diagnostic[8]=sizeof(music)+sizeof(music_pcm);
    rf_xbox_audio_close_phase=0;rf_xbox_audio_diagnostic[6]=sizeof(slots)+sizeof(rf_xbox_audio_snapshot)+sizeof(music)+sizeof(music_pcm);
    rf_xbox_audio_diagnostic[9]=available();
    if(!nxAudioInit(&init)){rf_xbox_audio_diagnostic[0]=(uint32_t)RF_IO;return RF_IO;}
    initialized=1;rf_xbox_audio_diagnostic[0]=1;rf_xbox_audio_diagnostic[8]=available();return RF_OK;
}
/* The backend invokes this after unpinning/removing a completed descriptor at
 * DISPATCH_LEVEL. No archive reads, decoding, waits or voice destruction here. */
static void music_completed(nxAudioVoice *voice,void *context)
{(void)voice;(void)context;++rf_xbox_music_diagnostic[3];}
static uint32_t music_borrowed(uint32_t *next_busy)
{
    uint32_t i,count=0;const nxAudioBuffer *next=music.buffers+music.next_buffer;
    KIRQL irql=KeRaiseIrqlToDpcLevel();
    *next_busy=0;
    for(i=0;i<2;i++)if(music.voice.buffers_hardware[i]) {
        ++count;
        if(music.voice.buffers_hardware[i]==next)*next_busy=1;
    }
    KfLowerIrql(irql);return count;
}
static int music_release(void)
{
    if(!music.created)return RF_OK;
    if(!nxAudioVoiceStop(&music.voice) || !stopped(&music.voice,1000))return RF_IO;
    nxAudioVoiceDestroy(&music.voice);music.created=music.started=0;paused_music=0;
    rf_xbox_music_diagnostic[0]=0;rf_xbox_music_diagnostic[9]=0;
    return RF_OK;
}
static int music_create(void)
{
    nxAudioFormat format={0};
    format.sample_rate=48000;format.channels=2;format.bytes_per_sample=2;
    format.codec=NX_AUDIO_CODEC_PCM;format.type=NX_VOICE_TYPE_2D_STREAM;
    if(!nxAudioVoiceCreate(&music.voice,&format))return RF_IO;
    music.created=1;
    nxAudioBufferSetCallback(&music.voice,music_completed,NULL);return RF_OK;
}
static int music_queue(const nxAudioBuffer *buffer)
{
    uint32_t busy;int status;KIRQL irql=KeRaiseIrqlToDpcLevel();
    /* Decoding may block on archive I/O after the earlier borrower check.
     * If the last list drained meanwhile, Queue's first-free A/B selection
     * need not match the hardware's next list. Keep this check and Queue in
     * one DPC exclusion; the backend explicitly supports Queue at this IRQL. */
    if(music.started && !music_borrowed(&busy)) {
        KfLowerIrql(irql);
        ++rf_xbox_music_diagnostic[4];
        /* Wait/recreate only at the caller's ordinary IRQL. The decoded PCM
         * and its descriptor stay intact; do not reset the decoder or index. */
        status=music_release();if(status)return status;
        status=music_create();if(status)return status;
        irql=KeRaiseIrqlToDpcLevel();
    }
    status=nxAudioBufferQueue(&music.voice,buffer)?RF_OK:RF_IO;
    KfLowerIrql(irql);return status;
}
static int music_poll(void *context,rf_music_stream *stream,uint32_t revision)
{
    uint32_t busy,queued,attempt;int status;(void)context;
    if(!initialized)return RF_NOT_FOUND;
    if(audio_paused)return RF_OK; /* Do not decode, refill, retire or restart. */
    if(revision!=music.revision) {
        music.revision=revision;music.failed=0;rf_xbox_music_diagnostic[10]=revision;
        status=music_release();if(status)goto fail;
        music.next_buffer=0;
    }
    if(music.failed)return RF_OK; /* Report a failed revision once. */
    if(music.created && music.started && stream->active && !music_borrowed(&busy)) {
        /* After a long main-thread stall both SSL lists may be empty. Restart
         * the voice so the hardware's next A/B list and our FIFO agree. */
        ++rf_xbox_music_diagnostic[4];
        status=music_release();if(status)goto fail;
    }
    if(!music.created) {
        if(!stream->active)return RF_OK;
        status=music_create();if(status)goto fail;
        music.next_buffer=0;
    }
    /* Queue only into descriptors whose previous PCM borrower has completed.
     * The DPC can only remove borrowers; it cannot acquire a free buffer while
     * this main-thread decoder is writing it. Preserve FIFO refill order. */
    for(attempt=0;attempt<2 && stream->active;attempt++) {
        int16_t *pcm;uint32_t i,nonzero=0;
        queued=music_borrowed(&busy);if(queued==2 || busy)break;
        pcm=music_pcm[music.next_buffer];memset(pcm,0,sizeof(music_pcm[0]));
        status=rf_music_mix(stream,pcm,MUSIC_FRAMES);if(status)goto fail;
        for(i=0;i<MUSIC_FRAMES*2;i++)nonzero+=pcm[i]!=0;
        if(!nxAudioBufferInitialize(music.buffers+music.next_buffer,pcm,sizeof(music_pcm[0]))){status=RF_IO;goto fail;}
        status=music_queue(music.buffers+music.next_buffer);if(status)goto fail;
        ++rf_xbox_music_diagnostic[2];rf_xbox_music_diagnostic[6]+=MUSIC_FRAMES;
        rf_xbox_music_diagnostic[7]+=nonzero;music.next_buffer^=1;
    }
    queued=music_borrowed(&busy);rf_xbox_music_diagnostic[9]=queued;
    if(queued && !music.started) {
        if(!nxAudioVoiceStart(&music.voice)){status=RF_IO;goto fail;}
        music.started=1;++rf_xbox_music_diagnostic[1];
    } else if(!queued && !stream->active) {
        status=music_release();if(status)goto fail;
    }
    rf_xbox_music_diagnostic[0]=music.created;
    rf_xbox_music_diagnostic[10]=revision;return RF_OK;
fail:
    ++rf_xbox_music_diagnostic[5];rf_xbox_music_diagnostic[11]=(uint32_t)status;
    /* A timeout retains the voice and immutable pages until full audio reset.
     * Never recycle a descriptor while the hardware may still read it. */
    (void)music_release();music.failed=1;return status;
}
static int32_t allocation_status(void *context,uint32_t slot,uint32_t *bits)
{(void)context;*bits=nxAudioVoiceGetState(&slots[slot].voice)==NX_STOPPED?0u:1u;return 0;}
static void allocation_release(void *context,uint32_t index)
{(void)context;nxAudioVoiceDestroy(&slots[index].voice);memset(slots+index,0,sizeof(*slots));paused_slots&=~(1u<<index);}
static int play_mode(void *context,uint32_t handle,const rf_wave_pcm *pcm,float left,float right,uint32_t looping)
{
    uint32_t i;int32_t selected;nxAudioFormat format={0};audio_slot *slot;rf_audio_allocation_slot facts[VOICES];(void)context;
    static const rf_audio_allocation_backend backend={allocation_status,allocation_release};
    if(!initialized || audio_paused)return RF_NOT_FOUND;
    if(!(left>=0 && left<=1 && right>=0 && right<=1) || looping>1 || !pcm || !pcm->samples || !pcm->frames ||
       !pcm->rate || pcm->rate>192000 || (pcm->channels!=1 && pcm->channels!=2) || (pcm->bits!=8 && pcm->bits!=16) ||
       (uint64_t)pcm->frames*pcm->channels*(pcm->bits/8)!=pcm->bytes){++rf_xbox_audio_diagnostic[3];return RF_RANGE;}
    for(i=0;i<VOICES;i++) {facts[i].present=slots[i].created;facts[i].flags=slots[i].created && slots[i].voice.looping?1u:0u;}
    selected=rf_audio_select_ordinary(facts,&backend,NULL);
    if(selected<0){++rf_xbox_audio_diagnostic[3];return RF_RANGE;}i=(uint32_t)selected;
    slot=slots+i;
    if(slot->created){nxAudioVoiceDestroy(&slot->voice);memset(slot,0,sizeof(*slot));}
    format.sample_rate=pcm->rate;format.channels=(uint8_t)pcm->channels;
    format.bytes_per_sample=(uint8_t)(pcm->bits/8);format.codec=NX_AUDIO_CODEC_PCM;format.type=NX_VOICE_TYPE_2D_STATIC;
    if(!nxAudioVoiceCreate(&slot->voice,&format))goto fail;
    slot->created=1;slot->handle=handle;
    if(!nxAudioBufferInitialize(&slot->buffer,pcm->samples,pcm->bytes) ||
       !nxAudioBufferSubmit(&slot->voice,&slot->buffer) ||
       !nxAudioVoiceSetLooping(&slot->voice,looping!=0) ||
       !nxAudioVoiceSetChannelGain(&slot->voice,left,right,0,0,0,0) || !nxAudioVoiceStart(&slot->voice))goto fail;
    ++rf_xbox_audio_diagnostic[1];return RF_OK;
fail:
    ++rf_xbox_audio_diagnostic[3];
    if(slot->created){nxAudioVoiceDestroy(&slot->voice);memset(slot,0,sizeof(*slot));}
    return RF_IO;
}
static void play(void *context,uint32_t handle,const rf_wave_pcm *pcm,float left,float right)
{(void)play_mode(context,handle,pcm,left,right,0);}
static void stop(void *context,uint32_t handle)
{
    int status;(void)context;
    status=rf_xbox_audio_release_voice(handle);
    if(status==RF_OK)++rf_xbox_audio_diagnostic[2];
    else if(status!=RF_NOT_FOUND)++rf_xbox_audio_diagnostic[3];
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
static void gain(void *context,uint32_t handle,float left,float right)
{
    uint32_t i;(void)context;if(!initialized)return;
    if(!(left>=0 && left<=1 && right>=0 && right<=1)){++rf_xbox_audio_diagnostic[3];return;}
    for(i=0;i<VOICES;i++)if(slots[i].created && slots[i].handle==handle) {
        if(!nxAudioVoiceSetChannelGain(&slots[i].voice,left,right,0,0,0,0))++rf_xbox_audio_diagnostic[3];
        break;
    }
}
static uint32_t playing(void *context,uint32_t handle)
{
    uint32_t i;(void)context;
    if(!initialized)return 0;
    for(i=0;i<VOICES;i++)if(slots[i].created && slots[i].handle==handle)
        return nxAudioVoiceGetState(&slots[i].voice)!=NX_STOPPED;
    return 0;
}
static void reset(void *context){(void)context;rf_xbox_audio_close();}
static int release_idle_sample(void *context,const uint8_t *samples)
{(void)context;return rf_xbox_audio_release_idle_sample(samples);}
const rf_scene_audio_events rf_xbox_audio_events={play,stop,poll,reset,gain,play_mode,release_idle_sample,playing,music_poll};
