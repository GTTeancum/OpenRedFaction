#ifndef RF_SETTINGS_INI_H
#define RF_SETTINGS_INI_H
#include <stdint.h>
/* Port-owned user settings read from openrf.ini beside the executable (Xbox:
 * D:\openrf.ini). Not an original-game file. Plain "key = value" lines;
 * "[section]" headers, blank lines and ';' or '#' comments are ignored, keys
 * and values are case-insensitive. Unknown keys and bad values keep the
 * default and are counted, never fatal.
 *
 * frame_rate: 60       game updates 60 times a second; pictures dropped when behind
 *             30       game updates 30 times a second, one picture each (steady 30)
 *             30_smooth 30 updates a second, an in-between picture after each (60 shown)
 *             auto     60 while it keeps up, otherwise the fallback below
 * fallback:   30_smooth | 30 | none   (used by auto; none keeps dropping pictures)
 * smooth_characters: on | off  move characters and vehicles in in-between pictures
 * tracers:   on | off  tracer streaks on the player's own bullet weapons
 *             (port option; the original draws none on the player's shots)
 * flicker_filter: auto | 0..5  TV encoder vertical blur (auto: 0 progressive, 2 interlaced)
 * textures: compressed | original  use the DXT texture pack (rfdds.vpp) when present
 * resolution: 480 | 720 | auto  720 needs the dashboard's 720p setting; the
 *             picture is 16:9 widescreen (auto = 720 when allowed)
 * color_depth: auto | 32 | 16  16 halves frame-buffer memory (dithered);
 *             auto uses 16 only at 720p, where the memory is needed */
enum { RF_FRAME_RATE_60, RF_FRAME_RATE_30, RF_FRAME_RATE_30_SMOOTH, RF_FRAME_RATE_AUTO };
typedef struct rf_settings {
    uint32_t frame_rate,fallback,smooth_characters,flicker_filter; /* flicker: 0..5, or UINT32_MAX auto */
    uint32_t compressed_textures,resolution; /* resolution: 480, 720, or 0 auto */
    uint32_t player_tracers; /* 1: player shots draw tracers like NPC rounds */
    uint32_t color_depth; /* 32, 16 (R5G6B5 frame buffers, dithered), or 0 auto: 16 at 720p, 32 at 480 */
    uint32_t unknown,invalid; /* ignored lines, for the RAM log */
} rf_settings;
/* Defaults when no file exists: fixed 60 (crash-harness replays rely on it). */
void rf_settings_defaults(rf_settings *settings);
/* Parse bytes (not NUL-terminated); settings must already hold defaults. */
void rf_settings_parse(rf_settings *settings,const char *text,uint32_t bytes);
#endif
