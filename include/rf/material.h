#ifndef RF_MATERIAL_H
#define RF_MATERIAL_H
#include "rf/geometry.h"
#include "rf/image.h"
#include "rf/model.h"
#include "rf/model_file.h"
#include "rf/effect.h"
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
    char name[64];rf_particle_bitmap bitmap;
} rf_level_particle_texture;
typedef struct rf_level_particle_binding {uint32_t uid,texture;} rf_level_particle_binding;
typedef struct rf_level_particle_materials {
    void *storage;
    rf_level_particle_binding *bindings;rf_level_particle_texture *textures;
    uint32_t count,texture_count,resident_bytes;
} rf_level_particle_materials;
/* Owned first-frame textures and UID mappings for v180 level emitters, with
 * case-insensitive deduplication. Budget includes owner, worst-case slot arrays
 * and pixels; fixed decoder/reader stack and allocator metadata excluded.
 * At most 128 emitters. Missing A00 yields an empty bundle. First matching
 * archive wins, including errors. Zero-initialize; close before reuse. Failed
 * loads preserve output and release partial allocations. Archives may close
 * after success. Animated playback/frame replacement remains caller work. */
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
void rf_model_materials_close(rf_model_materials *materials);
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
#endif
