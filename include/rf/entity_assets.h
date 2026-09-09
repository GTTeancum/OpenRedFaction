#ifndef RF_ENTITY_ASSETS_H
#define RF_ENTITY_ASSETS_H
#include "rf/vpp.h"
#include "rf/level.h"
#include "rf/motion_file.h"
#include "rf/entity.h"
#include "rf/movement.h"
/* movemodes.tbl fields and name/reference tables from original 433670.
 * Bounded archive read, one scratch allocation; output preserved on failure. */
int rf_movement_descriptor_load(rf_vpp *tables,uint32_t index,uint32_t budget,rf_movement_descriptor *result);
typedef struct rf_entity_class_physics {
    float mass;
    char material[64];
    uint32_t flags,flags2;
    uint32_t movement_index,use_kind;
    float use_radius;
} rf_entity_class_physics;
/* Authored mass/material/flag/use metadata only; movement_index identifies the
 * original 16-name table, not a resolved active movement descriptor. Missing
 * use-kind/radius starts at zero for a fresh static class. Later class defaults and mutations
 * are not applied. Unknown flags fail; errors preserve output. No allocation. */
int rf_entity_class_physics_read(const void *text,uint32_t bytes,const char *name,
    rf_entity_class_physics *result);
typedef struct rf_entity_material {
    uint32_t index;
    float elasticity,friction,density,buoyancy,traction;
} rf_entity_material;
/* Bounded materials.tbl metadata reader. Unknown names select Default as in
 * 4686c0. Requires all five finite coefficients; output unchanged on failure.
 * Does not load bitmap prefixes, debris or hit sounds. No allocation. */
int rf_entity_material_read(const void *text,uint32_t bytes,const char *name,
    rf_entity_material *result);
typedef struct rf_entity_sphere_declarations {
    uint32_t count;rf_entity_sphere_override items[8];
} rf_entity_sphere_declarations;
/* Bounded installed-table metadata reader for named sphere overrides, optional
 * radius and spring constant/length. Missing options use -1 (length bits zero
 * until a spring is supplied). No class defaults or model matching; errors
 * preserve output. This is not the original full table parser. */
int rf_entity_sphere_declarations_read(const void *text,uint32_t bytes,const char *class_name,
    rf_entity_sphere_declarations *result);
typedef struct rf_entity_physics_config {
    rf_entity_class_physics authored;
    rf_entity_material material;
    rf_entity_sphere_declarations spheres;
} rf_entity_physics_config;
/* Port-owned archive binding: reads entity.tbl once, then reuses the same
 * scratch allocation for materials.tbl. Budget caps that temporary allocation;
 * output is caller-owned and contains no archive pointers. Errors preserve it.
 * Does not resolve model spheres/poses or construct a gameplay body. */
int rf_entity_physics_config_load(rf_vpp *tables,const char *class_name,
    uint32_t scratch_budget,rf_entity_physics_config *result);
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
/* Select one authored +State motion from entity.tbl. Empty weapon selects only
 * the base block; a named weapon selects only that +Weapon Specific block.
 * No fallback, filename conversion or playback policy is inferred. ASCII case
 * insensitive keys; quoted empty motion is a valid explicit declaration.
 * Port metadata parser, not original table-parser equivalence. Output is a
 * zero-filled 64-byte name on success and unchanged on failure. */
int rf_entity_state_motion_read(const void *text,uint32_t bytes,const char *class_name,
    const char *weapon,const char *state,char motion[64]);
int rf_entity_state_motion_load(const char *tables_path,const char *class_name,
    const char *weapon,const char *state,char motion[64],uint32_t table_budget);
/* Port binding of one exact table declaration to a validated compiled motion.
 * No fallback or registration/cache policy. Absent/empty/missing motion returns
 * NOT_FOUND. Caller keeps motions archive open. Output unchanged on failure. */
int rf_entity_state_motion_open(const char *tables_path,const char *class_name,
    const char *weapon,const char *state,rf_vpp *motions,uint32_t table_budget,rf_motion_file *file);
typedef struct rf_entity_state_set {
    int32_t states[23];uint32_t count;
    rf_motion_cache_record cache[23];rf_motion_file files[23];
} rf_entity_state_set;
/* Register the 23 canonical state names (0x418030 order) from one exact base
 * or weapon block. Missing/empty declarations map to -1; missing referenced
 * files fail the whole operation. Distinct cache identities register once as
 * looping states. Port-owned composition, no fallback or playback transition.
 * Reads entity.tbl once; budget covers temporary table + working set, excluding
 * caller's output. Output unchanged on error. Result borrows motions archive;
 * no result heap allocation or close operation, caller retains archive lifetime. */
int rf_entity_state_set_open(const char *tables_path,const char *class_name,
    const char *weapon,rf_vpp *motions,uint32_t budget,rf_entity_state_set *result);
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
