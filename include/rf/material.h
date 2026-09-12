#ifndef RF_MATERIAL_H
#define RF_MATERIAL_H
#include "rf/geometry.h"
#include "rf/image.h"
#include "rf/model.h"
#include "rf/model_file.h"
#include "rf/effect.h"
#include "rf/entity_assets.h"
#include "rf/glare.h"
typedef struct rf_particle_bitmap {
    rf_image image;
    uint32_t frames,rate,archive_index,resident_bytes;
} rf_particle_bitmap;
/* Port resource ownership: first matching archive wins, including failures.
 * Loads one requested frame, with budget including this record and pixels.
 * Fixed decoder stack and allocator metadata excluded. TGA has frame 0 only.
 * Output preserved on failure. Zero-initialize; close before reuse. Archives
 * and authored metadata may be released after success. No animation clock. */
int rf_particle_bitmap_open(rf_particle_bitmap *bitmap,const rf_particle_definition *definition,
    rf_vpp *archives,uint32_t archive_count,uint32_t frame,uint32_t budget);
void rf_particle_bitmap_close(rf_particle_bitmap *bitmap);
/* Blood-pool asset selected by original42db58 (string595ef8). Uses the
 * existing bitmap owner and archive precedence; frame0, caller budget.
 * Release with rf_particle_bitmap_close. Does not bind a GPU texture. */
int rf_corpse_surface_texture_open(rf_particle_bitmap *bitmap,rf_vpp *archives,
    uint32_t archive_count,uint32_t budget);
typedef struct rf_particle_animation {
    rf_image *images;
    uint32_t count,rate,archive_index,resident_bytes;
} rf_particle_animation;
/* Retain every base-mip frame for allocation-free particle frame selection.
 * First matching archive wins. Budget includes owner, image descriptors and
 * all decoded pixels; decoder stack/allocator metadata excluded. Frame zero
 * transfers into the final array without a duplicate pixel allocation.
 * TGA yields one frame. Animated mip chains remain unsupported by the decoder.
 * Zero-initialize; close before reuse. Failure preserves output and frees all
 * partial state. Archives and definition may be released after success. */
int rf_particle_animation_open(rf_particle_animation *animation,const rf_particle_definition *definition,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget);
void rf_particle_animation_close(rf_particle_animation *animation);
typedef struct rf_level_particle_texture {
    char name[64];rf_particle_animation animation;
} rf_level_particle_texture;

typedef struct rf_glare_materials {
    void *storage;uint32_t (*bindings)[3];rf_level_particle_texture *textures;
    uint32_t count,texture_count,resident_bytes;
} rf_glare_materials;
/* Owned corona/volumetric/reflection slots per authored class, UINT32_MAX
 * for absent blocks. ASCII-insensitive deduplication, first archive wins.
 * Retains every base-mip frame; no animation clock or original bitmap IDs.
 * Budget includes owner, worst-case slot arrays and all decoded images.
 * Fixed decoder stack and allocator metadata excluded. At most64 classes.
 * Empty destination required; failure preserves it and releases partial state.
 * Definitions and archives may be released after success. */
int rf_glare_materials_open(rf_glare_materials *materials,
    const rf_glare_definition *definitions,uint32_t count,rf_vpp *archives,
    uint32_t archive_count,uint32_t budget);
void rf_glare_materials_close(rf_glare_materials *materials);
typedef struct rf_level_particle_binding {uint32_t uid,texture;} rf_level_particle_binding;
typedef struct rf_level_particle_materials {
    void *storage;
    rf_level_particle_binding *bindings;rf_level_particle_texture *textures;
    uint32_t count,texture_count,resident_bytes;
} rf_level_particle_materials;
/* Owned all-frame textures and UID mappings for v180 level emitters, with
 * case-insensitive deduplication. Budget includes owner, worst-case slot arrays
 * and pixels; fixed decoder/reader stack and allocator metadata excluded.
 * At most 128 emitters. Missing A00 yields an empty bundle. First matching
 * archive wins, including errors. Zero-initialize; close before reuse. Failed
 * loads preserve output and release partial allocations. Archives may close
 * after success. Frame selection uses the particle clock; all base frames remain resident. */
int rf_level_particle_materials_open(rf_level_particle_materials *materials,const rf_level *level,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget);
void rf_level_particle_materials_close(rf_level_particle_materials *materials);
typedef struct rf_material {
    rf_image image;
    int status;
    uint32_t archive_index;
} rf_material;
typedef struct rf_materials {
    rf_material *items;
    uint32_t count, loaded, missing, allocated_bytes;
} rf_materials;
/* Archives searched in caller-specified order; first exact case-insensitive
 * name wins. Missing entries are explicit slots, never substituted textures.
 * Unsupported/corrupt found images fail the whole load. TGA and static VBM supported.
 * Budget includes slots and decoded images, excluding allocator metadata.
 * Close before reuse; any failure releases allocations and leaves empty state. */
int rf_materials_open(rf_materials *materials, const rf_geometry *geometry,
                      rf_vpp *archives, uint32_t archive_count, uint32_t budget);
void rf_materials_close(rf_materials *materials);
/* Same bounded loader for a caller-owned name list (e.g. model materials).
 * Names are nonempty, NUL-terminated, at most 60 bytes. Slots retain caller
 * order, including duplicates. Close before reuse; failure leaves empty state. */
int rf_materials_open_names(rf_materials *materials,const char *const *names,uint32_t count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget);
typedef struct rf_model_materials {
    rf_materials textures;
    rf_model_material_instance *items;
    uint32_t count,resident_bytes,peak_bytes;
} rf_model_materials;
/* Per-model residency with case-insensitive texture deduplication. Budget
 * includes bundle, instance records, arrays, texture slots/pixels and temporary
 * name/mapping records, excluding allocator metadata and caller-owned model.
 * Runtime texture handles are bundle-local slot indices. Missing textures fail.
 * Zero-initialize before first use; close before reuse; failure leaves empty. */
int rf_model_materials_open(rf_model_materials *materials,const rf_model_file *model,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget);
/* Ordered primary-texture substitution scaffolding for authored entity skins,
 * not the original runtime skin-switch implementation. Nonzero primary_count
 * must exactly match all material records in SUBM section order. Names must
 * be nonempty and terminate within 32 bytes. Zero count selects base materials.
 * Secondary maps and other disk fields remain intact; deduplication and alpha
 * classification use the selected images. Same ownership/budget contract above. */
int rf_model_materials_open_skin(rf_model_materials *materials,const rf_model_file *model,
    const char *const *primary_names,uint32_t primary_count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget);
/* Same owned texture/material conversion from retained serialized84-byte rows.
 * Records remain caller-owned and unchanged; no geometry archive is required.
 * Budget includes the copied scratch rows and existing bundle ownership. */
int rf_model_materials_open_records(rf_model_materials *materials,const uint8_t (*records)[84],uint32_t count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget);
void rf_model_materials_close(rf_model_materials *materials);
/* Retained rows plus post-conversion primary replacements (410d30 semantics).
 * Optional overrides has count entries; NULL entries retain base handles.
 * Named replacements are 1..60 bytes. All other200-byte material fields and
 * auxiliary arrays retain base conversion, including alpha/secondary state.
 * Base and replacement images are retained, case-insensitive deduplicated.
 * Same ownership/budget/error contract as open_records; no glare effects. */
int rf_model_materials_open_records_overrides(rf_model_materials *materials,const uint8_t (*records)[84],uint32_t count,
    const char *const *overrides,rf_vpp *archives,uint32_t archive_count,uint32_t budget);
typedef struct rf_entity_materials {
    rf_model_materials materials;
    uint32_t *offsets;uint32_t count,resident_bytes,peak_bytes;
} rf_entity_materials;
/* Port ownership for authored NPC appearances. Appearance i uses flat material
 * indices [offsets[i], offsets[i+1]); primary/secondary handles index the single
 * shared image table. Deduplicates case-insensitively across all appearances.
 * Budget includes owner, retained arrays/pixels and temporary records; excludes
 * allocator metadata, decoder stack and caller-owned inputs. Static base mips
 * only. Inputs/archives may close after success. Zero-initialize; close before
 * reuse. Failure preserves output and releases partial state. */
int rf_entity_materials_open(rf_entity_materials *materials,const rf_entity_appearances *appearances,
    const rf_entity_render_models *models,rf_vpp *archives,uint32_t archive_count,uint32_t budget);
void rf_entity_materials_close(rf_entity_materials *materials);

typedef struct rf_geometry_materials {
    rf_materials textures;
    uint32_t *offsets, *slots;
    uint32_t count, resident_bytes, peak_bytes;
} rf_geometry_materials;
/* Shared world/mover texture residency scaffolding, not original allocation
 * policy. Geometry i's local texture j maps to slots[offsets[i]+j]. Names
 * deduplicate case-insensitively in first-use order, including missing images.
 * Owns all mappings/images; input geometries and archives may close afterward.
 * Budget includes owner, retained arrays/images and temporary names/pointers,
 * excluding allocator metadata. Zero-initialize; close before reuse. Failure
 * preserves output. Inputs must remain stable throughout the call. */
int rf_geometry_materials_open(rf_geometry_materials *materials,
    const rf_geometry *const *geometries,uint32_t count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget);
void rf_geometry_materials_close(rf_geometry_materials *materials);
/* Resolve an initial authored face through geometry-local texture slots and
 * sample its currently retained image. No allocation or placeholder texture.
 * Missing texture/slot image returns its load status; malformed mappings fail.
 * No animation clock or runtime texture override is implied. Color preserved
 * on failure; workspace follows geometry sampling scratch rules. */
int rf_geometry_material_sample(const rf_geometry_materials *materials,uint32_t geometry_index,
    const rf_geometry *geometry,uint32_t face,const float point[3],
    rf_geometry_texture_workspace *work,uint32_t *color);
typedef struct rf_geometry_body_surfaces {
    const rf_geometry *const *geometries;uint32_t count; /* World first, then movers. */
    const rf_geometry_materials *mapping;const rf_surface_materials *palette;
} rf_geometry_body_surfaces;
/* Metadata callback for rf_geometry_collision_body_sweep. Resolves file texture
 * names through the authored material prefixes (468740) and returns the shared
 * port texture slot. Slot IDs are not original RF.exe handles. A -1 face texture
 * returns -1/default material (468700). Borrowed loaded inputs, no allocation or
 * texture loading; errors preserve both outputs. No runtime face mutations. */
int rf_geometry_body_surface(void *context,uint32_t solid,uint32_t face,
    uint32_t *texture,uint32_t *material);
#endif
