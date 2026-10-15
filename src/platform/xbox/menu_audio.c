/* Cold frontend adapter; source-only proposal. No original assets embedded.
 * sounds.tbl rows40..43/46..49 name these classes; menu_select gain is0.8.
 * Xbox accepted-edge hooks are a bounded port adaptation. Actual animated
 * sweep behavior is not reconstructed by this file. */
#include "menu_audio.h"
#include "audio.h"
#include "rf/audio.h"
#include "rf/music.h"
#include <stdlib.h>
#include <string.h>

/* The existing scene PCM bank cap, not an increase and never simultaneous
 * with that bank. Include menu owner/diagnostics plus bounded call scratch.
 * The APU's pre-existing 30 ordinary slots and 32KiB music queue are unchanged. */
#define MENU_AUDIO_BUDGET (1280u*1024u)
#define MENU_AUDIO_SAMPLE_LIMIT (128u*1024u)
#define MENU_AUDIO_SCRATCH_RESERVE (16u*1024u)
enum {MENU_AUDIO_IDLE,MENU_AUDIO_OWNED,MENU_AUDIO_BLOCKED};
typedef struct menu_sample {void *storage;rf_wave_pcm pcm;uint32_t bytes;} menu_sample;
static struct {
    uint32_t phase,backend_owned,bytes,next_handle,music_failed;
    rf_vpp overlay,stock_audio,stock_music;
    rf_music_stream stream;
    menu_sample samples[RF_XBOX_MENU_SOUND_COUNT];
} menu;
uint32_t rf_xbox_menu_audio_diagnostic[20];
static const char *const names[RF_XBOX_MENU_SOUND_COUNT]={
    "menu_highlight.wav","menu_select.wav","panel_highlight.wav","panel_button_click.wav",
    "panel_scroll_up.wav","panel_scroll_down.wav","sweep_in.wav","sweep_out.wav"
};

static int resolve(rf_vpp **source,rf_vpp_entry *entry,const char *name,
    rf_vpp *stock,const char *stock_path,uint32_t bit)
{
    int status;
    if(menu.overlay.stream){
        status=rf_vpp_find(&menu.overlay,name,entry);
        if(!status){*source=&menu.overlay;rf_xbox_menu_audio_diagnostic[10]|=bit;return RF_OK;}
        if(status!=RF_NOT_FOUND)return status;
    }
    if(!stock->stream){status=rf_vpp_open_checked(stock,stock_path);if(status)return status;}
    status=rf_vpp_find(stock,name,entry);
    if(!status){*source=stock;rf_xbox_menu_audio_diagnostic[11]|=bit;}
    return status;
}
static int sample_load(uint32_t index,const char *stock_path)
{
    rf_vpp *source=NULL;rf_vpp_entry entry;rf_wave_pcm pcm={0};void *storage;int status;
    status=resolve(&source,&entry,names[index],&menu.stock_audio,stock_path,1u<<index);
    if(status)return status;
    if(!entry.size)return RF_FORMAT;
    if(entry.size>MENU_AUDIO_SAMPLE_LIMIT || (uint64_t)menu.bytes+entry.size>MENU_AUDIO_BUDGET)return RF_RANGE;
    storage=malloc(entry.size);if(!storage)return RF_RANGE;
    status=rf_vpp_read(source,&entry,0,storage,entry.size);
    if(!status)status=rf_wave_pcm_parse(storage,entry.size,&pcm);
    if(!status && (!pcm.frames || !pcm.rate || pcm.rate>192000))status=RF_FORMAT;
    if(status){free(storage);return status;}
    menu.samples[index].storage=storage;menu.samples[index].pcm=pcm;menu.samples[index].bytes=entry.size;
    menu.bytes+=entry.size;rf_xbox_menu_audio_diagnostic[5]+=entry.size;
    rf_xbox_menu_audio_diagnostic[6]=menu.bytes;return RF_OK;
}
int rf_xbox_menu_audio_idle_status(void)
{
    uint32_t i;
    if(menu.phase==MENU_AUDIO_BLOCKED)return RF_IO;
    if(menu.phase || menu.backend_owned || menu.overlay.stream || menu.stock_audio.stream ||
       menu.stock_music.stream || menu.stream.archive)return RF_RANGE;
    for(i=0;i<RF_XBOX_MENU_SOUND_COUNT;i++)if(menu.samples[i].storage)return RF_RANGE;
    return RF_OK;
}
int rf_xbox_menu_audio_end(void)
{
    uint32_t i;int status,left,right,third;
    if(menu.phase==MENU_AUDIO_BLOCKED)return RF_IO;
    if(menu.phase==MENU_AUDIO_IDLE)return RF_OK;
    if(menu.backend_owned){
        /* This is our own cold backend. Never call global reset merely because
         * a UI cue failed, or when an unrelated/gameplay backend denied begin. */
        rf_xbox_audio_close();status=rf_xbox_audio_idle_status();
        if(status){
            menu.phase=MENU_AUDIO_BLOCKED;rf_xbox_menu_audio_diagnostic[0]=menu.phase;
            rf_xbox_menu_audio_diagnostic[8]=(uint32_t)status;return status;
        }
        menu.backend_owned=0;rf_xbox_menu_audio_diagnostic[7]=0;
    }
    /* Neither native ordinary borrowers nor a music callback can access these
     * owners now. The queued native music data was never in this decoder. */
    for(i=0;i<RF_XBOX_MENU_SOUND_COUNT;i++){
        free(menu.samples[i].storage);memset(menu.samples+i,0,sizeof(menu.samples[i]));
    }
    rf_music_reset(&menu.stream);
    left=rf_vpp_close_checked(&menu.overlay);
    right=rf_vpp_close_checked(&menu.stock_audio);
    third=rf_vpp_close_checked(&menu.stock_music);
    status=left || right || third?RF_IO:RF_OK;
    rf_xbox_menu_audio_diagnostic[8]=(uint32_t)status;
    rf_xbox_menu_audio_diagnostic[5]=0;
    if(status){
        /* Checked VPP close already discarded uncertain FILE pointers. Keep
         * the failure latch, never retry fclose or clear admission evidence. */
        menu.phase=MENU_AUDIO_BLOCKED;rf_xbox_menu_audio_diagnostic[0]=menu.phase;return status;
    }
    memset(&menu,0,sizeof(menu));rf_xbox_menu_audio_diagnostic[0]=MENU_AUDIO_IDLE;return RF_OK;
}
int rf_xbox_menu_audio_begin(const rf_xbox_menu_audio_config *config)
{
    rf_vpp *music_archive=NULL;rf_vpp_entry entry;uint32_t i;int status,closed;
    if(!config || !config->stock_audio_path || !config->stock_music_path ||
       !config->music_name || !config->music_name[0] || config->looping>1)return RF_RANGE;
    status=rf_xbox_menu_audio_idle_status();if(status)return status;
    status=rf_xbox_audio_idle_status();if(status)return status;
    status=rf_vpp_checked_idle_status();if(status)return status;
    memset(rf_xbox_menu_audio_diagnostic,0,sizeof(rf_xbox_menu_audio_diagnostic));
    menu.phase=MENU_AUDIO_OWNED;menu.next_handle=1;
    menu.bytes=sizeof(menu)+sizeof(rf_xbox_menu_audio_diagnostic)+MENU_AUDIO_SCRATCH_RESERVE;
    rf_xbox_menu_audio_diagnostic[0]=menu.phase;rf_xbox_menu_audio_diagnostic[6]=menu.bytes;
    if(menu.bytes>MENU_AUDIO_BUDGET){status=RF_RANGE;goto fail;}
    if(config->overlay_path){status=rf_vpp_open_checked(&menu.overlay,config->overlay_path);if(status)goto fail;}
    for(i=0;i<RF_XBOX_MENU_SOUND_COUNT;i++){
        status=sample_load(i,config->stock_audio_path);
        rf_xbox_menu_audio_diagnostic[12+i]=(uint32_t)status;
        /* Missing individual stock/overlay members may be silent; an existing
         * corrupt member is a failed load, never permission to select stock. */
        if(status && status!=RF_NOT_FOUND)goto fail;
    }
    status=resolve(&music_archive,&entry,config->music_name,&menu.stock_music,
        config->stock_music_path,1u<<RF_XBOX_MENU_SOUND_COUNT);
    if(!status){
        status=rf_music_start(&menu.stream,music_archive,config->music_name);
        if(!status)menu.stream.looping=config->looping;
    }
    rf_xbox_menu_audio_diagnostic[2]=(uint32_t)status;
    if(status && status!=RF_NOT_FOUND)goto fail;
    /* UI PCM owns its bytes; only the chosen stream archive stays open. */
    status=rf_vpp_close_checked(&menu.stock_audio);if(status)goto fail;
    if(menu.stream.archive!=&menu.overlay){status=rf_vpp_close_checked(&menu.overlay);if(status)goto fail;}
    if(menu.stream.archive!=&menu.stock_music){status=rf_vpp_close_checked(&menu.stock_music);if(status)goto fail;}
    status=rf_xbox_audio_open();if(status)goto fail;
    menu.backend_owned=1;rf_xbox_menu_audio_diagnostic[7]=1;
    rf_xbox_menu_audio_poll();return RF_OK;
fail:
    rf_xbox_menu_audio_diagnostic[1]=(uint32_t)status;
    closed=rf_xbox_menu_audio_end();return closed?closed:status;
}
void rf_xbox_menu_audio_poll(void)
{
    int status;
    if(menu.phase!=MENU_AUDIO_OWNED || !menu.backend_owned)return;
    rf_xbox_audio_events.poll(NULL);
    if(!menu.music_failed){
        status=rf_xbox_audio_events.music(NULL,&menu.stream,1);
        if(status){
            /* A failed native retirement may still own pages. No stream reset,
             * archive close, replacement or retry before the full end fence. */
            menu.music_failed=1;rf_xbox_menu_audio_diagnostic[2]=(uint32_t)status;
        }
    }
    rf_xbox_menu_audio_diagnostic[9]=menu.stream.loops;
}
void rf_xbox_menu_audio_event(uint32_t sound)
{
    const rf_wave_pcm *pcm;uint32_t handle;float gain;int status;
    if(menu.phase!=MENU_AUDIO_OWNED || !menu.backend_owned || sound>=RF_XBOX_MENU_SOUND_COUNT)return;
    pcm=&menu.samples[sound].pcm;
    if(!pcm->samples)return;
    /* Handles are unique within this exclusive backend lifetime. Stop emitting
     * rather than wrapping an identity; no slot table or generation cap grows. */
    if(!menu.next_handle){++rf_xbox_menu_audio_diagnostic[4];return;}
    handle=menu.next_handle++;
    gain=sound==RF_XBOX_MENU_SOUND_SELECT?0.8f:1.0f;
    status=rf_xbox_audio_events.play_mode(NULL,handle,pcm,gain,gain,0);
    if(status)++rf_xbox_menu_audio_diagnostic[4];else ++rf_xbox_menu_audio_diagnostic[3];
}
void rf_xbox_menu_audio_highlight(uint32_t scope)
{
    if(scope==RF_XBOX_MENU_AUDIO_CATEGORY)rf_xbox_menu_audio_event(RF_XBOX_MENU_SOUND_HIGHLIGHT);
    else if(scope==RF_XBOX_MENU_AUDIO_PANEL)rf_xbox_menu_audio_event(RF_XBOX_PANEL_SOUND_HIGHLIGHT);
}
void rf_xbox_menu_audio_select(uint32_t scope)
{
    if(scope==RF_XBOX_MENU_AUDIO_CATEGORY)rf_xbox_menu_audio_event(RF_XBOX_MENU_SOUND_SELECT);
    else if(scope==RF_XBOX_MENU_AUDIO_PANEL)rf_xbox_menu_audio_event(RF_XBOX_PANEL_SOUND_SELECT);
}
