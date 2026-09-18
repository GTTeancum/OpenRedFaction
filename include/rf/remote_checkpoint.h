#ifndef RF_REMOTE_CHECKPOINT_H
#define RF_REMOTE_CHECKPOINT_H
#include "rf/remote_charge.h"
#define RF_REMOTE_CHECKPOINT_HEADER 48u
#define RF_REMOTE_CHECKPOINT_RECORD 228u
#define RF_REMOTE_CHECKPOINT_MAX (RF_REMOTE_CHECKPOINT_HEADER+RF_REMOTE_CHARGE_CAPACITY*RF_REMOTE_CHECKPOINT_RECORD)
typedef struct rf_remote_checkpoint {
    rf_remote_charge charges[RF_REMOTE_CHARGE_CAPACITY];
    uint32_t tags[RF_REMOTE_CHARGE_CAPACITY],owner_keys[RF_REMOTE_CHARGE_CAPACITY],host_keys[RF_REMOTE_CHARGE_CAPACITY];
    uint32_t held,pending,delay,cooldown,selected_mode; /*0 other,1 charge,2 detonator*/
} rf_remote_checkpoint;
/* Independent port RFRM1 chunk:48 LE header +228-byte records for ACTIVE slots
 * only. Full binary32 state, explicit words, no native padding/pointers. Header
 * carries caller level/catalog hashes and pending throw scheduler. Max7344B.
 * No checksum: enclosing RFSG owns integrity. Not a compatible RFCP1 append;
 * integrate via versioned wrapper after player/GeoMod preflight.
 * owner_keys/host_keys are caller durable identities (not runtime handles),
 * UINT32_MAX unknown rejected for active owner or bound host. Raw saved handles
 * are retained for diagnostics ONLY: decoded state is a quarantined candidate.
 * Parent MUST resolve keys, replace owner/host/contact tags, validate restored
 * hosts/geometry and publish with player/GeoMod atomically. Never decode directly
 * over live state. Keys and level hashes are not authentication.
 * No allocation; <=one record scratch. All inputs/outputs disjoint and immutable
 * during calls. Error preserves output/written. Omitted slots reset to zero. */
int rf_remote_checkpoint_encode(const rf_remote_checkpoint *,uint32_t level_hash,uint32_t catalog_hash,
    void *,uint32_t capacity,uint32_t *written);
int rf_remote_checkpoint_decode(const void *,uint32_t bytes,uint32_t level_hash,uint32_t catalog_hash,
    rf_remote_checkpoint *candidate);
/* Pure bounded validation, no decoded pool scratch or publication. */
int rf_remote_checkpoint_preflight(const void *,uint32_t bytes,uint32_t level_hash,uint32_t catalog_hash);
#endif
