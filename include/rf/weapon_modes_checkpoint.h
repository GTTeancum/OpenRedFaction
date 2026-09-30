#ifndef RF_WEAPON_MODES_CHECKPOINT_H
#define RF_WEAPON_MODES_CHECKPOINT_H
#include "rf/vpp.h"
enum { RF_WEAPON_MODES_CHECKPOINT_BYTES=32,
    RF_WEAPON_MODES_MACHINE_PISTOL_SPECIAL=1u,
    RF_WEAPON_MODES_UNDERCOVER_SUPPRESSOR_ATTACHED=2u,
    RF_WEAPON_MODES_PLAYER_SHIELD=4u };
typedef struct rf_weapon_modes_checkpoint {
    uint32_t flags,conventional_rng;float shield_life;
} rf_weapon_modes_checkpoint;
/* RFWM1: eight LE words: magic, version, length, FNV checksum (this word
 * treated as zero), catalog hash, flags, RNG, reserved zero. All RNG values
 * are valid. RFWM2 uses the final word for positive finite shield durability
 * when PLAYER_SHIELD is set; otherwise it must be zero. RFWM1 remains readable
 * and is written when no shield is owned. Caller must enforce inventory ownership, resource availability
 * and settled weapon transitions; this codec contains no runtime handles.
 * No allocation. Errors preserve outputs. Input/output/written must be
 * disjoint; successful encoding writes exactly32bytes. */
int rf_weapon_modes_checkpoint_encode(uint32_t catalog_hash,
    const rf_weapon_modes_checkpoint *,void *,uint32_t capacity,uint32_t *written);
int rf_weapon_modes_checkpoint_decode(const void *,uint32_t bytes,
    uint32_t catalog_hash,rf_weapon_modes_checkpoint *);
#endif
