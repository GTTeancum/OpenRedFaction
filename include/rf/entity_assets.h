#ifndef RF_ENTITY_ASSETS_H
#define RF_ENTITY_ASSETS_H
#include "rf/vpp.h"
#include "rf/level.h"
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
/* Load entity.tbl into temporary storage capped by table_budget; release it
 * before return. Output unchanged on failure. Port-owned archive integration. */
int rf_entity_assets_load(const char *tables_path,const char *class_name,
    const char *skin,rf_entity_assets *assets,uint32_t table_budget);
/* Skeletal loader 0x51ce60's .v3c specialization of 0x5142d0/0x514330.
 * This does not select the loader for arbitrary entity model types (.v3d etc.).
 * Replaces everything from the last dot, including dots in directory names;
 * otherwise appends .v3c. Case is preserved. Input must terminate within 64
 * bytes; output is at most 63 bytes plus NUL. Supports in-place use, preserves
 * output on failure. Callers must skip model-less entities: empty input here
 * deliberately produces .v3c, matching the original filename helper. */
int rf_entity_skeletal_filename(const char *authored,char compiled[64]);
typedef struct rf_level_actor_assets {
    rf_level_entity entity;
    rf_entity_assets assets;
    rf_vpp_entry mesh;
} rf_level_actor_assets;
/* Port-owned binding: select a validated UID, load its class/skin metadata,
 * resolve a .vcm declaration and find the compiled mesh. Only skeletal authored
 * models are supported; empty/other extensions return FORMAT. No aliasing,
 * spawning, animation selection or mesh decoding. Caller retains level/mesh
 * archives. table_budget caps temporary table bytes; output unchanged on error. */
int rf_level_actor_assets_load(const rf_level *level,int32_t uid,const char *tables_path,
    rf_vpp *meshes,uint32_t table_budget,rf_level_actor_assets *result);
#endif
