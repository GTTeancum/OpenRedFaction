#ifndef RF_COMPOSED_CHECKPOINT_H
#define RF_COMPOSED_CHECKPOINT_H
#include "rf/player_checkpoint.h"
#include "rf/checkpoint_file.h"
enum {
    RF_COMPOSED_PROFILE_CAVITY=1,
    RF_COMPOSED_PROFILE_AUTHORED=2,
    RF_COMPOSED_CHECKPOINT_HEADER=32,
    RF_COMPOSED_CHECKPOINT_RFDS_MIN=288,
    RF_COMPOSED_CHECKPOINT_RFDS_MAX=RF_CHECKPOINT_FILE_MAX-RF_COMPOSED_CHECKPOINT_HEADER-RF_PLAYER_CHECKPOINT_BYTES
};
typedef struct rf_composed_checkpoint {
    rf_player_checkpoint player;
    const unsigned char *rfds;uint32_t rfds_bytes;
} rf_composed_checkpoint;
/* RFCPv1:32 LE header bytes (magic,version,total,flags0,profile_id,RFPL bytes,
 * RFDS bytes,reserved0), RFPL544 then unchanged RFDS bytes. No checksum: RFSG
 * transport owns it. profile1 requires RFDSv1; profile2 requires RFDSv2 with
 * its416-byte header/128-byte extension. Unknown or crossed profiles reject;
 * full source/level identity remains the existing RFDS identity/level checks.
 * This is bounded settled DEV player+destruction state, NOT a full-game save.
 * No allocation. Inputs/outputs disjoint except RFDS may already occupy its
 * exact final output+576 slice (no self-copy). Errors preserve every output.
 * Encoder performs player and structural RFDS checks before writing; caller
 * must already validate RFDS semantics. RFDS max109948 preserves RFSG110524 cap.
 * Capacity may exceed required bytes; only written bytes are changed. */
int rf_composed_checkpoint_encode(uint32_t profile_id,const rf_player_checkpoint *,
    const rf_player_checkpoint_catalog *,const void *rfds,uint32_t rfds_bytes,
    void *output,uint32_t capacity,uint32_t *written);
/* Preflight only: validates RFCP lengths/profile/reserved words, RFPL candidate,
 * and RFDS magic/version/total length. Returns a BORROWED RFDS slice. Caller
 * MUST run its complete pure RFDS identity/geometry/material/placement validator
 * before publishing either state; backing bytes must live through that work.
 * Standalone RFDS compatibility is an explicit caller dispatch policy; this
 * parser never silently treats a legacy RFDS as a composed save. */
int rf_composed_checkpoint_preflight(const void *,uint32_t bytes,uint32_t profile_id,
    const rf_player_checkpoint_catalog *,rf_composed_checkpoint *);
#endif
