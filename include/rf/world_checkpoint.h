#ifndef RF_WORLD_CHECKPOINT_H
#define RF_WORLD_CHECKPOINT_H
#include "rf/checkpoint_file.h"
#ifdef __cplusplus
extern "C" {
#endif
enum rf_world_checkpoint_section {
    RF_WORLD_PLAYER=1,RF_WORLD_NPC,RF_WORLD_MOVER,RF_WORLD_EVENT,RF_WORLD_TRIGGER,
    RF_WORLD_GOALS,RF_WORLD_PICKUPS,RF_WORLD_CLUTTER,RF_WORLD_WEAPON_MODES,
    RF_WORLD_REMOTE,RF_WORLD_VEHICLE,RF_WORLD_DESTRUCTION,RF_WORLD_SWITCHES,RF_WORLD_STARTUP,
    RF_WORLD_CAMPAIGN_HISTORY,RF_WORLD_ENVIRONMENT
};
enum {RF_WORLD_CHECKPOINT_SECTIONS=16,RF_WORLD_CHECKPOINT_HEADER=128,
    RF_WORLD_CHECKPOINT_DIRECTORY_ROW=12,RF_WORLD_CHECKPOINT_PREFIX_V1=308,RF_WORLD_CHECKPOINT_PREFIX=320};
#define RF_WORLD_CHECKPOINT_MASK(type) (UINT32_C(1)<<((type)-1))
#define RF_WORLD_CHECKPOINT_ALL_MASK ((UINT32_C(1)<<RF_WORLD_CHECKPOINT_SECTIONS)-1)
typedef struct rf_world_checkpoint_slice {const void *data;uint32_t bytes;} rf_world_checkpoint_slice;
typedef struct rf_world_checkpoint {
    unsigned char identity[32];char level[64];
    rf_world_checkpoint_slice sections[RF_WORLD_CHECKPOINT_SECTIONS]; /* type minus one */
} rf_world_checkpoint;
/* Structural RFWC2 envelope only, capped at RF_CHECKPOINT_FILE_MAX unchanged.
 * Wire: 128-byte header (magic RFWC at0, u32 version2 at4, total at8,
 * FNV1a checksum at12, section count at16, reserved zero at20, identity at24,
 * level at56, reserved zero at120/124), then 16 directory rows, then payload.
 * Each row is u32 type/offset/bytes. All integers are little endian. Checksum
 * covers the entire envelope with bytes12..15 treated as zero. Maximum total
 * includes the 320-byte prefix. Components, including campaign history, are
 * opaque: presence alone does not establish complete gameplay restore support.
 * Fixed canonical directory lists all 16 types once, sorted, with contiguous
 * payload offsets including zero-length absent components. required_mask is
 * caller policy: each selected component must be nonempty. Omit EVENT/TRIGGER
 * etc when the authenticated level has none; semantic codecs remain required.
 * Header identity/level and component bytes must be validated against authored
 * resources by caller BEFORE any gameplay publication. Hash is corruption
 * detection, not authentication. Decode slices borrow unchanged input storage.
 * No allocation. Errors leave all outputs unchanged. Inputs/outputs disjoint,
 * except each encoder payload may already occupy its exact final output slice.
 * Decode accepts RFWC1 (15 sections, prefix308) with ENVIRONMENT absent;
 * requiring ENVIRONMENT rejects that legacy envelope. Encoder emits onlyv2.
 * Zero-length decode slices have data=NULL. Level must be nonempty and NUL
 * terminated within64; encoder canonicalizes its padding to zero. */
int rf_world_checkpoint_encode(const rf_world_checkpoint *,uint32_t required_mask,
    void *,uint32_t capacity,uint32_t *written);
int rf_world_checkpoint_decode(const void *,uint32_t bytes,uint32_t required_mask,rf_world_checkpoint *);
int rf_world_checkpoint_preflight(const void *,uint32_t bytes,uint32_t required_mask);
#ifdef __cplusplus
}
#endif
#endif
