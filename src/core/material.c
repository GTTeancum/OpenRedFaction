#include "rf/material.h"
#include <stdlib.h>
#include <string.h>
int rf_geometry_body_surface(void *context,uint32_t solid,uint32_t face,
    uint32_t *texture,uint32_t *material)
{
    const rf_geometry_body_surfaces *c=context;const rf_geometry *g;rf_geometry_face source;
    uint32_t index,slot=UINT32_MAX,value=0,offset;char name[256];int status;
    if(!c || !texture || !material || !c->mapping || !c->palette || !c->geometries)return RF_RANGE;
    if(solid!=UINT32_MAX && solid>=c->count)return RF_RANGE;
    index=solid==UINT32_MAX?0:solid+1;
    if(index>=c->count || c->mapping->count!=c->count || !c->mapping->offsets)return RF_RANGE;
    g=c->geometries[index];if(!g)return RF_RANGE;
    status=rf_geometry_get_face(g,face,&source);if(status)return status;
    if(source.texture!=UINT32_MAX) {
        if(source.texture>=g->textures || !c->mapping->slots)return RF_FORMAT;
        offset=c->mapping->offsets[index];
        if(c->mapping->offsets[index+1]<offset || c->mapping->offsets[index+1]-offset!=g->textures)return RF_FORMAT;
        slot=c->mapping->slots[offset+source.texture];if(slot>=c->mapping->textures.count)return RF_FORMAT;
        status=rf_geometry_texture_name(g,source.texture,name,sizeof(name));if(status)return status;
        value=rf_surface_material_lookup(c->palette,name);
    }
    *texture=slot;*material=value;return RF_OK;
}
void rf_particle_animation_close(rf_particle_animation *animation)
{
    uint32_t i;if(!animation)return;
    for(i=0;i<animation->count;i++)rf_image_close(animation->images+i);
    free(animation->images);memset(animation,0,sizeof(*animation));
}
int rf_particle_animation_open(rf_particle_animation *animation,const rf_particle_definition *definition,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    rf_particle_animation value={0};rf_particle_bitmap first={0};rf_vpp_entry entry;
    uint64_t bytes;uint32_t i;int status;
    if(!animation || animation->images || animation->count || budget<sizeof(value))return RF_RANGE;
    status=rf_particle_bitmap_open(&first,definition,archives,archive_count,0,budget);if(status)return status;
    bytes=sizeof(value)+(uint64_t)first.frames*(sizeof(rf_image)+(uint64_t)first.image.bytes);
    if(!first.frames || bytes>budget){status=RF_RANGE;goto failed;}
    value.images=calloc(first.frames,sizeof(*value.images));if(!value.images){status=RF_IO;goto failed;}
    value.count=first.frames;value.rate=first.rate;value.archive_index=first.archive_index;value.resident_bytes=(uint32_t)bytes;
    value.images[0]=first.image;memset(&first.image,0,sizeof(first.image));
    if(value.count>1) {
        status=rf_vpp_find(archives+value.archive_index,definition->bitmap,&entry);if(status)goto failed;
        for(i=1;i<value.count;i++) {
            uint32_t count,rate;
            status=rf_image_vbm_frame(value.images+i,archives+value.archive_index,&entry,i,
                value.images[0].bytes,&count,&rate);if(status)goto failed;
            if(count!=value.count || rate!=value.rate || value.images[i].bytes!=value.images[0].bytes ||
               value.images[i].width!=value.images[0].width || value.images[i].height!=value.images[0].height) {
                status=RF_FORMAT;goto failed;
            }
        }
    }
    rf_particle_bitmap_close(&first);*animation=value;return RF_OK;
failed:
    rf_particle_bitmap_close(&first);rf_particle_animation_close(&value);return status;
}
void rf_particle_bitmap_close(rf_particle_bitmap *bitmap)
{
    if(!bitmap)return;
    rf_image_close(&bitmap->image);memset(bitmap,0,sizeof(*bitmap));
}
int rf_particle_bitmap_open(rf_particle_bitmap *bitmap,const rf_particle_definition *definition,
    rf_vpp *archives,uint32_t archive_count,uint32_t frame,uint32_t budget)
{
    rf_particle_bitmap value={0};uint32_t i;int status;
    if(!bitmap || !definition || (!archives && archive_count) || budget<sizeof(value))return RF_RANGE;
    if(!definition->bitmap[0] || !memchr(definition->bitmap,0,sizeof(definition->bitmap)))return RF_RANGE;
    for(i=0;i<archive_count;++i) {
        rf_vpp_entry entry;unsigned char magic[4];
        status=rf_vpp_find(archives+i,definition->bitmap,&entry);
        if(status==RF_NOT_FOUND)continue;if(status)return status;
        if(entry.size<4)return RF_FORMAT;
        status=rf_vpp_read(archives+i,&entry,0,magic,4);if(status)return status;
        if(!memcmp(magic,".vbm",4))status=rf_image_vbm_frame(&value.image,archives+i,&entry,
            frame,budget-(uint32_t)sizeof(value),&value.frames,&value.rate);
        else {
            if(frame)return RF_RANGE;
            status=rf_image_tga(&value.image,archives+i,&entry,budget-(uint32_t)sizeof(value));
            value.frames=1;
        }
        if(status){rf_particle_bitmap_close(&value);return status;}
        value.archive_index=i;value.resident_bytes=(uint32_t)sizeof(value)+value.image.bytes;
        *bitmap=value;return RF_OK;
    }
    return RF_NOT_FOUND;
}
int rf_corpse_surface_texture_open(rf_particle_bitmap *bitmap,rf_vpp *archives,
    uint32_t archive_count,uint32_t budget)
{
    static const rf_particle_definition definition={.bitmap="somenewblood_A.tga"};
    return rf_particle_bitmap_open(bitmap,&definition,archives,archive_count,0,budget);
}
char rf_material_failure_name[61];
uint32_t rf_material_failure[3]; /* status, archive index, entry size */
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
    memset(rf_material_failure_name,0,sizeof(rf_material_failure_name));
    memset(rf_material_failure,0,sizeof(rf_material_failure));
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
            result = rf_image_open(&item->image, archives + a, &entry, budget - m->allocated_bytes);
            if (result) {
                memcpy(rf_material_failure_name,name,strlen(name)+1);
                rf_material_failure[0]=(uint32_t)result;rf_material_failure[1]=a;rf_material_failure[2]=entry.size;
                goto fail;
            }
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
void rf_geometry_materials_close(rf_geometry_materials *m)
{
    if(!m)return;
    rf_materials_close(&m->textures);free(m->offsets);free(m->slots);
    memset(m,0,sizeof(*m));
}
int rf_geometry_materials_open(rf_geometry_materials *m,
    const rf_geometry *const *geometries,uint32_t count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    rf_geometry_materials next={0};char *storage=NULL;const char **names=NULL;
    uint64_t total=0,base,scratch;uint32_t i,j,at=0,unique=0;int status=RF_OK;
    if(!m || m->offsets || m->slots || m->textures.items || m->resident_bytes ||
        (!geometries && count) || (!archives && archive_count))return RF_RANGE;
    for(i=0;i<count;++i) {
        if(!geometries[i] || !geometries[i]->data)return RF_RANGE;
        total+=geometries[i]->textures;
    }
    base=sizeof(next)+((uint64_t)count+1)*sizeof(uint32_t)+total*sizeof(uint32_t);
    scratch=total*(61+sizeof(*names));
    if(total>UINT32_MAX || base+scratch>budget || base+scratch>SIZE_MAX)return RF_RANGE;
    next.offsets=malloc(((size_t)count+1)*sizeof(uint32_t));
    if(!next.offsets){status=RF_RANGE;goto done;}
    if(total) {
        next.slots=malloc((size_t)total*sizeof(uint32_t));
        storage=malloc((size_t)total*61);names=malloc((size_t)total*sizeof(*names));
        if(!next.slots || !storage || !names){status=RF_RANGE;goto done;}
    }
    next.count=count;
    for(i=0;i<count;++i) {
        next.offsets[i]=at;
        for(j=0;j<geometries[i]->textures;++j,++at) {
            uint32_t slot;char *name=storage+(size_t)unique*61;
            status=rf_geometry_texture_name(geometries[i],j,name,61);if(status)goto done;
            if(!*name){status=RF_FORMAT;goto done;}
            for(slot=0;slot<unique;++slot)if(equal_texture_name(name,names[slot]))break;
            if(slot==unique)names[unique++]=name;
            next.slots[at]=slot;
        }
    }
    next.offsets[count]=at;
    status=rf_materials_open_names(&next.textures,names,unique,archives,archive_count,
        budget-(uint32_t)(base+scratch));
    if(status)goto done;
    next.resident_bytes=(uint32_t)base+next.textures.allocated_bytes;
    next.peak_bytes=next.resident_bytes+(uint32_t)scratch;
done:
    free(storage);free(names);
    if(status)rf_geometry_materials_close(&next);else *m=next;
    return status;
}
void rf_level_particle_materials_close(rf_level_particle_materials *materials)
{
    uint32_t i;if(!materials)return;
    for(i=0;i<materials->texture_count;i++)rf_particle_animation_close(&materials->textures[i].animation);
    free(materials->storage);memset(materials,0,sizeof(*materials));
}
int rf_level_particle_materials_open(rf_level_particle_materials *materials,const rf_level *level,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    rf_level_particle_materials value={0};rf_level_emitter_reader reader;rf_level_emitter record;
    uint32_t i,slot;uint64_t bytes;int status;
    if(!materials || !level || (!archives && archive_count) || budget<sizeof(value))return RF_RANGE;
    status=rf_level_emitters_begin(level,&reader);
    if(status==RF_NOT_FOUND){value.resident_bytes=(uint32_t)sizeof(value);*materials=value;return RF_OK;}
    if(status)return status;if(reader.count>128)return RF_RANGE;
    value.count=reader.count;
    bytes=(uint64_t)value.count*(sizeof(*value.bindings)+sizeof(*value.textures));
    if(bytes+sizeof(value)>budget)return RF_RANGE;
    value.resident_bytes=(uint32_t)(bytes+sizeof(value));
    if(bytes) {
        value.storage=calloc(1,(size_t)bytes);if(!value.storage)return RF_RANGE;
        value.bindings=value.storage;value.textures=(rf_level_particle_texture *)(value.bindings+value.count);
    }
    for(i=0;i<value.count;i++) {
        rf_particle_definition definition={0};size_t length;
        status=rf_level_emitter_next(&reader,&record);if(status)goto failed;
        length=strlen(record.bitmap);if(!length || length>=sizeof(definition.bitmap)){status=RF_RANGE;goto failed;}
        for(slot=0;slot<value.texture_count;slot++)if(equal_texture_name(record.bitmap,value.textures[slot].name))break;
        if(slot==value.texture_count) {
            memcpy(definition.bitmap,record.bitmap,length+1);
            status=rf_particle_animation_open(&value.textures[slot].animation,&definition,archives,archive_count,
                budget-value.resident_bytes+(uint32_t)sizeof(rf_particle_animation));
            if(status)goto failed;
            memcpy(value.textures[slot].name,record.bitmap,length+1);value.texture_count++;
            value.resident_bytes+=value.textures[slot].animation.resident_bytes-(uint32_t)sizeof(rf_particle_animation);
        }
        value.bindings[i].uid=record.uid;value.bindings[i].texture=slot;
    }
    *materials=value;return RF_OK;
 failed:
    rf_level_particle_materials_close(&value);return status;
}

int rf_model_materials_open_skin(rf_model_materials *m,const rf_model_file *model,
    const char *const *primary_names,uint32_t primary_count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    rf_model_materials next={0}; uint8_t *raw=NULL;const char **names=NULL;int32_t *mapping=NULL;
    uint64_t count=0,base,scratch,used;uint32_t i,j,k,mesh=0,at=0,unique=0;int status=RF_OK;
    if (!m || !model || !model->archive || model->section_count>RF_MODEL_MAX_SECTIONS ||
        (!archives && archive_count) || m->items || m->textures.items || m->resident_bytes) return RF_RANGE;
    for(i=0;i<model->section_count;++i) if(model->sections[i].type==0x5355424d) count+=model->sections[i].material_count;
    if(primary_count && (!primary_names || primary_count!=count))return RF_RANGE;
    for(i=0;i<primary_count;++i) {
        size_t length=0;if(!primary_names[i])return RF_RANGE;
        while(length<32 && primary_names[i][length])++length;
        if(!length || length==32)return RF_RANGE;
    }
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
            if(primary_count) {
                memset(record,0,32);memcpy(record,primary_names[at],strlen(primary_names[at]));
            }
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
int rf_model_materials_open(rf_model_materials *m,const rf_model_file *model,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    return rf_model_materials_open_skin(m,model,NULL,0,archives,archive_count,budget);
}

void rf_entity_materials_close(rf_entity_materials *m)
{
    if(!m)return;
    rf_model_materials_close(&m->materials);free(m->offsets);memset(m,0,sizeof(*m));
}
int rf_entity_materials_open(rf_entity_materials *m,const rf_entity_appearances *appearances,
    const rf_entity_render_models *models,rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    rf_entity_materials next={0};uint8_t *raw=NULL;const char **names=NULL;int32_t *mapping=NULL;
    uint64_t count=0,base,scratch,used;uint32_t a,i,j,k,at=0,unique=0;int status=RF_OK;
    if(!m || !appearances || !models || (appearances->count && !appearances->items) ||
        (models->count && !models->items) || (!archives && archive_count) ||
        m->materials.items || m->materials.textures.items || m->offsets || m->resident_bytes)return RF_RANGE;
    for(a=0;a<appearances->count;++a) {
        const rf_entity_appearance *appearance=appearances->items+a;const rf_model_file *model;uint64_t n=0;
        if(appearance->skeleton>=models->count)return RF_RANGE;
        model=&models->items[appearance->skeleton].file;
        if(!model->archive || model->section_count>RF_MODEL_MAX_SECTIONS)return RF_RANGE;
        for(i=0;i<model->section_count;++i)if(model->sections[i].type==0x5355424d)n+=model->sections[i].material_count;
        if(appearance->texture_count && (!appearance->textures || appearance->texture_count!=n))return RF_RANGE;
        for(i=0;i<appearance->texture_count;++i) {
            const char *name=appearance->textures[i];
            if(!name[0] || !memchr(name,0,32))return RF_RANGE;
        }
        count+=n;if(count>INT32_MAX/2)return RF_RANGE;
    }
    base=sizeof(next)+((uint64_t)appearances->count+1)*sizeof(*next.offsets)+count*sizeof(*next.materials.items);
    scratch=count*(84+2*sizeof(*names)+2*sizeof(*mapping));used=base+scratch;
    if(count>INT32_MAX/2 || used>budget || used>SIZE_MAX)return RF_RANGE;
    next.offsets=calloc((size_t)appearances->count+1,sizeof(*next.offsets));
    if(!next.offsets)return RF_IO;
    next.count=appearances->count;
    if(count) {
        next.materials.items=calloc((size_t)count,sizeof(*next.materials.items));raw=malloc((size_t)count*84);
        names=malloc((size_t)count*2*sizeof(*names));mapping=malloc((size_t)count*2*sizeof(*mapping));
        if(!next.materials.items || !raw || !names || !mapping){status=RF_IO;goto done;}
    }
    next.materials.count=(uint32_t)count;
    for(a=0;a<appearances->count;++a) {
        const rf_entity_appearance *appearance=appearances->items+a;
        const rf_model_file *model=&models->items[appearance->skeleton].file;uint32_t mesh=0,local=0;
        next.offsets[a]=at;
        for(i=0;i<model->section_count;++i)if(model->sections[i].type==0x5355424d) {
            for(j=0;j<model->sections[i].material_count;++j,++at,++local) {
                uint8_t *record=raw+(size_t)at*84;
                status=rf_model_file_material(model,mesh,j,record);if(status)goto done;
                if(!record[0] || !memchr(record,0,32) || !memchr(record+48,0,32)){status=RF_FORMAT;goto done;}
                if(appearance->texture_count) {
                    memset(record,0,32);memcpy(record,appearance->textures[local],strlen(appearance->textures[local]));
                }
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
    }
    next.offsets[next.count]=at;
    status=rf_materials_open_names(&next.materials.textures,names,unique,archives,archive_count,budget-(uint32_t)used);
    if(status)goto done;
    if(next.materials.textures.missing){status=RF_NOT_FOUND;goto done;}
    used+=next.materials.textures.allocated_bytes;
    for(i=0;i<next.materials.count;++i) {
        rf_model_material_instance *item=next.materials.items+i;
        uint32_t alpha=(uint32_t)rf_image_format_has_alpha(next.materials.textures.items[mapping[i*2]].image.source_format);
        status=rf_model_material_from_disk(item,raw+(size_t)i*84,84,mapping[i*2],mapping[i*2+1],alpha,
            budget-(uint32_t)used+(uint32_t)sizeof(*item));if(status)goto done;
        used+=item->accounted_bytes-sizeof(*item);
    }
    next.peak_bytes=(uint32_t)used;next.resident_bytes=(uint32_t)(used-scratch);
    next.materials.resident_bytes=next.resident_bytes-(uint32_t)(base-count*sizeof(*next.materials.items))+sizeof(next.materials);
    next.materials.peak_bytes=next.materials.resident_bytes+(uint32_t)scratch;
 done:
    free(raw);free(names);free(mapping);
    if(status)rf_entity_materials_close(&next);else *m=next;
    return status;
}
