#ifndef RF_MATERIAL_H
#define RF_MATERIAL_H
#include "rf/geometry.h"
#include "rf/image.h"
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
#endif
