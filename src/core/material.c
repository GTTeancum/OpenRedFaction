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
void rf_model_materials_close(rf_model_materials *m)
{
    uint32_t i;
    if (!m) return;
    for (i=0;i<m->count;++i) rf_model_material_instance_close(m->items+i);
    free(m->items);rf_materials_close(&m->textures);memset(m,0,sizeof(*m));
}
static int equal_texture_name(const char *a,const char *b)
{
    for (;;) {
        unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++;
        if (x>='A' && x<='Z') x+='a'-'A';
        if (y>='A' && y<='Z') y+='a'-'A';
        if (x!=y) return 0;
        if (!x) return 1;
    }
}
int rf_model_materials_open(rf_model_materials *m,const rf_model_file *model,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    rf_model_materials next={0}; uint8_t *raw=NULL;const char **names=NULL;int32_t *mapping=NULL;
    uint64_t count=0,base,scratch,used;uint32_t i,j,k,mesh=0,at=0,unique=0;int status=RF_OK;
    if (!m || !model || !model->archive || model->section_count>RF_MODEL_MAX_SECTIONS ||
        (!archives && archive_count) || m->items || m->textures.items || m->resident_bytes) return RF_RANGE;
    for(i=0;i<model->section_count;++i) if(model->sections[i].type==0x5355424d) count+=model->sections[i].material_count;
    base=sizeof(next)+count*sizeof(*next.items);
    scratch=count*(84+2*sizeof(*names)+2*sizeof(*mapping));used=base+scratch;
    if(count>INT32_MAX/2 || used>budget || used>SIZE_MAX) return RF_RANGE;
    if(count) {
        next.items=calloc((size_t)count,sizeof(*next.items));raw=malloc((size_t)count*84);
        names=malloc((size_t)count*2*sizeof(*names));mapping=malloc((size_t)count*2*sizeof(*mapping));
        if(!next.items || !raw || !names || !mapping) { status=RF_IO;goto done; }
    }
    next.count=(uint32_t)count;
    for(i=0;i<model->section_count;++i) if(model->sections[i].type==0x5355424d) {
        for(j=0;j<model->sections[i].material_count;++j,++at) {
            uint8_t *record=raw+(size_t)at*84;
            status=rf_model_file_material(model,mesh,j,record);if(status)goto done;
            if(!record[0] || !memchr(record,0,32) || !memchr(record+48,0,32)) { status=RF_FORMAT;goto done; }
            for(k=0;k<2;++k) {
                const char *name=(const char *)record+(k?48:0);uint32_t slot;
                mapping[at*2+k]=-1;if(!*name)continue;
                for(slot=0;slot<unique;++slot)if(equal_texture_name(name,names[slot]))break;
                if(slot==unique)names[unique++]=name;
                mapping[at*2+k]=(int32_t)slot;
            }
        }
        ++mesh;
    }
    status=rf_materials_open_names(&next.textures,names,unique,archives,archive_count,budget-(uint32_t)used);
    if(status)goto done;
    if(next.textures.missing) { status=RF_NOT_FOUND;goto done; }
    used+=next.textures.allocated_bytes;
    for(i=0;i<next.count;++i) {
        uint32_t alpha=(uint32_t)rf_image_format_has_alpha(next.textures.items[mapping[i*2]].image.source_format);
        status=rf_model_material_from_disk(next.items+i,raw+(size_t)i*84,84,mapping[i*2],mapping[i*2+1],alpha,
            budget-(uint32_t)used+(uint32_t)sizeof(*next.items));
        if(status)goto done;
        used+=next.items[i].accounted_bytes-sizeof(*next.items);
    }
    next.peak_bytes=(uint32_t)used;next.resident_bytes=(uint32_t)(used-scratch);
done:
    free(raw);free(names);free(mapping);
    if(status)rf_model_materials_close(&next);else *m=next;
    return status;
}
