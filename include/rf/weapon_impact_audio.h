#ifndef RF_WEAPON_IMPACT_AUDIO_H
#define RF_WEAPON_IMPACT_AUDIO_H
#include "rf/entity_assets.h"

/* Original weapon descriptor178 and1a0: ordinary and UNDERWATER material
 * tables, never primary/alternate trigger modes. Material order is the shared
 * 4686c0 order: Default,Rock,Metal,Flesh,Water,Lava,Solid,Sand,Ice,Glass.
 * Missing/empty/unresolved groups remain-1; runtime sample selection, followed
 * by same-table default fallback, belongs to the contact consumer. */
typedef struct rf_weapon_impact_audio_groups {
    int32_t groups[64][2][10];
    uint32_t count;
} rf_weapon_impact_audio_groups;

/* Read $Impact Sound: "material[ underwater]" "Foley group" for the
 * immutable named weapon order. Last repeated slot wins, including an empty
 * or unresolved-1 group, as in the original parser. Unknown material names
 * select Default. The installed
 * underwater suffix is recognized separately; unsupported placement rejects.
 * Allocates bounded temporary staging; output is preserved on every error. */
int rf_weapon_impact_audio_groups_read(const void *text,uint32_t bytes,
    const rf_weapon_names *names,const rf_foley_owner *sounds,
    rf_weapon_impact_audio_groups *result);
/* scratch_budget covers weapons.tbl plus the fixed staged group catalog.
 * No audio declaration, PCM load, or mutation of the supplied Foley owner. */
int rf_weapon_impact_audio_groups_load(rf_vpp *tables,uint32_t scratch_budget,
    const rf_weapon_names *names,const rf_foley_owner *sounds,
    rf_weapon_impact_audio_groups *result);
#endif
