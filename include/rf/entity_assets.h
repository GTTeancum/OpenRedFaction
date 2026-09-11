#ifndef RF_ENTITY_ASSETS_H
#define RF_ENTITY_ASSETS_H
#include "rf/vpp.h"
#include "rf/level.h"
#include "rf/motion_file.h"
#include "rf/entity.h"
#include "rf/model.h"
#include "rf/model_file.h"
#include "rf/movement.h"
#include "rf/effect.h"
typedef struct rf_weapon_names {
    char names[64][64];uint32_t count,primary_count;
} rf_weapon_names;
/* Selected weapons.tbl name/order reader: primary then secondary declarations,
 * matching4c67a0 sequencing. Requires both delimited sections. No weapon stats.
 * Port name limit63, capacity64; output unchanged on errors. */
int rf_weapon_names_read(const void *text,uint32_t bytes,rf_weapon_names *result);
int rf_weapon_names_load(rf_vpp *tables,uint32_t scratch_budget,rf_weapon_names *result);
/* 4c81f0 lookup over stable loaded names: first ASCII-insensitive match or-1.
 * NULL query behaves as empty. Table must be valid; no allocation. */
int32_t rf_weapon_name_find(const rf_weapon_names *table,const char *name);
/* Resolve selected class +Weapon Specific names to a64-bit ID mask. No fallback;
 * unknown names/duplicate groups fail and preserve output. Port metadata only. */
int rf_entity_weapon_groups_read(const void *text,uint32_t bytes,const char *class_name,
    const rf_weapon_names *weapons,uint32_t groups[2]);
/* Port-owned emitters.tbl binding: first ASCII case-insensitive name match,
 * copied metadata with no retained table pointers. Only the selected block is
 * validated. Read allocates nothing; load caps temporary archive storage by
 * scratch_budget. Both preserve output on failure; no resource creation. */
int rf_emitter_definition_read(const void *text,uint32_t bytes,const char *name,
    rf_particle_definition *result);
int rf_emitter_definition_load(rf_vpp *tables,const char *name,uint32_t scratch_budget,
    rf_particle_definition *result);
typedef struct rf_vclip_definition {
    char name[64],vbm[64],vfx[64],explosion[32],foley[64];
    uint32_t flags,glow;float damage,vfx_radius;
    int32_t particle_count;uint32_t has_particle,has_foley;
    rf_particle_definition particle;
} rf_vclip_definition;
/* Owned named vclip metadata, with defaults from 4c1460 and the shared
 * particle reader. First ASCII-insensitive name match. Selected block only;
 * names/resources are retained, not resolved into original runtime handles.
 * No allocation in read; load uses one budgeted scratch block. Output is
 * preserved on failure. This is a bounded port parser, not the full original. */
int rf_vclip_definition_read(const void *text,uint32_t bytes,const char *name,rf_vclip_definition *result);
int rf_vclip_definition_load(rf_vpp *tables,const char *name,uint32_t scratch_budget,rf_vclip_definition *result);
typedef struct rf_explosion_central {
    char emitter[64];uint32_t process_per_frame;float min_size,play_factor;
} rf_explosion_central;
typedef struct rf_explosion_recipe {
    char name[32];uint32_t flags,central_count;float play_time;
    rf_explosion_central central[6];
    char sparks[64],head[64],tail[64];int32_t sparks_count;
    uint32_t present; /* bits 0/1/2: sparks/head/tail authored. */
    float central_random,head_time,head_random;
} rf_explosion_recipe;
/* Ordered metadata reader using 48dd90 defaults, with six-slot bounds.
 * Optional absent records are zeroed and marked absent, not runtime defaults.
 * Central random is shared and overwritten by each central entry. Emitter
 * names are owned but not resolved. Error preserves output. */
int rf_explosion_recipe_read(const void *text,uint32_t bytes,const char *name,rf_explosion_recipe *result);
int rf_explosion_recipe_load(rf_vpp *tables,const char *name,uint32_t scratch_budget,rf_explosion_recipe *result);
typedef struct rf_explosion_definition {
    rf_explosion_recipe recipe;
    rf_particle_definition emitters[9]; /* central 0..5, sparks 6, head 7, tail 8 */
    uint32_t resolved,resident_bytes,peak_bytes;
} rf_explosion_definition;
/* Port-owned resolved metadata, with prepared directions and no asset pointers.
 * Missing central emitters fail; optional missing emitters leave mask bits clear.
 * Resolve allocates nothing. Load reuses one scratch buffer for both tables;
 * budget counts retained definition plus scratch, excluding stack and allocator
 * metadata. Output preserved on error. No images, live particles or scheduling. */
int rf_explosion_definition_resolve(const rf_explosion_recipe *recipe,const void *emitters,
    uint32_t bytes,rf_explosion_definition *result);
int rf_explosion_definition_load(rf_vpp *tables,const char *name,uint32_t budget,rf_explosion_definition *result);
/* 48e7c5..48e879 central size gate/scaling. Returns NOT_FOUND when below
 * minimum (also unordered) or unresolved; preserves output on non-success.
 * Copies velocity/radius/lifetime scaled by size, plus shared random extent.
 * Position, direction, allocation, random sampling and creation remain separate. */
int rf_explosion_central_prepare(const rf_explosion_definition *definition,uint32_t slot,float size,
    rf_particle_definition *particle,float *random_extent);
typedef struct rf_explosion_clock {
    float elapsed,size;uint32_t live,active;
} rf_explosion_clock;
typedef struct rf_explosion_clock_actions {uint32_t process,release;} rf_explosion_clock_actions;
/* Central-only timing from 48e290. Increment elapsed, select process slots,
 * then release all live slots on strict recipe expiry. Caller executes process
 * in slot order before release in slot order; callbacks may not mutate these
 * inputs during selection. Inactive clocks produce no actions. Finite time/size
 * domain; output/state preserved on error. Trail updates and list ownership
 * remain separate. No animation clock or particle processing is implemented. */
int rf_explosion_clock_tick(const rf_explosion_recipe *recipe,float dt,
    rf_explosion_clock *clock,rf_explosion_clock_actions *actions);
/* movemodes.tbl fields and name/reference tables from original 433670.
 * Bounded archive read, one scratch allocation; output preserved on failure. */
int rf_movement_descriptor_load(rf_vpp *tables,uint32_t index,uint32_t budget,rf_movement_descriptor *result);
/* game.tbl $Max Entity Jump Height from 433dd0/433e94. Decimal parser is
 * shared with authored class numbers; no NXDK strtod stub. Output preserved
 * on missing/duplicate/invalid height. Loader uses one bounded scratch block. */
int rf_game_jump_height_read(const void *text,uint32_t bytes,float *height);
int rf_game_jump_height_load(rf_vpp *tables,uint32_t budget,float *height);

typedef struct rf_entity_movement_values {
    float speed,slow_factor,fast_factor,acceleration;
} rf_entity_movement_values;
/* Numeric class +50..5c fields at 41be39..41bea0; absent factors default to 1. */
int rf_entity_movement_load(rf_vpp *tables,const char *name,uint32_t budget,rf_entity_movement_values *result);
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
/* Bounded port table storage. Lookup follows original 468740: exact ASCII
 * case-insensitive prefix before the first underscore, first declaration wins.
 * Read requires all ten coefficient records; output unchanged on error. The
 * lookup takes a successfully parsed table and NUL-terminated texture name. */
typedef struct rf_surface_materials {
    rf_entity_material materials[10];
    uint32_t count;
    struct {char name[32];uint32_t material;} prefixes[64];
} rf_surface_materials;
int rf_surface_materials_read(const void *text,uint32_t bytes,rf_surface_materials *result);
uint32_t rf_surface_material_lookup(const rf_surface_materials *table,const char *texture);

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
typedef struct rf_entity_lod_distances {
    uint32_t count;float distances[4];
} rf_entity_lod_distances;
/* Authored $LOD Distances from41bad7..41bb30; retain first four, consume
 * surplus numbers. Missing list is empty; no sorting or runtime selection.
 * Bounded grammar rejects malformed/duplicate lists; errors preserve output. */
int rf_entity_lod_distances_read(const void *text,uint32_t bytes,const char *name,
    rf_entity_lod_distances *result);

typedef struct rf_entity_seed_class {
    uint32_t record_index;
    char model[64];uint32_t model_kind;
    rf_entity_creation_vitals_class vitals;
    rf_entity_class_physics physics;
    rf_entity_lod_distances lod;
} rf_entity_seed_class;
typedef struct rf_entity_seed {
    uint32_t class_index;
    rf_level_entity_spawn spawn;
} rf_entity_seed;
typedef struct rf_entity_seeds {
    rf_level_owned_entities records;
    rf_entity_seed *items;
    rf_entity_seed_class *classes;
    uint32_t class_count,resident_bytes,peak_bytes;
} rf_entity_seeds;
/* Port-owned construction inputs, not live actors. Empty destination required.
 * Budget includes owner, retained allocations and peak table scratch, excluding
 * stack/allocator overhead. Class names share owned records, compared ignoring
 * ASCII case. Archives may close after success. Failure preserves destination. */
int rf_entity_seeds_open(const rf_level *level,rf_vpp *tables,uint32_t budget,rf_entity_seeds *result);
void rf_entity_seeds_close(rf_entity_seeds *seeds);
typedef struct rf_entity_skeleton {
    char model[64];rf_model_bone *bones;uint32_t count;
} rf_entity_skeleton;
typedef struct rf_entity_skeletons {
    rf_entity_skeleton *items;uint32_t *class_indices;
    uint32_t count,class_count,resident_bytes,peak_bytes;
} rf_entity_skeletons;
/* Level-owned shared immutable bones for kind2 classes. Non-skeletal classes
 * map to UINT32_MAX. No per-actor pose, geometry, skins or animation ownership.
 * Requires successful seeds, empty output. Budget includes owner/heap/scratch,
 * excluding stack and allocator overhead. Errors preserve output; archives and
 * seeds may close after success. Close clears storage and is repeatable. */
int rf_entity_skeletons_open(const rf_entity_seeds *seeds,rf_vpp *meshes,uint32_t budget,rf_entity_skeletons *result);
void rf_entity_skeletons_close(rf_entity_skeletons *skeletons);
typedef struct rf_entity_render_model {
    rf_model_file file;rf_model_geometry *lods;float (*stored)[12];uint32_t bone_count;
} rf_entity_render_model;
typedef struct rf_entity_render_models {
    rf_entity_render_model *items;uint32_t count,resident_bytes;
} rf_entity_render_models;
/* One immutable copy of every model LOD and bind transform per shared skeleton.
 * Bone/skeleton index order must stay stable; file metadata borrows meshes for
 * later material reads. Geometry itself is resident. No textures, posed vertices
 * or render scratch. Budget includes owner/arrays, excludes allocator/stack.
 * Empty output required; failure preserves it. Close is repeatable. */
int rf_entity_render_models_open(const rf_entity_skeletons *skeletons,rf_vpp *meshes,uint32_t budget,rf_entity_render_models *result);
void rf_entity_render_models_close(rf_entity_render_models *models);
typedef struct rf_entity_appearance {
    uint32_t skeleton,texture_count;char (*textures)[64];
} rf_entity_appearance;
typedef struct rf_entity_appearances {
    rf_entity_appearance *items;uint32_t *actor_indices;
    uint32_t count,actor_count,resident_bytes,peak_bytes;
} rf_entity_appearances;
/* Authored class/skin binding, shared by skeleton and ordered case-insensitive
 * replacement names. Empty skin uses base materials; non-skeletal actors map
 * to UINT32_MAX. This is port binding, not original dynamic skin switching.
 * Owns names/mappings; archives/seeds may close after success. Same skeleton
 * index order required. Budget includes table scratch and worst-case item slots.
 * Empty destination; failures preserve it; close is repeatable. No images. */
int rf_entity_appearances_open(const rf_entity_seeds *seeds,const rf_entity_skeletons *skeletons,
    rf_vpp *tables,uint32_t budget,rf_entity_appearances *result);
void rf_entity_appearances_close(rf_entity_appearances *appearances);
typedef struct rf_entity_pose {
    uint32_t skeleton,bone_count;rf_motion_playback_state playback;
    rf_motion_controller controller;
    float (*matrices)[12];uint16_t *generations;
} rf_entity_pose;
typedef struct rf_entity_poses {
    rf_entity_pose *items;float (*matrices)[12];uint16_t *generations;
    uint32_t count,bone_count,resident_bytes;
} rf_entity_poses;
/* Separate playback/cache storage per authored skeletal actor; no active motions
 * or valid cached matrices initially. Non-skeletal entries have UINT32_MAX.
 * Skeleton index order must remain stable while using poses. Empty destination;
 * budget covers owner/arrays, excluding stack/allocator overhead. New instances
 * only, not a reset/release path for live motion references. Failure preserves
 * destination; close releases arrays. Original character limit is50 bones. */
int rf_entity_poses_open(const rf_entity_seeds *seeds,const rf_entity_skeletons *skeletons,uint32_t budget,rf_entity_poses *result);
void rf_entity_poses_close(rf_entity_poses *poses);
/* Original41ba4d classification of the last-dot extension: vfx=3,vcm=2,
 * otherwise1, ASCII-insensitive. Bounded port input; error preserves output. */
int rf_entity_model_kind(const char *model,uint32_t *kind);
/* Required entity.tbl $Life, $Envirosuit and $FOV metadata from41bcba.
 * field_764 contains binary32 cos(FOV * binary32(pi/180) *0.5).
 * Selected class only, ASCII-insensitive name, no allocation in read. Missing
 * or repeated required fields fail; FOV0..360 and finite numbers are the port
 * input domain. Loader owns one bounded scratch block. Outputs preserved on
 * error; this is a port parser, not execution of the full original loader. */
int rf_entity_vitals_config_read(const void *text,uint32_t bytes,const char *class_name,
    rf_entity_creation_vitals_class *result);
int rf_entity_vitals_config_load(rf_vpp *tables,const char *class_name,uint32_t scratch_budget,
    rf_entity_creation_vitals_class *result);

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
typedef struct rf_entity_action_declaration {char motion[64],sound[64];} rf_entity_action_declaration;
/* Selected +Action name, motion and sound-class label. Exact base/weapon group,
 * ASCII-insensitive selection, no fallback. Empty quoted values are retained.
 * Port parser, no allocation, errors preserve output; sound ID resolution and
 * one-shot registration are separate. */
int rf_entity_action_read(const void *text,uint32_t bytes,const char *class_name,
    const char *weapon,const char *action,rf_entity_action_declaration *result);
int rf_entity_state_motion_load(const char *tables_path,const char *class_name,
    const char *weapon,const char *state,char motion[64],uint32_t table_budget);
/* Port binding of one exact table declaration to a validated compiled motion.
 * No fallback or registration/cache policy. Absent/empty/missing motion returns
 * NOT_FOUND. Caller keeps motions archive open. Output unchanged on failure. */
int rf_entity_state_motion_open(const char *tables_path,const char *class_name,
    const char *weapon,const char *state,rf_vpp *motions,uint32_t table_budget,rf_motion_file *file);
typedef struct rf_entity_state_declaration {
    char motion[64];uint32_t marker_count;float marker_frames[2];
} rf_entity_state_declaration;
/* Exact state plus optional +Footstep Trigger pair in authored left/right order.
 * Bounded port parser, not original full-table parser equivalence. Duplicate
 * pairs, missing/invalid numbers and duplicate selected states fail unchanged. */
int rf_entity_state_declaration_read(const void *text,uint32_t bytes,const char *class_name,
    const char *weapon,const char *state,rf_entity_state_declaration *result);
typedef struct rf_entity_default_weapons {int32_t primary,secondary;} rf_entity_default_weapons;
/* Required quoted class defaults, resolved with weapon-name lookup. Empty and
 * unknown names map to -1 as at41bf57..41bfe8. Duplicate/missing fields fail with
 * unchanged output. Port metadata reader, not a complete original parser. */
int rf_entity_default_weapons_read(const void *text,uint32_t bytes,const char *class_name,
    const rf_weapon_names *weapons,rf_entity_default_weapons *result);
typedef struct rf_entity_state_set {
    int32_t states[23];uint32_t count;
    rf_motion_cache_record cache[68];rf_motion_file files[68];
    uint32_t cache_indices[68];uint8_t looping[68];int32_t actions[45];char action_sounds[45][64];
    uint32_t weapon_groups[2];rf_entity_default_weapons default_weapons;
    uint32_t marker_counts[23];float marker_frames[23][2];
} rf_entity_state_set;
typedef struct rf_entity_weapon_motion_group {
    uint32_t class_index,weapon,count;
    int32_t states[23],actions[45];char action_sounds[45][64];
    rf_motion_file *files;uint8_t *looping;char (*identities)[64];
} rf_entity_weapon_motion_group;
typedef struct rf_entity_base_motions {
    rf_entity_state_set *classes;uint32_t class_count,resident_bytes,peak_bytes;
    rf_weapon_names weapons;
    rf_entity_weapon_motion_group *groups;uint32_t group_count;
} rf_entity_base_motions;
/* Retain canonical base state and action mappings per skeletal class, reading the
 * table once. Motion files borrow the caller's open immutable motions archive.
 * Includes sparse declared skeletal weapon groups, retaining only their actual
 * resource counts. Base authored footstep pairs and sound labels retained; no
 * sound-ID resolution, initial selection or playback. Each group's states/actions share local indices;
 * identities retain authored cache names (last-dot stems), which must not be
 * inferred from compiled filenames (first-dot stems). cache_indices maps each
 * base resource to its cache record; compact groups own the corresponding names.
 * Identical files with different looping flags register distinct entries.
 * Budget includes owner/arrays/table scratch; errors preserve empty output.
 * Port ownership and local motion indices, not the original global registry. */
int rf_entity_base_motions_open(const rf_entity_seeds *seeds,rf_vpp *tables,rf_vpp *motions,uint32_t budget,rf_entity_base_motions *result);
void rf_entity_base_motions_close(rf_entity_base_motions *motions);
/* Exact group only, NULL if absent; fallback/state switching policy is external. */
const rf_entity_weapon_motion_group *rf_entity_weapon_motion_find(const rf_entity_base_motions *motions,uint32_t class_index,int32_t weapon);
typedef struct rf_entity_model_motion {
    rf_motion_file file;char identity[64];uint8_t looping;
    rf_motion_weight_envelope comparison; /* Track-zero envelope, as in existing playback adapter. */
    int32_t markers[2];uint32_t marker_mask;
} rf_entity_model_motion;
typedef struct rf_entity_model_motions {
    rf_entity_model_motion *items;rf_motion_marker_names *marker_names;uint32_t count;
} rf_entity_model_motions;
typedef struct rf_entity_motion_mapping {
    uint32_t class_index,skeleton;int32_t weapon,states[23],actions[45];
} rf_entity_motion_mapping;
typedef struct rf_entity_motion_catalog {
    rf_entity_model_motions *models;rf_entity_motion_mapping *mappings;
    uint32_t model_count,mapping_count,class_count,resident_bytes,peak_bytes;
} rf_entity_motion_catalog;
/* Stable per-skeleton indices across class base/weapon groups. Requires owners
 * returned by skeletons_open/base_motions_open for the same seeds. Classes are
 * visited in seed order, weapon groups before base (422360); resolved cache
 * identity + exact loop byte keys use 539be0/51cc42 helpers. Original global
 * cache order and selection are not reproduced here. Per-model marker_names
 * retain two bounded names per motion index in contiguous owned storage. Base footstep markers are
 * registered in class/state order and shared across model/loop identities.
 * First class_count mappings are base, followed by source weapon-group order.
 * Maps/resources survive closing inputs; motion archives and skeleton index
 * order must remain valid. Sounds stay in the source bindings. No playback.
 * Empty destination; budget includes owner/heap/workspace (not stack/allocator
 * overhead). Errors preserve output. Close is repeatable. */
int rf_entity_motion_catalog_open(const rf_entity_skeletons *skeletons,
    const rf_entity_base_motions *bindings,uint32_t budget,rf_entity_motion_catalog *result);
void rf_entity_motion_catalog_close(rf_entity_motion_catalog *catalog);
/* Evaluate an actor's existing playback into its owned bone cache. Owners must
 * share stable skeleton indices; motion archives remain open. At most16 active
 * slots are projected into bounded stack scratch, with no heap allocation or
 * reference changes. Playback and catalog are read-only. Pending displacement
 * is consumed by evaluated roots. As with model_evaluate_playback, archive or
 * sampling failures may leave partial matrices/generations; caller must stop
 * using the failed pose. This does not select, advance or render an actor. */
int rf_entity_pose_evaluate(rf_entity_pose *pose,const rf_entity_skeletons *skeletons,
    const rf_entity_motion_catalog *catalog,float pending_displacement[3]);

typedef struct rf_entity_playback_model {
    rf_motion_playback_resource *resources;uint32_t *cache_ids;uint32_t count;
} rf_entity_playback_model;
typedef struct rf_entity_playback_resources {
    rf_entity_playback_model *models;rf_motion_playback_resource *resources;uint32_t *cache_ids;
    uint32_t model_count,resource_count,cache_count,resident_bytes,peak_bytes;
} rf_entity_playback_resources;
/* Shared mutable reference counters per catalog registration, initially zero.
 * Cache IDs join last-dot/case aliases across models and loop flags, using the
 * original cache key helper (at most800 unique identities). No lazy load/unload.
 * Budget includes retained arrays and temporary identity cache; excludes stack
 * and allocator overhead. Empty output required; errors preserve it. */
int rf_entity_playback_resources_open(const rf_entity_motion_catalog *catalog,uint32_t budget,rf_entity_playback_resources *result);
/* Level teardown only, after all actors using these counters have stopped. */
void rf_entity_playback_resources_close(rf_entity_playback_resources *resources);
/* Sum runtime references across all registrations of one cache identity.
 * Rejects negative counters/overflow without changing output. Does not release
 * any archive data; callers must not infer unloadability from a single slot. */
int rf_entity_playback_cache_references(const rf_entity_playback_resources *resources,uint32_t cache_id,uint32_t *references);
/* Advance existing actor playback, then refresh its owned pose cache. Shared
 * owners must correspond to the same catalog. Selection/weighting is external.
 * A sampling error can leave an advanced playback/partial pose; stop the actor
 * and release its slots before destroying these owners. No heap allocation. */
int rf_entity_pose_advance(rf_entity_pose *pose,const rf_entity_skeletons *skeletons,
    const rf_entity_motion_catalog *catalog,rf_entity_playback_resources *resources,float elapsed,float displacement[3]);
/* Release exactly this actor's active references, reset playback and invalidate
 * bone cache stamps. Other actors sharing registrations retain their counters.
 * Empty playback is repeatable; invalid slots/counts preserve the owner/pose. */
int rf_entity_pose_release(rf_entity_pose *pose,rf_entity_playback_resources *resources);
/* Initial unlinked, nonplayer actor selection from creation-cleared intent and
 * velocity, action0 and flags0. Uses base bindings before later weapon overlay.
 * Verified opening-class startup composition, not a complete actor constructor.
 * All poses must be fresh. On failure callers must release any active poses
 * before freeing shared resources; earlier actors may already be initialized. */
int rf_entity_poses_start_initial(const rf_entity_seeds *seeds,const rf_entity_skeletons *skeletons,
    const rf_entity_motion_catalog *catalog,rf_entity_playback_resources *resources,rf_entity_poses *poses,float elapsed);
/* Compose a base catalog map and an optional already-resolved weapon map using
 * the42ab20 overlay rule. Inputs must share class/skeleton; base weapon is-1.
 * Result keeps base entries when weapon entries are-1. Does not choose a weapon,
 * resolve its alias, alter playback or resolve sound labels. For an overridden
 * action use its weapon-group sound label; otherwise retain the base label.
 * Output may alias either input; invalid input leaves it unchanged. */
int rf_entity_motion_mapping_overlay(const rf_entity_motion_mapping *base,
    const rf_entity_motion_mapping *weapon,rf_entity_motion_mapping *result);
typedef struct rf_entity_motion_selection {
    rf_entity_motion_mapping mapping;const char *action_sounds[45];
} rf_entity_motion_selection;
/* Initial base selection, before runtime weapon overlays. Sound strings borrow
 * bindings; retain that owner while using the selection. No animation starts.
 * Catalog and bindings must originate from the same seeds; skeletal classes
 * only, non-skeletal returns NOT_FOUND. Errors preserve output. */
int rf_entity_motion_selection_base(const rf_entity_motion_catalog *catalog,
    const rf_entity_base_motions *bindings,uint32_t class_index,rf_entity_motion_selection *result);
/* Apply42ab20's weapon selection to the compact view: negative weapon is a
 * no-op; nonnegative IDs resolve machine-pistol-special to machine-pistol, then
 * restore base and overlay available records. Sound labels follow their action
 * record, including explicit empty labels. Does not grant/equip weapons, change
 * playback, or decide when selection should occur. Output is unchanged on error. */
int rf_entity_motion_selection_weapon(const rf_entity_motion_catalog *catalog,
    const rf_entity_base_motions *bindings,uint32_t class_index,int32_t weapon,
    rf_entity_motion_selection *result);
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
typedef struct rf_entity_skeletal_assets {
    rf_entity_assets assets;
    rf_vpp_entry mesh;
} rf_entity_skeletal_assets;
/* Class/skin resource binding without a serialized level entity. Resolves only
 * authored .vcm models; no spawn, skin-index policy, mesh decoding or ownership
 * registration. Table budget caps scratch allocation. Output unchanged on
 * failure; caller retains meshes archive. Shares the level-actor resolver. */
int rf_entity_skeletal_assets_load(const char *tables_path,const char *class_name,
    const char *skin,rf_vpp *meshes,uint32_t table_budget,rf_entity_skeletal_assets *result);
/* Port-owned binding: select a validated UID, load its class/skin metadata,
 * resolve a .vcm declaration and find the compiled mesh. Only skeletal authored
 * models are supported; empty/other extensions return FORMAT. No aliasing,
 * spawning, animation selection or mesh decoding. Caller retains level/mesh
 * archives. table_budget caps temporary table bytes; output unchanged on error. */
int rf_level_actor_assets_load(const rf_level *level,int32_t uid,const char *tables_path,
    rf_vpp *meshes,uint32_t table_budget,rf_level_actor_assets *result);
#endif
