#ifndef RF_CLUTTER_GAMEPLAY_H
#define RF_CLUTTER_GAMEPLAY_H
#include "rf/vpp.h"
/* Reusable parsing scratch, NOT retained once per instance. Resource names are
 * owned, unresolved and may be empty. Bind resources separately before damage
 * readiness. Existing rf_clutter_class owns the shared timer; vclip resource
 * owns particle_count. No effects, health mutation or resource lookup here. */
typedef struct rf_clutter_gameplay_definition {
    char name[64],explosion[64],corpse[64],debris_model[64],debris_sound[64];
    float life,damage_factors[11];
    float explosion_radius,explosion_damage,explosion_offset[3],debris_velocity;
    uint32_t flags,protected_object,present;
} rf_clutter_gameplay_definition;
enum {
    RF_CLUTTER_GAMEPLAY_DEBRIS_VELOCITY=1,
    RF_CLUTTER_GAMEPLAY_DEBRIS_MODEL=2,
    RF_CLUTTER_GAMEPLAY_DEBRIS_SOUND=4
};
/* First ASCII-insensitive $Class Name match, before $Skin or next class.
 * Reuses rf_clutter_definition_read for required model/material/life/flags and
 * existing resource metadata validation. Damage factors default to1; nine
 * original named types supported, indices9/10 remain1; repeated factors last
 * win. Radius/damage default1, offset0. Finite signed factors/life accepted.
 * Negative life sets protected_object=1; life itself remains authored.
 * Missing debris velocity is explicitly absent: its zero scratch value is NOT
 * the original default and must not be used without the presence bit.
 * Duplicate singleton, malformed number/vector/name returns an error, retaining
 * result byte-for-byte. No allocation; input bytes need not be terminated. */
int rf_clutter_gameplay_read(const void *text,uint32_t bytes,const char *name,
    rf_clutter_gameplay_definition *result);
/* Load-time accelerator for many lookups in one clutter.tbl buffer. Build
 * records, in token order, every unquoted "$Class Name:" pair exactly as the
 * reader's pre-match scan sees it (outside #Clutter too). Indexed reads parse
 * from the first recorded name match, which is the same token position the
 * full scan selects, so results and errors are byte-identical to
 * rf_clutter_gameplay_read(text,bytes,name,...). Recording stops at the first
 * tokenizer/name error or when capacity is full (complete=0); a name with no
 * recorded match always falls back to the full scan. Caller owns storage and
 * must pass the same text/bytes to both calls. No allocation. */
typedef struct rf_clutter_class_index_entry {uint32_t boundary,name,length;} rf_clutter_class_index_entry;
typedef struct rf_clutter_class_index {
    rf_clutter_class_index_entry *entries;const void *text;uint32_t count,capacity,complete,bytes;
} rf_clutter_class_index;
int rf_clutter_class_index_build(const void *text,uint32_t bytes,rf_clutter_class_index_entry *entries,
    uint32_t capacity,rf_clutter_class_index *index);
int rf_clutter_gameplay_read_indexed(const rf_clutter_class_index *index,const void *text,uint32_t bytes,
    const char *name,rf_clutter_gameplay_definition *result);
#endif
