#ifndef RF_ENTITY_ASSETS_H
#define RF_ENTITY_ASSETS_H
#include "rf/vpp.h"
typedef struct rf_entity_assets {
    char model[64];
    char textures[64][64];uint32_t texture_count;
} rf_entity_assets;
/* Metadata reader for installed entity.tbl syntax, not the original full table
 * parser. Caller-owned text, no allocation; class/skin comparisons ignore ASCII
 * case. Empty skin selects authored base materials. Preserves .vcm/.v3d names
 * verbatim: runtime compiled-filename resolution is separate. Empty model means
 * the class has no authored model (e.g. a camera). Output unchanged
 * on failure. Limits: token 255 bytes, asset 63 bytes, 64 skin replacements. */
int rf_entity_assets_read(const void *text,uint32_t bytes,const char *class_name,
    const char *skin,rf_entity_assets *assets);
#endif
