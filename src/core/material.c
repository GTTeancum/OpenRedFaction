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
static int open_materials(rf_materials *m,const rf_geometry *g,const char *const *names,uint32_t count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    uint32_t i;
    uint64_t slots;
    int result;
    if (!m) return RF_RANGE;
    memset(m, 0, sizeof(*m));
    if ((g && !g->data) || (!g && !names && count) || (!archives && archive_count)) return RF_RANGE;
    slots = (uint64_t)count * sizeof(rf_material);
    if (slots > budget) return RF_RANGE;
    if (!slots) return RF_OK;
    m->items = (rf_material *)calloc(count, sizeof(rf_material));
    if (!m->items) return RF_RANGE;
    m->count = count; m->allocated_bytes = (uint32_t)slots;
    for (i = 0; i < m->count; ++i) {
        char name[61];
        uint32_t a;
        rf_material *item = m->items + i;
        if (g) result = rf_geometry_texture_name(g, i, name, sizeof(name));
        else {
            size_t length=0;
            if (!names[i]) { result=RF_RANGE; goto fail; }
            while (length<sizeof(name) && names[i][length]) ++length;
            if (!length || length==sizeof(name)) { result=RF_RANGE; goto fail; }
            memcpy(name,names[i],length+1); result=RF_OK;
        }
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
int rf_materials_open(rf_materials *m,const rf_geometry *g,rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    if (!g) { if (m) memset(m,0,sizeof(*m)); return RF_RANGE; }
    return open_materials(m,g,NULL,g->textures,archives,archive_count,budget);
}
int rf_materials_open_names(rf_materials *m,const char *const *names,uint32_t count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    return open_materials(m,NULL,names,count,archives,archive_count,budget);
}
