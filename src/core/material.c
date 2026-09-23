#include "rf/material.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
static int geometry_material_slot(const rf_geometry_materials *materials,uint32_t geometry_index,
    const rf_geometry *geometry,uint32_t face,uint32_t *slot)
{
    rf_geometry_face source;uint32_t first,last,value;int status;
    if(!materials || !geometry || !slot || !materials->offsets || geometry_index>=materials->count)return RF_RANGE;
    status=rf_geometry_get_face(geometry,face,&source);if(status)return status;
    if(source.texture==UINT32_MAX){*slot=UINT32_MAX;return RF_OK;}
    first=materials->offsets[geometry_index];last=materials->offsets[geometry_index+1];
    if(last<first || last-first!=geometry->textures || source.texture>=geometry->textures || !materials->slots)return RF_FORMAT;
    value=materials->slots[first+source.texture];if(value>=materials->textures.count || !materials->textures.items)return RF_FORMAT;
    *slot=value;return RF_OK;
}
int rf_geometry_material_shadow_images(const rf_geometry_materials *materials,uint32_t index,
    const rf_geometry *geometry,const rf_image **output,uint32_t capacity)
{
    uint32_t first,last,i;
    if(!materials || !geometry || !materials->offsets || index>=materials->count ||
       capacity<geometry->textures || (geometry->textures && !output))return RF_RANGE;
    first=materials->offsets[index];last=materials->offsets[index+1];
    if(last<first || last-first!=geometry->textures ||
       (geometry->textures && (!materials->slots || !materials->textures.items)))return RF_FORMAT;
    for(i=0;i<geometry->textures;i++) {
        uint32_t slot=materials->slots[first+i];int status;
        if(slot>=materials->textures.count)return RF_FORMAT;
        status=materials->textures.items[slot].status;
        if(status && status!=RF_NOT_FOUND)return status;
    }
    for(i=0;i<geometry->textures;i++) {
        const rf_material *item=materials->textures.items+materials->slots[first+i];
        output[i]=item->status==RF_NOT_FOUND?NULL:&item->image;
    }
    return RF_OK;
}

int rf_geometry_material_sample(const rf_geometry_materials *materials,uint32_t geometry_index,
    const rf_geometry *geometry,uint32_t face,const float point[3],
    rf_geometry_texture_workspace *work,uint32_t *color)
{
    uint32_t slot;const rf_material *item;int status;if(!color)return RF_RANGE;
    status=geometry_material_slot(materials,geometry_index,geometry,face,&slot);if(status)return status;
    if(slot==UINT32_MAX)return RF_NOT_FOUND;
    item=materials->textures.items+slot;if(item->status)return item->status;
    return rf_geometry_sample_texture(geometry,face,point,&item->image,work,color);
}
int rf_geometry_material_collision_sample(void *context,uint32_t index,const rf_collision_face *face,
    int32_t bitmap,const float point[3],uint32_t *color)
{
    const rf_geometry_material_collision *view=context;uint32_t source,slot;int status;
    if(!view || !face || !color || index>=view->face_count)return RF_RANGE;
    source=view->source_indices?view->source_indices[index]:index;
    status=geometry_material_slot(view->materials,view->geometry_index,view->geometry,source,&slot);if(status)return status;
    if(slot==UINT32_MAX)return RF_NOT_FOUND;
    if(slot>INT32_MAX || bitmap<0 || (uint32_t)bitmap!=slot)return RF_FORMAT;
    return rf_geometry_material_sample(view->materials,view->geometry_index,view->geometry,source,point,view->work,color);
}
int rf_geometry_material_collision_bind(const rf_geometry_material_collision *view,
    int32_t *bitmaps,uint32_t capacity,rf_collision_indexed_texture_backend *backend)
{
    uint32_t i,source,slot;int status;
    if(!view || !view->materials || !view->geometry || !view->work || !backend ||
        capacity<view->face_count || (view->face_count && !bitmaps))return RF_RANGE;
    for(i=0;i<view->face_count;++i) {
        source=view->source_indices?view->source_indices[i]:i;
        status=geometry_material_slot(view->materials,view->geometry_index,view->geometry,source,&slot);if(status)return status;
        if(slot!=UINT32_MAX && slot>INT32_MAX)return RF_RANGE;
        bitmaps[i]=(int32_t)slot;
    }
    backend->bitmaps=bitmaps;backend->sample=rf_geometry_material_collision_sample;backend->context=(void *)view;
    return RF_OK;
}

static int runtime_view_valid(const rf_geometry_material_runtime *v) {
    return v && v->base && v->base->geometry && v->base->materials && v->count<=1024 && (!v->count || v->surfaces);
}
static const rf_geometry_runtime_surface *runtime_surface(const rf_geometry_material_runtime *v,uint32_t id) {
    uint32_t i;for(i=0;i<v->count;i++)if(v->surfaces[i].id==id)return v->surfaces+i;return NULL;
}
int rf_geometry_material_runtime_lookup(const rf_geometry_material_runtime *v,uint32_t id,uint32_t *texture,uint32_t *slot) {
    const rf_geometry_material_collision *b;const rf_geometry_runtime_surface *r;
    const rf_geometry_materials *m;uint32_t first,last,t,value;int status;
    if(!runtime_view_valid(v) || !texture || !slot)return RF_RANGE;b=v->base;m=b->materials;
    if(id<b->geometry->faces) {
        rf_geometry_face f;status=rf_geometry_get_face(b->geometry,id,&f);if(status)return status;t=f.texture;
    } else {
        if(id==UINT32_MAX)return RF_NOT_FOUND;
        r=runtime_surface(v,id);if(!r)return RF_NOT_FOUND;t=r->texture;
    }
    if(!m->offsets || b->geometry_index>=m->count)return RF_RANGE;
    if(t==UINT32_MAX){*texture=t;*slot=UINT32_MAX;return RF_OK;}
    first=m->offsets[b->geometry_index];last=m->offsets[b->geometry_index+1];
    if(last<first || last-first!=b->geometry->textures || t>=b->geometry->textures || !m->slots)return RF_FORMAT;
    value=m->slots[first+t];if(value>=m->textures.count || !m->textures.items)return RF_FORMAT;
    *texture=t;*slot=value;return RF_OK;
}
int rf_geometry_material_runtime_sample(void *context,uint32_t index,const rf_collision_face *face,
    int32_t bitmap,const float point[3],uint32_t *color) {
    const rf_geometry_material_runtime *v=context;const rf_geometry_material_collision *b;
    const rf_geometry_runtime_surface *r;const rf_material *m;uint32_t id,texture,slot,matched;float uv[2];int status;
    if(!runtime_view_valid(v) || !face || !point || !color)return RF_RANGE;b=v->base;
    if(index>=b->face_count)return RF_RANGE;id=b->source_indices?b->source_indices[index]:index;
    if(id<b->geometry->faces)return rf_geometry_material_collision_sample((void *)b,index,face,bitmap,point,color);
    status=rf_geometry_material_runtime_lookup(v,id,&texture,&slot);if(status)return status;
    if(slot==UINT32_MAX)return RF_NOT_FOUND;
    if(slot>INT32_MAX || bitmap<0 || (uint32_t)bitmap!=slot)return RF_FORMAT;
    r=runtime_surface(v,id);if(!r)return RF_NOT_FOUND;
    m=b->materials->textures.items+slot;if(m->status)return m->status;
    status=rf_collision_texture_coordinates(r->plane,point,r->vertices,r->uv,r->count,uv,&matched);if(status)return status;
    if(!matched)return RF_NOT_FOUND;
    return rf_image_sample_owned(&m->image,uv[0],uv[1],color);
}
int rf_geometry_material_runtime_bind(const rf_geometry_material_runtime *v,int32_t *bitmaps,
    uint32_t capacity,rf_collision_indexed_texture_backend *backend) {
    const rf_geometry_material_collision *b;uint32_t i,j,k,texture,slot;int status;
    if(!runtime_view_valid(v) || !backend)return RF_RANGE;b=v->base;
    if(!b->work || capacity<b->face_count || (b->face_count && !bitmaps))return RF_RANGE;
    for(i=0;i<v->count;i++) {
        const rf_geometry_runtime_surface *r=v->surfaces+i;double norm=0;
        if(r->id<b->geometry->faces || r->id==UINT32_MAX || r->count<3 || r->count>256 || !r->vertices || !r->uv)return RF_FORMAT;
        for(j=0;j<i;j++)if(v->surfaces[j].id==r->id)return RF_FORMAT;
        for(j=0;j<4;j++){if(!isfinite(r->plane[j]))return RF_FORMAT;if(j<3)norm+=(double)r->plane[j]*r->plane[j];}
        if(fabs(norm-1)>1e-4)return RF_FORMAT;
        for(j=0;j<r->count;j++) {
            for(k=0;k<3;k++)if(!isfinite(r->vertices[j][k]))return RF_FORMAT;
            for(k=0;k<2;k++)if(!isfinite(r->uv[j][k]))return RF_FORMAT;
        }
        status=rf_geometry_material_runtime_lookup(v,r->id,&texture,&slot);if(status)return status;
    }
    for(i=0;i<b->face_count;i++) {
        uint32_t id=b->source_indices?b->source_indices[i]:i;
        status=rf_geometry_material_runtime_lookup(v,id,&texture,&slot);if(status)return status;
        if(slot!=UINT32_MAX && slot>INT32_MAX)return RF_RANGE;bitmaps[i]=(int32_t)slot;
    }
    backend->bitmaps=bitmaps;backend->sample=rf_geometry_material_runtime_sample;backend->context=(void *)v;return RF_OK;
}

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
static int equal_texture_name(const char *a,const char *b);
static int open_materials(rf_materials *m,const rf_geometry *g,const char *const *names,uint32_t count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget,uint32_t limit,uint32_t *peak)
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
    m->count = count; m->allocated_bytes = (uint32_t)slots;if(peak)*peak=(uint32_t)slots;
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
            if(limit && (item->image.width>limit || item->image.height>limit)) {
                uint32_t old=item->image.bytes;
                result=rf_image_reduce(&item->image,limit,budget-m->allocated_bytes);if(result)goto fail;
                if(peak && m->allocated_bytes+old+item->image.bytes>*peak)*peak=m->allocated_bytes+old+item->image.bytes;
            } else if(peak && m->allocated_bytes+item->image.bytes>*peak)*peak=m->allocated_bytes+item->image.bytes;
            item->archive_index = a; item->status = RF_OK;
            m->allocated_bytes += item->image.bytes; ++m->loaded;
            break;
        }
        if(item->status==RF_NOT_FOUND && equal_texture_name(name,"USERBMAP")) {
            /* Ordinary uncached USERBMAP:50f6e0 ->510470. Runtime bitmap
             * replacement remains the caller's responsibility. */
            result=rf_image_missing(&item->image,budget-m->allocated_bytes);
            if(result)goto fail;
            item->status=RF_OK;m->allocated_bytes+=item->image.bytes;++m->loaded;
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
    return open_materials(m,g,NULL,g->textures,archives,archive_count,budget,0,NULL);
}
int rf_materials_open_names(rf_materials *m,const char *const *names,uint32_t count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    return open_materials(m,NULL,names,count,archives,archive_count,budget,0,NULL);
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
void rf_glare_materials_close(rf_glare_materials *m)
{
    uint32_t i;if(!m)return;
    for(i=0;i<m->texture_count;++i)rf_particle_animation_close(&m->textures[i].animation);
    free(m->storage);memset(m,0,sizeof(*m));
}
int rf_glare_materials_open(rf_glare_materials *m,const rf_glare_definition *definitions,
    uint32_t count,rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    rf_glare_materials next={0};uint32_t i,j,slot;uint64_t bytes;int status;
    if(!m || m->storage || m->bindings || m->textures || m->count || m->texture_count || m->resident_bytes ||
       (!definitions && count) || (!archives && archive_count) || count>64)return RF_RANGE;
    bytes=sizeof(next)+(uint64_t)count*(sizeof(*next.bindings)+3*sizeof(*next.textures));
    if(bytes>budget)return RF_RANGE;
    if(count) {
        next.storage=calloc(1,(size_t)(bytes-sizeof(next)));if(!next.storage)return RF_IO;
        next.bindings=next.storage;next.textures=(rf_level_particle_texture *)(next.bindings+count);
    }
    next.count=count;next.resident_bytes=(uint32_t)bytes;
    for(i=0;i<count;++i) {
        const char *names[]={definitions[i].corona,definitions[i].volumetric,definitions[i].reflection};
        if(definitions[i].fields&~7u){status=RF_FORMAT;goto failed;}
        for(j=0;j<3;++j) {
            next.bindings[i][j]=UINT32_MAX;if(!(definitions[i].fields&(1u<<j)))continue;
            if(!names[j][0] || !memchr(names[j],0,64)){status=RF_FORMAT;goto failed;}
            for(slot=0;slot<next.texture_count;++slot)if(equal_texture_name(names[j],next.textures[slot].name))break;
            if(slot==next.texture_count){strcpy(next.textures[slot].name,names[j]);++next.texture_count;}
            next.bindings[i][j]=slot;
        }
    }
    for(i=0;i<next.texture_count;++i) {
        rf_particle_definition definition={0};rf_particle_animation *animation=&next.textures[i].animation;
        strcpy(definition.bitmap,next.textures[i].name);
        status=rf_particle_animation_open(animation,&definition,archives,archive_count,
            budget-next.resident_bytes+(uint32_t)sizeof(*animation));
        if(status)goto failed;
        next.resident_bytes+=animation->resident_bytes-(uint32_t)sizeof(*animation);
    }
    *m=next;return RF_OK;
failed:
    rf_glare_materials_close(&next);return status;
}
void rf_geometry_materials_close(rf_geometry_materials *m)
{
    if(!m)return;
    rf_materials_close(&m->textures);free(m->offsets);free(m->slots);
    memset(m,0,sizeof(*m));
}
int rf_geometry_materials_open_limit(rf_geometry_materials *m,
    const rf_geometry *const *geometries,uint32_t count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget,uint32_t limit)
{
    rf_geometry_materials next={0};char *storage=NULL;const char **names=NULL;
    uint64_t total=0,base,scratch;uint32_t i,j,at=0,unique=0,texture_peak=0;int status=RF_OK;
    if(limit>4096 || (limit && (limit&(limit-1))))return RF_RANGE;
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
    status=open_materials(&next.textures,NULL,names,unique,archives,archive_count,
        budget-(uint32_t)(base+scratch),limit,&texture_peak);
    if(status)goto done;
    next.resident_bytes=(uint32_t)base+next.textures.allocated_bytes;
    next.peak_bytes=(uint32_t)(base+scratch)+texture_peak;
done:
    free(storage);free(names);
    if(status)rf_geometry_materials_close(&next);else *m=next;
    return status;
}
int rf_geometry_materials_open(rf_geometry_materials *m,const rf_geometry *const *geometries,uint32_t count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{return rf_geometry_materials_open_limit(m,geometries,count,archives,archive_count,budget,0);}
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

static int model_materials_open_source(rf_model_materials *m,const rf_model_file *model,
    const uint8_t (*records)[84],uint32_t record_count,
    const char *const *primary_names,uint32_t primary_count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget,const char *const *overrides,uint32_t limit)
{
    rf_model_materials next={0}; uint8_t *raw=NULL;const char **names=NULL;int32_t *mapping=NULL;
    uint64_t count=0,base,scratch,used;uint32_t i,j,k,mesh=0,at=0,unique=0,texture_peak=0,load_peak=0;int status=RF_OK;
    if(limit>4096 || (limit && (limit&(limit-1))))return RF_RANGE;
    if (!m || (model && (!model->archive || model->section_count>RF_MODEL_MAX_SECTIONS)) ||
        (!model && record_count && !records) ||
        (!archives && archive_count) || m->items || m->textures.items || m->resident_bytes) return RF_RANGE;
    if(model) {for(i=0;i<model->section_count;++i) if(model->sections[i].type==0x5355424d) count+=model->sections[i].material_count;}
    else count=record_count;
    if(primary_count && (!primary_names || primary_count!=count))return RF_RANGE;
    for(i=0;i<primary_count;++i) {
        size_t length=0;if(!primary_names[i])return RF_RANGE;
        while(length<32 && primary_names[i][length])++length;
        if(!length || length==32)return RF_RANGE;
    }
    if(overrides)for(i=0;i<count;++i)if(overrides[i]) {
        size_t length=0;while(length<61 && overrides[i][length])++length;
        if(!length || length>60)return RF_RANGE;
    }
    base=sizeof(next)+count*sizeof(*next.items);
    scratch=count*(84+(overrides?3:2)*(sizeof(*names)+sizeof(*mapping)));used=base+scratch;
    if(count>INT32_MAX/(overrides?3:2) || used>budget || used>SIZE_MAX) return RF_RANGE;
    if(count) {
        next.items=calloc((size_t)count,sizeof(*next.items));raw=malloc((size_t)count*84);
        names=malloc((size_t)count*(overrides?3:2)*sizeof(*names));mapping=malloc((size_t)count*(overrides?3:2)*sizeof(*mapping));
        if(!next.items || !raw || !names || !mapping) { status=RF_IO;goto done; }
    }
    next.count=(uint32_t)count;
    if(model) {
        for(i=0;i<model->section_count;++i) if(model->sections[i].type==0x5355424d) {
            for(j=0;j<model->sections[i].material_count;++j,++at) {
                status=rf_model_file_material(model,mesh,j,raw+(size_t)at*84);if(status)goto done;
            }
            ++mesh;
        }
    } else if(count)memcpy(raw,records,(size_t)count*84);
    for(at=0;at<count;++at) {
        uint8_t *record=raw+(size_t)at*84;
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
    /*410d30 changes only the primary handle after material creation. Keep base
     * texture conversion/alpha flags and secondary handles unchanged. */
    if(overrides)for(i=0;i<count;++i) {
        uint32_t slot;mapping[count*2+i]=-1;if(!overrides[i])continue;
        for(slot=0;slot<unique;++slot)if(equal_texture_name(overrides[i],names[slot]))break;
        if(slot==unique)names[unique++]=overrides[i];mapping[count*2+i]=(int32_t)slot;
    }
    status=open_materials(&next.textures,NULL,names,unique,archives,archive_count,budget-(uint32_t)used,limit,&texture_peak);
    load_peak=(uint32_t)used+texture_peak;
    if(status)goto done;
    if(next.textures.missing) { status=RF_NOT_FOUND;goto done; }
    used+=next.textures.allocated_bytes;
    for(i=0;i<next.count;++i) {
        uint32_t alpha=(uint32_t)rf_image_format_has_alpha(next.textures.items[mapping[i*2]].image.source_format);
        status=rf_model_material_from_disk(next.items+i,raw+(size_t)i*84,84,mapping[i*2],mapping[i*2+1],alpha,
            budget-(uint32_t)used+(uint32_t)sizeof(*next.items));
        if(status)goto done;
        if(overrides && mapping[count*2+i]>=0)memcpy(next.items[i].record.bytes+0x10,mapping+count*2+i,4);
        used+=next.items[i].accounted_bytes-sizeof(*next.items);
    }
    next.peak_bytes=load_peak>used?load_peak:(uint32_t)used;next.resident_bytes=(uint32_t)(used-scratch);
done:
    free(raw);free(names);free(mapping);
    if(status)rf_model_materials_close(&next);else *m=next;
    return status;
}
int rf_model_materials_open_skin(rf_model_materials *m,const rf_model_file *model,
    const char *const *primary_names,uint32_t primary_count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    if(!model)return RF_RANGE;
    return model_materials_open_source(m,model,NULL,0,primary_names,primary_count,archives,archive_count,budget,NULL,0);
}
int rf_model_materials_open_records(rf_model_materials *m,const uint8_t (*records)[84],uint32_t count,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    return model_materials_open_source(m,NULL,records,count,NULL,0,archives,archive_count,budget,NULL,0);
}

int rf_model_materials_open_records_overrides(rf_model_materials *m,const uint8_t (*records)[84],uint32_t count,
    const char *const *overrides,rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    return model_materials_open_source(m,NULL,records,count,NULL,0,archives,archive_count,budget,overrides,0);
}

int rf_model_materials_open_records_overrides_limit(rf_model_materials *m,const uint8_t (*records)[84],uint32_t count,
    const char *const *overrides,rf_vpp *archives,uint32_t archive_count,uint32_t budget,uint32_t limit)
{return model_materials_open_source(m,NULL,records,count,NULL,0,archives,archive_count,budget,overrides,limit);}

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
{return rf_entity_materials_open_limit(m,appearances,models,archives,archive_count,budget,0);}
int rf_entity_materials_open_limit(rf_entity_materials *m,const rf_entity_appearances *appearances,
    const rf_entity_render_models *models,rf_vpp *archives,uint32_t archive_count,uint32_t budget,uint32_t limit)
{
    rf_entity_materials next={0};uint8_t *raw=NULL;const char **names=NULL;int32_t *mapping=NULL;
    uint64_t count=0,base,scratch,used;uint32_t a,i,j,k,at=0,unique=0,texture_peak=0,load_peak=0;int status=RF_OK;
    if(limit>4096 || (limit && (limit&(limit-1))))return RF_RANGE;
    if(!m || !appearances || !models || (appearances->count && !appearances->items) ||
        (models->count && !models->items) || (!archives && archive_count) ||
        m->materials.items || m->materials.textures.items || m->offsets || m->resident_bytes)return RF_RANGE;
    for(a=0;a<appearances->count;++a) {
        const rf_entity_appearance *appearance=appearances->items+a;const rf_model_file *model;uint64_t n=0;
        if(appearance->skeleton>=models->count)return RF_RANGE;
        model=&models->items[appearance->skeleton].file;
        if(!model->archive || model->section_count>RF_MODEL_MAX_SECTIONS)return RF_RANGE;
        for(i=0;i<model->section_count;++i)if(model->sections[i].type==0x5355424d)n+=model->sections[i].material_count;
        /* Some authored skins replace only the leading high-detail materials.
         * eos.v3c has three class overrides followed by six separate -mip2/-mip3
         * records; those later records retain their model-authored textures. */
        if(appearance->texture_count && (!appearance->textures || appearance->texture_count>n))return RF_RANGE;
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
                if(local<appearance->texture_count) {
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
    status=open_materials(&next.materials.textures,NULL,names,unique,archives,archive_count,budget-(uint32_t)used,limit,&texture_peak);
    load_peak=(uint32_t)used+texture_peak;
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
    next.peak_bytes=load_peak>used?load_peak:(uint32_t)used;next.resident_bytes=(uint32_t)(used-scratch);
    next.materials.resident_bytes=next.resident_bytes-(uint32_t)(base-count*sizeof(*next.materials.items))+sizeof(next.materials);
    next.materials.peak_bytes=next.materials.resident_bytes+next.peak_bytes-next.resident_bytes;
 done:
    free(raw);free(names);free(mapping);
    if(status)rf_entity_materials_close(&next);else *m=next;
    return status;
}

void rf_vfx_material_textures_close(rf_vfx_material_textures *m)
{rf_glare_materials_close(m);}
int rf_vfx_material_textures_open(rf_vfx_material_textures *m,const rf_vfx_material_view *views,
    uint32_t count,rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    rf_vfx_material_textures next={0},empty={0};uint64_t bytes;uint32_t i,j,slot;int status;
    static const uint32_t offsets[3]={5,18,36};
    if(!m || memcmp(m,&empty,sizeof(empty)) || (!views && count) || (!archives && archive_count))return RF_RANGE;
    bytes=sizeof(next)+(uint64_t)count*(sizeof(*next.bindings)+3*sizeof(*next.textures));if(bytes>budget)return RF_RANGE;
    if(count){next.storage=calloc(1,(size_t)(bytes-sizeof(next)));if(!next.storage)return RF_IO;next.bindings=next.storage;next.textures=(rf_level_particle_texture *)(next.bindings+count);}
    next.count=count;next.resident_bytes=(uint32_t)bytes;
    for(i=0;i<count;++i) {
        if(views[i].bitmap_requests&~7u){status=RF_FORMAT;goto failed;}
        for(j=0;j<3;++j) {
            const char *name=(const char *)(views[i].words+offsets[j]);next.bindings[i][j]=UINT32_MAX;
            if(!(views[i].bitmap_requests&(1u<<j)))continue;
            if(!memchr(name,0,33)){status=RF_FORMAT;goto failed;}if(!*name)continue;
            for(slot=0;slot<next.texture_count;++slot)if(equal_texture_name(name,next.textures[slot].name))break;
            if(slot==next.texture_count){strcpy(next.textures[slot].name,name);++next.texture_count;}next.bindings[i][j]=slot;
        }
    }
    for(i=0;i<next.texture_count;++i) {
        rf_particle_definition definition={0};rf_particle_animation *animation=&next.textures[i].animation;
        strcpy(definition.bitmap,next.textures[i].name);
        status=rf_particle_animation_open(animation,&definition,archives,archive_count,budget-next.resident_bytes+(uint32_t)sizeof(*animation));
        if(status==RF_NOT_FOUND) {
            for(slot=0;slot<count;++slot)for(j=0;j<3;++j)if(next.bindings[slot][j]==i)next.bindings[slot][j]=UINT32_MAX;
            continue;
        }
        if(status)goto failed;next.resident_bytes+=animation->resident_bytes-(uint32_t)sizeof(*animation);
    }
    *m=next;return RF_OK;
failed:
    rf_vfx_material_textures_close(&next);return status;
}

int rf_vfx_material_texture_sample(const rf_vfx_material_textures *textures,const rf_vfx_material_view *view,
    uint32_t material,uint32_t slot,float time,uint32_t normalized,const rf_image **out)
{
    const rf_particle_animation *animation;uint32_t index,frame,field;int32_t start;float speed,duration;int status;
    if(!textures || !view || !out || slot>1 || normalized>1)return RF_RANGE;
    if(material>=textures->count)return RF_NOT_FOUND;
    if(!textures->bindings)return RF_RANGE;index=textures->bindings[material][slot];
    if(index==UINT32_MAX)return RF_NOT_FOUND;
    if(index>=textures->texture_count || !textures->textures)return RF_RANGE;
    animation=&textures->textures[index].animation;
    if(!animation->images || !animation->count)return RF_RANGE;
    status=rf_vfx_texture_duration(animation->count,animation->rate,&duration);if(status)return status;
    field=slot?27:14;memcpy(&start,view->words+field,4);memcpy(&speed,view->words+field+1,4);
    status=rf_vfx_texture_frame(animation->count,duration,start,speed,view->words[field+2],time,normalized,&frame);
    if(status)return status;*out=animation->images+frame;return RF_OK;
}

void rf_vfx_asset_materials_close(rf_vfx_asset_materials **out)
{
    rf_vfx_asset_materials *m;if(!out || !(m=*out))return;
    rf_vfx_material_textures_close(&m->textures);free(m);*out=NULL;
}
int rf_vfx_asset_materials_open(const rf_vfx_geometry_asset *asset,rf_vpp *maps,uint32_t map_count,
    uint32_t budget,rf_vfx_asset_materials **out)
{
    rf_vfx_asset_materials *m;uint32_t i,j,at;int status;
    if(!asset || !maps || !map_count || !out || *out || asset->count>RF_VFX_ASSET_MESH_CAPACITY || budget<sizeof(*m))return RF_RANGE;
    m=calloc(1,sizeof(*m));if(!m)return RF_IO;
    if(asset->version>=0x40000) {
        const rf_vfx_material_bank *bank=asset->material_bank;
        if(!bank || !bank->views || bank->count>64){status=RF_FORMAT;goto done;}
        m->count=bank->count;
        for(i=0;i<m->count;i++){m->views[i]=bank->views[i];m->colors[i]=0xffffffffu;}
    }
    for(i=0;i<asset->count;i++) {
        const rf_vfx_mesh *mesh=asset->meshes[i];
        if(!mesh){status=RF_FORMAT;goto done;}
        m->first[i]=m->count;at=mesh->material_offset;
        if(mesh->version>=0x40000) {
            const rf_vfx_material_bank *bank=asset->material_bank;
            if(!bank || !bank->views || !mesh->data || (uint64_t)at+(uint64_t)mesh->materials*4>mesh->bytes){status=RF_FORMAT;goto done;}
            for(j=0;j<mesh->materials;j++) {
                const unsigned char *p=mesh->data+at+j*4;
                uint32_t id=(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
                if(id>=bank->count){status=RF_FORMAT;goto done;}
            }
            continue;
        }
        if(mesh->materials>64-m->count){status=RF_RANGE;goto done;}
        for(j=0;j<mesh->materials;j++) {
            rf_vfx_embedded_material_view view;
            if(at>mesh->bytes){status=RF_FORMAT;goto done;}
            status=rf_vfx_embedded_material_read(mesh->data+at,mesh->bytes-at,mesh->version,
                mesh->prefix.timing.flags,mesh->prefix.timing.samples,&view);if(status)goto done;
            m->views[m->count]=view.material;m->colors[m->count]=view.color_word;++m->count;at+=view.material.bytes;
        }
    }
    m->first[asset->count]=m->count;
    status=rf_vfx_material_textures_open(&m->textures,m->views,m->count,maps,map_count,
        budget-(uint32_t)sizeof(*m)+(uint32_t)sizeof(m->textures));if(status)goto done;
    m->resident_bytes=(uint32_t)sizeof(*m)+m->textures.resident_bytes-(uint32_t)sizeof(m->textures);
    *out=m;m=NULL;status=RF_OK;
done:
    rf_vfx_asset_materials_close(&m);return status;
}

int rf_vfx_asset_material_index(const rf_vfx_geometry_asset *asset,const rf_vfx_asset_materials *materials,
    uint32_t mesh_index,uint32_t local_material,uint32_t *index)
{
    const rf_vfx_mesh *mesh;uint32_t id;
    if(!asset || !materials || !index || asset->count>RF_VFX_ASSET_MESH_CAPACITY || mesh_index>=asset->count)return RF_RANGE;
    mesh=asset->meshes[mesh_index];if(!mesh || local_material>=mesh->materials)return RF_FORMAT;
    if(mesh->version>=0x40000) {
        const unsigned char *p;uint64_t offset=(uint64_t)mesh->material_offset+(uint64_t)local_material*4;
        if(!mesh->data || offset+4>mesh->bytes)return RF_FORMAT;
        p=mesh->data+(uint32_t)offset;
        id=(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
    } else {
        if(materials->first[mesh_index]>materials->count || local_material>=materials->count-materials->first[mesh_index])return RF_FORMAT;
        id=materials->first[mesh_index]+local_material;
    }
    if(id>=materials->count)return RF_FORMAT;
    *index=id;return RF_OK;
}

void rf_explosion_materials_close(rf_explosion_materials *owner)
{
    uint32_t i;if(!owner)return;
    for(i=0;i<owner->count;i++)rf_particle_animation_close(owner->animations+i);
    memset(owner,0,sizeof(*owner));
}
int rf_explosion_materials_open(rf_explosion_materials *out,const rf_explosion_definition *definition,
    rf_vpp *archives,uint32_t archive_count,uint32_t budget)
{
    rf_explosion_materials value={0};uint32_t i,j,slot,source[9];int status;
    if(!out || !definition || !archives || !archive_count || out->count || out->resident_bytes ||
       !definition->resolved || (definition->resolved&~511u) || budget<sizeof(value))return RF_RANGE;
    for(i=0;i<9;i++)if(out->animations[i].images || out->animations[i].count)return RF_RANGE;
    value.resident_bytes=sizeof(value);
    for(i=0;i<9;i++)value.slot_texture[i]=UINT32_MAX;
    for(i=0;i<9;i++)if(definition->resolved&(1u<<i)) {
        const rf_particle_definition *particle=definition->emitters+i;
        if(!memchr(particle->bitmap,0,sizeof(particle->bitmap)) || !particle->bitmap[0]){status=RF_FORMAT;goto failed;}
        for(j=0;j<value.count;j++)if(equal_texture_name(particle->bitmap,definition->emitters[source[j]].bitmap))break;
        slot=j;
        if(slot==value.count) {
            rf_particle_animation *animation=value.animations+slot;
            status=rf_particle_animation_open(animation,particle,archives,archive_count,
                budget-value.resident_bytes+(uint32_t)sizeof(*animation));if(status)goto failed;
            value.resident_bytes+=animation->resident_bytes-(uint32_t)sizeof(*animation);
            source[slot]=i;value.count++;
        }
        value.slot_texture[i]=slot;
    }
    *out=value;return RF_OK;
failed:
    rf_explosion_materials_close(&value);return status;
}
