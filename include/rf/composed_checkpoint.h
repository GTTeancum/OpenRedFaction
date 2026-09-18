#ifndef RF_COMPOSED_CHECKPOINT_H
#define RF_COMPOSED_CHECKPOINT_H
#include "rf/player_checkpoint.h"
#include "rf/checkpoint_file.h"
enum {
    RF_COMPOSED_PROFILE_CAVITY=1,
    RF_COMPOSED_PROFILE_AUTHORED=2,
    RF_COMPOSED_PROFILE_AUTHORED_COLLECTION=3,
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
 * its416-byte header/128-byte extension and exact bounded variable-table spans.
 * Profile3 requires RFDSv3 with an RFAS1 directory instead of one core,
 * shared tables once, and no global body trailer. Scene restore remains a caller gate.
 * Unknown or crossed profiles reject;
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
 * and RFDS magic/version/total length (plus authored table spans). Returns a BORROWED RFDS slice. Caller
 * MUST run its complete pure RFDS identity/geometry/material/placement validator
 * before publishing either state; backing bytes must live through that work.
 * Standalone RFDS compatibility is an explicit caller dispatch policy; this
 * parser never silently treats a legacy RFDS as a composed save. */
int rf_composed_checkpoint_preflight(const void *,uint32_t bytes,uint32_t profile_id,
    const rf_player_checkpoint_catalog *,rf_composed_checkpoint *);
typedef struct rf_composed_checkpoint_v2 {
    rf_composed_checkpoint base;
    const unsigned char *remote;uint32_t remote_bytes; /* borrowed, NULL/0 if absent */
} rf_composed_checkpoint_v2;
/* Opt-in RFCP2, same32-byte header: word28 stores optional RFRM length.
 * Layout is RFPL544, RFDS, then validated RFRM1 bytes (or none). Legacy encode/
 * preflight remain v1-only and unchanged. New preflight accepts v1 as absent
 * remote state, never synthesizing charges. Total RFSG payload cap unchanged:
 * remote bytes reduce available RFDS budget. Full RFDS identity/placement and
 * durable remote owner/host remapping remain caller publication gates.
 * level_hash/catalog_hash validate RFRM context only; not substitutes for full
 * outer identity validation. No allocation/pool scratch. Error atomic output.
 * Same disjointness rules as v1; RFDS/remote may already be at their exact final
 * output slices, with immutable sources for the duration of validation/copy. */
int rf_composed_checkpoint_encode_v2(uint32_t profile_id,const rf_player_checkpoint *,
    const rf_player_checkpoint_catalog *,const void *rfds,uint32_t rfds_bytes,
    const void *remote,uint32_t remote_bytes,uint32_t level_hash,uint32_t catalog_hash,
    void *output,uint32_t capacity,uint32_t *written);
int rf_composed_checkpoint_preflight_v2(const void *,uint32_t bytes,uint32_t profile_id,
    const rf_player_checkpoint_catalog *,uint32_t level_hash,uint32_t catalog_hash,
    rf_composed_checkpoint_v2 *);
typedef struct rf_composed_checkpoint_v3 {
    rf_composed_checkpoint_v2 base;
    const unsigned char *vehicle;uint32_t vehicle_bytes;
    uint32_t vehicle_profile; /*0 absent,1 Driller,2 APC,3 Jeep,4 submarine; validated RFVC class*/
} rf_composed_checkpoint_v3;
/* RFCP3 retains the32-byte header and RFPL/RFDS/RFRM offsets. Word12 now
 * contains optional RFVC length (0,128 or160); RFVC follows RFRM. New preflight
 * accepts RFCP1/2 as vehicle-absent; old APIs deliberately reject RFCP3.
 * Actual class decoders enforce RFVC1/Driller128, RFVC2/APC128, RFVC3/Jeep160 and RFVC4/submarine128.
 * Crossed version/class/size rejects. RFVC checksum and value ranges are checked, but class/world identity,
 * clearance, occupancy and player binding remain transactional caller gates.
 * Occupied RFVC requires RFPL3 seated encoding; absent/unoccupied RFVC requires
 * legacy standing RFPL. RFVC is decoded before selecting the RFPL decoder;
 * crossed occupancy modes reject. This validates semantic pairing, not actual
 * tag placement or safe live restore. Stored seated angles are exit look.
 * Vehicle bytes reduce RFDS capacity under the existing RFSG cap.
 * All inputs disjoint from output except each payload may occupy its exact
 * final slice. Error preserves output/written; returned slices are borrowed. */
int rf_composed_checkpoint_encode_v3(uint32_t profile_id,const rf_player_checkpoint *,
    const rf_player_checkpoint_catalog *,const void *rfds,uint32_t rfds_bytes,
    const void *remote,uint32_t remote_bytes,uint32_t level_hash,uint32_t catalog_hash,
    const void *vehicle,uint32_t vehicle_bytes,void *output,uint32_t capacity,uint32_t *written);
int rf_composed_checkpoint_preflight_v3(const void *,uint32_t bytes,uint32_t profile_id,
    const rf_player_checkpoint_catalog *,uint32_t level_hash,uint32_t catalog_hash,
    rf_composed_checkpoint_v3 *);
#endif
