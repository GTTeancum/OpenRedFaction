#ifndef RF_XBOX_MENU_AUDIO_H
#define RF_XBOX_MENU_AUDIO_H
#include <stdint.h>

/* Cold frontend only. These never install scene callbacks, pause/resume a
 * session, or steal an open audio backend. Main-thread calls only. */
typedef struct rf_xbox_menu_audio_config {
    const char *stock_audio_path,*stock_music_path,*overlay_path,*music_name;
    uint32_t looping;
} rf_xbox_menu_audio_config;
enum {RF_XBOX_MENU_AUDIO_CATEGORY,RF_XBOX_MENU_AUDIO_PANEL};
enum rf_xbox_menu_sound {
    RF_XBOX_MENU_SOUND_HIGHLIGHT,RF_XBOX_MENU_SOUND_SELECT,
    RF_XBOX_PANEL_SOUND_HIGHLIGHT,RF_XBOX_PANEL_SOUND_SELECT,
    RF_XBOX_PANEL_SOUND_SCROLL_UP,RF_XBOX_PANEL_SOUND_SCROLL_DOWN,
    RF_XBOX_PANEL_SOUND_SWEEP_IN,RF_XBOX_PANEL_SOUND_SWEEP_OUT,
    RF_XBOX_MENU_SOUND_COUNT
};
/* Overlay NULL means explicitly absent/disabled, not open failure. A non-NULL
 * overlay must open successfully; only a missing member falls through to the
 * stock archive. Malformed overrides never choose stock data. Config strings
 * are borrowed during this call only. The chosen music archive remains owned
 * by this module until end; UI files are independent immutable PCM owners.
 * Begin requires backend and checked-VPP admission already idle. An ordinary
 * load/init error unwinds; an uncertain close retains a BLOCKED owner. A
 * caller may continue a silent menu only after this module, audio backend and
 * checked-VPP idle statuses all return RF_OK. Always report the begin error. */
int rf_xbox_menu_audio_begin(const rf_xbox_menu_audio_config *config);
/* Pump on every cold-menu loop, including idle/controller-disconnected and
 * nested Options/Audio/Mapping loops. Never call through scene audio owners.
 * Optional play/stream errors are reported in diagnostics; end remains required. */
void rf_xbox_menu_audio_poll(void);
/* Caller gates actual accepted changes; these do not poll/reinterpret input.
 * No sound for redraw, initial focus, held/reconnected input, disabled choices
 * or navigation with no change. All ordinary voices are non-looping 2D. */
void rf_xbox_menu_audio_highlight(uint32_t scope);
void rf_xbox_menu_audio_select(uint32_t scope);
/* Extra authored classes are available, not implicitly tied to page changes.
 * Scroll/sweep calls require the corresponding implemented interaction. */
void rf_xbox_menu_audio_event(uint32_t sound);
/* Checked full backend close before any PCM free or archive close. Failure
 * retains ownership, blocks later begin/end retries and must prevent gameplay
 * or dashboard launch. An idle end is harmless and never touches gameplay. */
int rf_xbox_menu_audio_end(void);
int rf_xbox_menu_audio_idle_status(void);
/* phase (0 idle,1 owned,2 blocked), begin status, music status, plays,drops,
 * retained WAV bytes, accounted peak, backend owned, end status, music loops,
 * overlay-resolution mask, stock-resolution mask, then eight sample statuses.
 * Bit8 in resolution masks is music. No audibility/runtime claim is implied. */
extern uint32_t rf_xbox_menu_audio_diagnostic[20];
#endif
