#ifndef RF_MATERIAL_H
#define RF_MATERIAL_H
#include "rf/geometry.h"
#include "rf/image.h"
#include "rf/model.h"
#include "rf/model_file.h"
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
 * Unsupported/corrupt found images fail the whole load. Only TGA supported.
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
#endif
