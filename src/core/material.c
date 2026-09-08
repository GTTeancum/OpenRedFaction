#include "rf/material.h"
#include <stdlib.h>
#include <string.h>
void rf_materials_close(rf_materials *m)
{
    uint32_t i;
    if (!m) return;
    for (i = 0; i < m->count; ++i) rf_image_close(&m->items[i].image);
    free(m->items); memset(m, 0, sizeof(*m));
}
int rf_materials_open(rf_materials *m, const rf_geometry *g, rf_vpp *archives,
                      uint32_t archive_count, uint32_t budget)
{
    uint32_t i;
    uint64_t slots;
    int result;
    if (!m) return RF_RANGE;
    memset(m, 0, sizeof(*m));
    if (!g || !g->data || (!archives && archive_count)) return RF_RANGE;
    slots = (uint64_t)g->textures * sizeof(rf_material);
    if (slots > budget) return RF_RANGE;
    if (!slots) return RF_OK;
    m->items = (rf_material *)calloc(g->textures, sizeof(rf_material));
    if (!m->items) return RF_RANGE;
    m->count = g->textures; m->allocated_bytes = (uint32_t)slots;
    for (i = 0; i < m->count; ++i) {
        char name[61];
        uint32_t a;
        rf_material *item = m->items + i;
        result = rf_geometry_texture_name(g, i, name, sizeof(name));
        if (result) goto fail;
        item->status = RF_NOT_FOUND; item->archive_index = UINT32_MAX;
        for (a = 0; a < archive_count; ++a) {
            rf_vpp_entry entry;
            result = rf_vpp_find(archives + a, name, &entry);
            if (result == RF_NOT_FOUND) continue;
            if (result) goto fail;
            result = rf_image_tga(&item->image, archives + a, &entry, budget - m->allocated_bytes);
            if (result) goto fail;
            item->archive_index = a; item->status = RF_OK;
            m->allocated_bytes += item->image.bytes; ++m->loaded;
            break;
        }
        if (item->status == RF_NOT_FOUND) ++m->missing;
    }
    return RF_OK;
fail:
    rf_materials_close(m);
    return result;
}
