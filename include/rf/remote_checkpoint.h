#ifndef RF_REMOTE_CHECKPOINT_H
#define RF_REMOTE_CHECKPOINT_H
#include "rf/remote_charge.h"
#define RF_REMOTE_CHECKPOINT_HEADER_V1 48u
#define RF_REMOTE_CHECKPOINT_HEADER 56u
#define RF_REMOTE_CHECKPOINT_RECORD 228u
#define RF_REMOTE_CHECKPOINT_MAX (RF_REMOTE_CHECKPOINT_HEADER+RF_REMOTE_CHARGE_CAPACITY*RF_REMOTE_CHECKPOINT_RECORD)
/* Port safety bound for the fixed60Hz selection timer, not an authored delay. */
#define RF_REMOTE_FOLLOWUP_MAX_TICKS 3600u
typedef struct rf_remote_checkpoint {
    rf_remote_charge charges[RF_REMOTE_CHARGE_CAPACITY];
    uint32_t tags[RF_REMOTE_CHARGE_CAPACITY],owner_keys[RF_REMOTE_CHARGE_CAPACITY],host_keys[RF_REMOTE_CHARGE_CAPACITY];
    uint32_t held,pending,delay,cooldown,selected_mode; /*0 other,1 charge,2 detonator*/
    uint32_t followup_target,followup_ticks; /*0 none,1 charge,2 detonator; remaining60Hz ticks*/
} rf_remote_checkpoint;
/* Independent port RFRM chunk: version1 has48-byte LE header; version2 has56
 * with unchanged bytes0..43, reserved zero word44, followup target48/ticks52.
 * Both retain228-byte ACTIVE-slot records, full binary32 state and explicit
 * words without native padding/pointers. Header carries caller level/catalog
 * hashes and pending throw scheduler. Max7352B (version1 max7344B). Encoding
 * retains version1 unless followup_target is nonzero. Legacy decode clears the
 * followup; neither deployed charges nor cooldown infer an unsaved selection.
 * A followup targets the opposite selected mode; return-to-charge cannot have
 * a pending throw, and return-to-detonator must outlast any pending release.
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
