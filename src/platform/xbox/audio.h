#ifndef RF_XBOX_AUDIO_H
#define RF_XBOX_AUDIO_H
#include "rf/scene_preview.h"
int rf_xbox_audio_open(void);
void rf_xbox_audio_close(void);
enum {RF_XBOX_AUDIO_VOLUME_MAX=100u,RF_XBOX_AUDIO_VOLUME_DEFAULT=100u};
typedef struct rf_xbox_audio_options {
    uint32_t master_percent,effects_percent,music_percent;
} rf_xbox_audio_options;
/* Process-retained user levels, initialized to 100/100/100 and not reset by
 * open/close. Effects covers every ordinary voice, including speech; streamed
 * music has its own level. No PCM or authored gain is changed. Null get is a
 * no-op; get also works before the optional audio backend is opened. */
void rf_xbox_audio_get_options(rf_xbox_audio_options *options);
/* Main-thread/platform-owner call, serialized with play/poll/pause/reset.
 * Null or any percentage above 100 returns RF_RANGE without changing anything.
 * Valid settings are retained and applied to all created voices, including
 * paused voices, without stopping/restarting them. RF_IO means a native gain
 * update failed: requested settings stay retained (and get returns them),
 * native gains may be partially applied, and the ordinary poll retries them.
 * Voice/PCM/stream ownership is unchanged on failure. Future voices use the
 * retained settings. An unopened backend accepts valid settings with RF_OK.
 * This API never accesses settings storage; persistence belongs to its caller. */
int rf_xbox_audio_set_options(const rf_xbox_audio_options *options);
/* Read-only shutdown admission, including the existing stop-timeout fault.
 * Never treats a paused voice or a failed drain as successful retirement. */
int rf_xbox_audio_idle_status(void);
/* Native-only single-player pause. Serialized with play/reset by the platform
 * owner; preserves voice cursors, borrowed PCM and the music decoder. Resume
 * only starts voices paused by this operation, never completed one-shots.
 * Idempotent; an unopened optional backend succeeds without doing anything. */
int rf_xbox_audio_set_paused(uint32_t paused);
/* paused, pause entries, resumes, marked ordinary voices, marked music,
 * completed voices not restarted, empty music deferrals, API failures. */
extern uint32_t rf_xbox_audio_pause_diagnostic[8];
/* Synchronously stop/destroy one logical voice and release its PCM page locks.
 * RF_NOT_FOUND means this handle owns no slot. RF_IO retains ownership: caller
 * must not free PCM on failure. Other voices are untouched. Calls are serialized
 * with play/reset; all voices borrowing a sample must be released before unload. */
int rf_xbox_audio_release_voice(uint32_t handle);
/* Port ownership operation, serialized with play/reset. RF_RANGE means null
 * samples or a matching active/looping voice; no borrowers are changed.
 * RF_OK releases all completed borrowers with this exact PCM base pointer
 * (also succeeds if none). Other voices remain untouched. Bank-owned sample
 * views must use one consistent base, not overlapping/subrange aliases. */
int rf_xbox_audio_release_idle_sample(const uint8_t *samples);
extern const rf_scene_audio_events rf_xbox_audio_events;
/* status, plays, stops, rejected, resets, nonzero DMA samples, adapter bytes,
 * peak active voices, pages after init, before init, after close, timeout fault.
 * DMA observation is not proof of host-speaker audibility. */
extern uint32_t rf_xbox_audio_diagnostic[12];
extern uint32_t rf_xbox_audio_close_phase;
/* Music: created, starts, queued blocks, completed blocks, starvation polls,
 * errors, queued PCM frames, nonzero queued samples, bounded adapter bytes,
 * current queued buffers, track revision, last error. Completion is native
 * consumption evidence, not proof that host speakers are audible. */
extern volatile uint32_t rf_xbox_music_diagnostic[12];
#endif
