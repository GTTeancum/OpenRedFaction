#include "rf/model_file.h"
#include "rf/model.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
int rf_model_compiled_filename(const char *authored,char compiled[64],const char extension[5])
{
    uint32_t length=0,stem=0,i;int dot=0;
    if(!authored || !compiled || !extension || extension[0]!='.')return RF_RANGE;
    for(i=1;i<4;++i)if(!extension[i])return RF_RANGE;
    if(extension[4])return RF_RANGE;
    while(length<64 && authored[length]){if(authored[length]=='.'){stem=length;dot=1;}++length;}
    if(length==64)return RF_RANGE;if(!dot)stem=length;if(stem>59)return RF_RANGE;
    memmove(compiled,authored,stem);memcpy(compiled+stem,extension,5);return RF_OK;
}
int rf_static_model_metadata_open(rf_vpp *archive,const char *authored,uint32_t budget,rf_static_model_metadata *owner)
{
    rf_static_model_metadata value={0};rf_model_file *file;uint64_t peak;uint32_t i;int status;
    if(!archive || !owner || owner->filename[0] || owner->spheres || owner->count || owner->allocated_bytes || owner->peak_bytes)return RF_RANGE;
    if((uint64_t)sizeof(value)+sizeof(*file)>budget)return RF_RANGE;
    status=rf_model_compiled_filename(authored,value.filename,".v3m");if(status)return status;
    file=malloc(sizeof(*file));if(!file)return RF_IO;
    status=rf_model_file_open(file,archive,value.filename);if(status)goto done;
    status=rf_model_file_static_bound_sphere(file,value.bound);if(status)goto done;
    for(i=0;i<file->section_count;++i)if(file->sections[i].type==0x43535048)++value.count;
    peak=sizeof(value)+(uint64_t)sizeof(*file)+(uint64_t)value.count*sizeof(*value.spheres);
    if(peak>budget){status=RF_RANGE;goto done;}
    if(value.count){value.spheres=malloc(value.count*sizeof(*value.spheres));if(!value.spheres){status=RF_IO;goto done;}}
    for(i=0;i<value.count;++i){status=rf_model_file_collision_sphere(file,i,value.spheres+i);if(status)goto done;}
    value.allocated_bytes=sizeof(value)+value.count*sizeof(*value.spheres);value.peak_bytes=(uint32_t)peak;
    *owner=value;value.spheres=NULL;
done:
    free(value.spheres);free(file);return status;
}
void rf_static_model_metadata_close(rf_static_model_metadata *owner)
{if(owner){free(owner->spheres);memset(owner,0,sizeof(*owner));}}
typedef struct reader { rf_model_file *model; uint32_t cursor; int status; } reader;
int rf_model_collision_sphere_pose(const rf_model_collision_sphere *sphere,
    const float (*matrices)[12],uint32_t bones,float result[4])
{
    static const float identity[12]={1,0,0,0,1,0,0,0,1,0,0,0};
    const float *m;float value[4];double x,y,z;uint32_t i;
    if(!sphere || !result || sphere->parent<-1 || !isfinite(sphere->radius) || sphere->radius<0)return RF_RANGE;
    if(sphere->parent>=0 && (!matrices || (uint32_t)sphere->parent>=bones))return RF_RANGE;
    m=sphere->parent<0?identity:matrices[sphere->parent];
    for(i=0;i<12;++i)if(!isfinite(m[i]))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(sphere->center[i]))return RF_RANGE;
    x=sphere->center[0];y=sphere->center[1];z=sphere->center[2];
    value[0]=(float)(((m[3]*y+m[6]*z)+m[0]*x)+m[9]);
    for(i=1;i<3;++i)value[i]=(float)(((m[3+i]*y+m[i]*x)+m[6+i]*z)+m[9+i]);
    for(i=0;i<3;++i)if(!isfinite(value[i]))return RF_RANGE;
    value[3]=sphere->radius;memcpy(result,value,sizeof(value));return RF_OK;
}
int rf_model_creation_spheres(const rf_model_collision_sphere *models,uint32_t count,
    uint32_t kind,const float (*matrices)[12],uint32_t bones,
    rf_physics_sphere *spheres,uint32_t capacity)
{
    uint32_t i,j;float value[4];int status;
    if(count>capacity || (count && (!models || !spheres)) || (kind!=1 && kind!=2))return RF_RANGE;
    for(i=0;i<count;++i) {
        if(kind==2){status=rf_model_collision_sphere_pose(models+i,matrices,bones,value);if(status)return status;}
        else {
            memcpy(value,models[i].center,12);value[3]=models[i].radius;
            for(j=0;j<4;++j)if(!isfinite(value[j]))return RF_RANGE;
            if(value[3]<0)return RF_RANGE;
        }
        if(count==1)value[0]=value[1]=value[2]=0;
        memcpy(spheres[i].center,value,16);
    }
    return RF_OK;
}
int rf_model_corpse_spheres_refresh(const rf_model_collision_sphere *models,uint32_t model_count,
    const float (*matrices)[12],uint32_t bones,rf_physics_sphere *spheres,uint32_t sphere_count,
    const float position[3],rf_physics_bounds *bounds,float *radius)
{
    uint32_t i;int status;float posed[4];rf_physics_bounds value;
    if((model_count && !models) || (sphere_count && !spheres) || model_count>sphere_count ||
       !position || !bounds || !radius)return RF_RANGE;
    for(i=0;i<model_count;i++) {
        status=rf_model_collision_sphere_pose(models+i,matrices,bones,posed);if(status)return status;
        memcpy(spheres[i].center,posed,12);
    }
    status=rf_physics_spheres_bounds(spheres,sphere_count,position,&value);if(status)return status;
    *bounds=value;*radius=value.radius;return RF_OK;
}
int rf_model_file_collision_sphere(const rf_model_file *model,uint32_t index,rf_model_collision_sphere *sphere)
{
    uint32_t i,j;uint8_t raw[44];rf_model_collision_sphere value={0};int status;
    if(!model || !model->archive || !sphere || model->section_count>RF_MODEL_MAX_SECTIONS)return RF_RANGE;
    for(i=0;i<model->section_count;++i) {
        const rf_model_section *section=model->sections+i;
        if(section->type!=0x43535048)continue;
        if(index) {--index;continue;}
        if(section->size!=sizeof(raw) || (uint64_t)section->offset+sizeof(raw)>model->entry.size)return RF_FORMAT;
        status=rf_vpp_read(model->archive,&model->entry,section->offset,raw,sizeof(raw));if(status)return status;
        memcpy(value.name,raw,24);memcpy(&value.parent,raw+24,4);
        memcpy(value.center,raw+28,12);memcpy(&value.radius,raw+40,4);
        if(value.parent<-1 || !isfinite(value.radius) || value.radius<0)return RF_FORMAT;
        for(j=0;j<3;++j)if(!isfinite(value.center[j]))return RF_FORMAT;
        *sphere=value;return RF_OK;
    }
    return RF_NOT_FOUND;
}
int rf_model_geometry_clip_near(const rf_model_geometry *geometry,uint32_t batch,
    rf_model_render_buffers *buffers,float near_depth)
{
    const rf_model_draw_batch *draw;uint32_t i;
    if(!geometry || !geometry->batches || !geometry->reuse || batch>=geometry->batch_count ||
       !buffers || !buffers->cache || !buffers->clip || !isfinite(near_depth) || near_depth<=0)return RF_RANGE;
    draw=geometry->batches+batch;
    if(draw->vertices>buffers->capacity || draw->first_vertex>geometry->vertex_count ||
       draw->vertices>geometry->vertex_count-draw->first_vertex)return RF_RANGE;
    for(i=0;i<draw->vertices;++i) {
        int32_t reuse=geometry->reuse[draw->first_vertex+i];
        if(reuse>0) {if((uint32_t)reuse>i)return RF_RANGE;}
        else if(!isfinite(buffers->clip[i][2]))return RF_FORMAT;
    }
    for(i=0;i<draw->vertices;++i) {
        int32_t reuse=geometry->reuse[draw->first_vertex+i];
        if(reuse>0)buffers->cache[i].clip=buffers->cache[i-reuse].clip;
        else buffers->cache[i].clip=(buffers->cache[i].clip&~0x81u)|(buffers->clip[i][2]<near_depth?1:0);
    }
    return RF_OK;
}
static int model_geometry_emit_batch(const rf_model_geometry *geometry,uint32_t batch,
    const rf_model_render_buffers *buffers,const rf_model_projection *view,
    const rf_model_clip_planes *planes,const rf_model_clip_projection *projection,
    const rf_model_render_output *attributes,uint16_t base,rf_model_clip_pool *pool,
    rf_model_triangle_output *output,const float (*face_planes)[4],const uint8_t (*colors)[3])
{
    const rf_model_draw_batch *draw;uint32_t i,j;int status;
    if(!geometry || !geometry->batches || batch>=geometry->batch_count || !buffers || !buffers->cache || !buffers->clip ||
        !view || !planes || !projection || !attributes || !pool || !output || !output->vertices || !output->indices)return RF_RANGE;
    draw=geometry->batches+batch;
    if(!geometry->vertices || !geometry->reuse || !geometry->triangles || draw->vertices>buffers->capacity ||
        draw->first_vertex>geometry->vertex_count || draw->vertices>geometry->vertex_count-draw->first_vertex ||
        draw->first_triangle>geometry->triangle_count || draw->triangles>geometry->triangle_count-draw->first_triangle ||
        output->vertex_count<draw->vertices || output->vertex_count>output->vertex_capacity || output->index_count>output->index_capacity)return RF_RANGE;
    for(i=0;i<draw->triangles;++i)for(j=0;j<3;++j) {
        uint32_t index=geometry->triangles[draw->first_triangle+i].indices[j];int64_t source;
        if(index>=draw->vertices || index>INT16_MAX)return RF_RANGE;
        source=(int64_t)index-geometry->reuse[draw->first_vertex+index];
        if(source<0 || source>=draw->vertices)return RF_RANGE;
    }
    for(i=0;i<draw->triangles;++i) {
        const rf_model_triangle *triangle=geometry->triangles+draw->first_triangle+i;uint32_t route;
        status=face_planes?rf_model_route_static_triangle(buffers->cache,draw->vertices,triangle->indices,triangle->flags,face_planes[i],view,&route):
            rf_model_route_triangle(buffers->cache,draw->vertices,triangle->indices,triangle->flags,view,&route);if(status)return status;
        if(route==RF_MODEL_TRIANGLE_DIRECT) {
            uint32_t available=output->index_capacity-output->index_count;
            if(available<3 || (view->compute_clip && available==3))return RF_RANGE;
            for(j=0;j<3;++j)output->indices[output->index_count++]=(uint16_t)(base+triangle->indices[j]);
        } else if(route==RF_MODEL_TRIANGLE_CLIP) {
            uint8_t records[3][48],*original[3],*result[48],mask[2]={0,255};uint32_t count;
            memset(records,0,sizeof(records));rf_model_clip_pool_reset(pool);
            if(face_planes)status=rf_model_prepare_static_clip_triangle(geometry->vertices+draw->first_vertex,geometry->reuse+draw->first_vertex,
                buffers->cache,buffers->clip,draw->vertices,triangle->indices,colors,records);
            else status=rf_model_prepare_clip_triangle(geometry->vertices+draw->first_vertex,geometry->reuse+draw->first_vertex,
                buffers->cache,buffers->clip,draw->vertices,triangle->indices,attributes,records);
            if(status)return status;
            for(j=0;j<3;++j) {original[j]=records[j];mask[0]|=records[j][24];mask[1]&=records[j][24];}
            status=rf_model_clip_polygon(pool,original,3,planes,view,0x66,5,result,&count,mask);if(status)return status;
            status=rf_model_emit_clip_polygon(result,count,mask[1],triangle->indices,base,projection,attributes,view->depth_factor,output);if(status)return status;
        }
    }
    return RF_OK;
}

int rf_model_geometry_emit_batch(const rf_model_geometry *geometry,uint32_t batch,
    const rf_model_render_buffers *buffers,const rf_model_projection *view,
    const rf_model_clip_planes *planes,const rf_model_clip_projection *projection,
    const rf_model_render_output *attributes,uint16_t base,rf_model_clip_pool *pool,
    rf_model_triangle_output *output)
{
    return model_geometry_emit_batch(geometry,batch,buffers,view,planes,projection,attributes,base,pool,output,NULL,NULL);
}

int rf_model_geometry_emit_static_batch(const rf_model_geometry *geometry,uint32_t batch,
    const rf_model_render_buffers *buffers,const rf_model_projection *view,
    const rf_model_clip_planes *planes,const rf_model_clip_projection *projection,
    const rf_model_render_output *attributes,uint16_t base,rf_model_clip_pool *pool,
    rf_model_triangle_output *output,const float (*face_planes)[4],const uint8_t (*colors)[3])
{
    if(!face_planes)return RF_RANGE;
    return model_geometry_emit_batch(geometry,batch,buffers,view,planes,projection,attributes,base,pool,output,face_planes,colors);
}

int rf_model_prepare_clip_triangle(const rf_model_vertex *vertices,const int32_t *reuse,
    const rf_model_render_cache *cache,const float (*clip)[3],uint32_t count,
    const uint16_t indices[3],const rf_model_render_output *output,uint8_t records[3][48])
{
    uint32_t i,sources[3];
    if(!vertices || !reuse || !cache || !clip || !indices || !output || !records)return RF_RANGE;
    for(i=0;i<3;++i) {
        uint32_t index=indices[i];int64_t source;
        if(index>=count || index>INT16_MAX)return RF_RANGE;
        source=(int64_t)index-reuse[index];
        if(source<0 || source>=count)return RF_RANGE;
        sources[i]=(uint32_t)source;
    }
    for(i=0;i<3;++i) {
        uint32_t index=indices[i];const uint8_t *rgb=output->lighting?cache[sources[i]].rgb:output->rgb;
        memcpy(records[i],clip[sources[i]],12);records[i][24]=cache[index].clip;
        records[i][25]=0;records[i][26]=(uint8_t)i;
        memcpy(records[i]+28,vertices[index].uv,8);memcpy(records[i]+44,rgb,3);
    }
    return RF_OK;
}

int rf_model_prepare_static_clip_triangle(const rf_model_vertex *vertices,const int32_t *reuse,
    const rf_model_render_cache *cache,const float (*clip)[3],uint32_t count,
    const uint16_t indices[3],const uint8_t (*colors)[3],uint8_t records[3][48])
{
    rf_model_render_output attributes={0};uint32_t i;int status;
    attributes.lighting=1;
    status=rf_model_prepare_clip_triangle(vertices,reuse,cache,clip,count,indices,&attributes,records);
    if(status)return status;
    if(colors)for(i=0;i<3;++i)memcpy(records[i]+44,colors[indices[i]],3);
    return RF_OK;
}

int rf_model_geometry_render_batch(const rf_model_geometry *geometry,uint32_t batch,
    const float (*matrices)[12],uint32_t bones,const rf_model_projection *view,
    const rf_model_lighting *lights,const rf_model_render_output *output,rf_model_render_buffers *buffers)
{
    const rf_model_draw_batch *draw;uint32_t i,j;int status;
    if(!geometry || !geometry->batches || batch>=geometry->batch_count || !view || !lights || !output ||
        !buffers || !buffers->cache || !buffers->clip || !buffers->second || !buffers->vertices || bones>256 ||
        (bones && !matrices))return RF_RANGE;
    draw=geometry->batches+batch;
    if(draw->vertices>buffers->capacity || draw->first_vertex>geometry->vertex_count ||
        draw->vertices>geometry->vertex_count-draw->first_vertex || !geometry->vertices || !geometry->reuse)return RF_RANGE;
    for(i=0;i<draw->vertices;++i) {
        const rf_model_vertex *v=geometry->vertices+draw->first_vertex+i;
        int32_t reuse=geometry->reuse[draw->first_vertex+i];
        if(reuse>0) {if((uint32_t)reuse>i)return RF_RANGE;}
        else for(j=0;j<4 && v->weights[j];++j)if(v->bones[j]>=bones)return RF_RANGE;
    }
    for(i=0;i<draw->vertices;++i) {
        const rf_model_vertex *v=geometry->vertices+draw->first_vertex+i;
        int32_t reuse=geometry->reuse[draw->first_vertex+i];
        if(reuse>0)status=rf_model_render_reuse_vertex(buffers->cache,draw->vertices,i,reuse,output,v->uv,buffers->vertices[i]);
        else {
            float pair[6];uint32_t visible;
            status=rf_model_render_vertex_pair(v->position,v->normal,v->weights,v->bones,matrices,bones,pair);if(status)return status;
            memcpy(buffers->cache[i].world,pair,12);memcpy(buffers->second[i],pair+3,12);
            status=rf_model_project_vertex(pair,view,buffers->cache+i,buffers->clip[i],buffers->vertices[i],&visible);if(status)return status;
            if(visible)status=rf_model_finish_render_vertex(buffers->cache+i,buffers->second[i],output,lights->lights,lights->ambient,v->uv,buffers->vertices[i]);
        }
        if(status)return status;
    }
    return RF_OK;
}

static uint32_t integer(reader *r, uint32_t bytes)
{
    unsigned char raw[4] = {0};
    uint32_t value = 0, i;
    if (r->status) return 0;
    r->status = rf_vpp_read(r->model->archive, &r->model->entry, r->cursor, raw, bytes);
    if (r->status) return 0;
    r->cursor += bytes;
    for (i = 0; i < bytes; ++i) value |= (uint32_t)raw[i] << (i * 8);
    return value;
}
static void skip(reader *r, uint64_t bytes)
{
    if (r->status) return;
    if (r->cursor > r->model->entry.size || bytes > r->model->entry.size - r->cursor) r->status = RF_FORMAT;
    else r->cursor += (uint32_t)bytes;
}
static void submesh(reader *r)
{
    uint32_t version, lods, i, count,thresholds[3];
    skip(r, 48);
    version = integer(r, 4); lods = integer(r, 4);
    if (r->status) return;
    if (version < 7 || version > INT32_MAX || lods < 1 || lods > 3) { r->status = RF_FORMAT; return; }
    for(i=0;i<lods;++i)thresholds[i]=integer(r,4);
    skip(r,40);
    for (i = 0; i < lods && !r->status; ++i) {
        uint32_t batches, bytes, textures, t, flags, auxiliary, b;
        uint64_t relative;
        rf_model_lod *lod;
        if (r->model->lod_count == RF_MODEL_MAX_LODS) { r->status = RF_RANGE; return; }
        lod = &r->model->lods[r->model->lod_count++];
        memcpy(&lod->threshold,thresholds+i,4);
        flags = integer(r, 4); auxiliary = integer(r, 4);
        batches = integer(r, 2); bytes = integer(r, 4);
        lod->offset = r->cursor; lod->size = bytes;
        skip(r, bytes); skip(r, 4);
        lod->batch_offset=r->cursor;lod->batch_count=batches;lod->flags=flags;lod->auxiliary=auxiliary;
        relative = ((uint64_t)batches * 56 + 15) & ~(uint64_t)15;
        for (b = 0; b < batches && !r->status; ++b) {
            uint32_t info[7], j;
            for (j = 0; j < 7; ++j) info[j] = integer(r, 2);
            skip(r, 4);
            /* Original 0x569920: positions, normals, UV, indices, optional
             * planes, extra data, bone links, auxiliary bytes, all aligned. */
            relative = (relative + info[2] + 15) & ~(uint64_t)15;
            relative = (relative + info[2] + 15) & ~(uint64_t)15;
            relative = (relative + info[6] + 15) & ~(uint64_t)15;
            relative = (relative + info[3] + 15) & ~(uint64_t)15;
            if (flags & 0x20) relative = (relative + (uint64_t)info[1] * 16 + 15) & ~(uint64_t)15;
            relative = (relative + info[4] + 15) & ~(uint64_t)15;
            if (info[5]) relative = (relative + info[5] + 15) & ~(uint64_t)15;
            if (flags & 1) relative = (relative + (uint64_t)auxiliary * 2 + 15) & ~(uint64_t)15;
        }
        lod->attachment_count = integer(r, 4);
        if (!r->status && relative + (uint64_t)lod->attachment_count * 100 != bytes) r->status = RF_FORMAT;
        if (r->status) return;
        lod->attachment_offset = lod->offset + (uint32_t)relative;
        textures = integer(r, 4);
        lod->texture_offset=r->cursor;lod->texture_count=textures;lod->section_index=r->model->section_count-1;
        for (t = 0; t < textures && !r->status; ++t) {
            uint32_t length = 0;
            skip(r, 1);
            while (!r->status && integer(r, 1))
                if (++length > 256) r->status = RF_FORMAT;
        }
    }
    count = integer(r, 4);
    r->model->sections[r->model->section_count-1].material_count=count;
    r->model->sections[r->model->section_count-1].material_offset=r->cursor;
    skip(r, (uint64_t)count * 84);
    count = integer(r, 4); skip(r, (uint64_t)count * 28);
}
int rf_model_file_open(rf_model_file *model, rf_vpp *archive, const char *name)
{
    reader r;
    uint32_t magic, version, declared_meshes, found_meshes = 0;
    int ended = 0;
    if (!model) return RF_RANGE;
    memset(model, 0, sizeof(*model));
    if (!archive || !name) return RF_RANGE;
    model->archive = archive;
    r.model = model; r.cursor = 0;
    r.status = rf_vpp_find(archive, name, &model->entry);
    magic = integer(&r, 4); version = integer(&r, 4); declared_meshes = integer(&r, 4);
    if (!r.status && ((magic != 0x5246434d && magic != 0x52463344) || version != 0x40000)) r.status = RF_FORMAT;
    skip(&r, 28);
    while (!r.status && r.cursor < model->entry.size) {
        rf_model_section *section;
        uint32_t kind, bytes;
        if (model->section_count == RF_MODEL_MAX_SECTIONS) { r.status = RF_RANGE; break; }
        kind = integer(&r, 4); bytes = integer(&r, 4);
        if (r.status) break;
        section = &model->sections[model->section_count++];
        section->type = kind; section->offset = r.cursor;
        if (!kind) {
            if (bytes || r.cursor != model->entry.size) r.status = RF_FORMAT;
            ended = 1; break;
        }
        if (kind == 0x5355424d) { submesh(&r); ++found_meshes; }
        else skip(&r, bytes);
        section->size = r.cursor - section->offset;
    }
    if (!r.status && (!ended || found_meshes != declared_meshes)) r.status = RF_FORMAT;
    if (r.status) { memset(model, 0, sizeof(*model)); return r.status; }
    model->submeshes = found_meshes;
    return RF_OK;
}

int rf_model_file_attachment(const rf_model_file *model, uint32_t lod_index, uint32_t index, rf_model_attachment *attachment)
{
    const rf_model_lod *lod;
    unsigned char raw[100];
    rf_model_attachment value;
    uint32_t i, j;
    uint64_t offset;
    int status;
    if (!model || !model->archive || !attachment || lod_index >= model->lod_count || lod_index >= RF_MODEL_MAX_LODS) return RF_RANGE;
    lod = &model->lods[lod_index];
    if (index >= lod->attachment_count) return RF_RANGE;
    offset = (uint64_t)lod->attachment_offset + (uint64_t)index * 100;
    if (offset > UINT32_MAX || offset + 100 > (uint64_t)lod->offset + lod->size) return RF_RANGE;
    status = rf_vpp_read(model->archive, &model->entry, (uint32_t)offset, raw, 100);
    if (status) return status;
    memset(&value, 0, sizeof(value));
    memcpy(value.name, raw, 68); value.name[68] = 0;
    for (i = 0; i < 8; ++i) {
        uint32_t bits = 0;
        for (j = 0; j < 4; ++j) bits |= (uint32_t)raw[68 + i * 4 + j] << (j * 8);
        if (i == 7) memcpy(&value.parent, &bits, 4);
        else {
            float f;
            memcpy(&f, &bits, 4);
            if (!isfinite(f)) return RF_FORMAT;
            if (i < 4) value.rotation[i] = f; else value.position[i - 4] = f;
        }
    }
    if (value.parent < -1) return RF_FORMAT;
    *attachment = value;
    return RF_OK;
}
int rf_model_file_select_lod(const rf_model_file *model,uint32_t submesh,uint32_t flags,
    int alternate,int32_t minimum,int scaled,int animated,double metric,uint32_t *out)
{
    uint32_t section,n=0,i,count=0,indices[3],selected;float thresholds[3];int status;
    if(!model || !model->archive || !out || model->section_count>RF_MODEL_MAX_SECTIONS ||
        model->lod_count>RF_MODEL_MAX_LODS)return RF_RANGE;
    for(section=0;section<model->section_count;++section)
        if(model->sections[section].type==0x5355424d && n++==submesh)break;
    if(section==model->section_count)return RF_RANGE;
    for(i=0;i<model->lod_count;++i)if(model->lods[i].section_index==section) {
        if(count==3)return RF_FORMAT;
        thresholds[count]=model->lods[i].threshold;indices[count++]=i;
    }
    if(!count)return RF_FORMAT;
    status=rf_model_select_lod(thresholds,count,flags,alternate,minimum,scaled,animated,metric,&selected);
    if(status)return status;
    *out=indices[selected];return RF_OK;
}

int rf_model_file_material(const rf_model_file *model,uint32_t submesh_index,uint32_t index,uint8_t raw[84])
{
    uint32_t i,n=0; uint8_t value[84];
    if (!model || !model->archive || !raw || model->section_count>RF_MODEL_MAX_SECTIONS) return RF_RANGE;
    for (i=0;i<model->section_count;++i) {
        const rf_model_section *section=&model->sections[i]; uint64_t offset,end; int status;
        if (section->type!=0x5355424d) continue;
        if (n++!=submesh_index) continue;
        if (index>=section->material_count) return RF_RANGE;
        offset=(uint64_t)section->material_offset+(uint64_t)index*84;
        end=(uint64_t)section->offset+section->size;
        if (offset<section->offset || offset>UINT32_MAX || offset+84>end) return RF_RANGE;
        status=rf_vpp_read(model->archive,&model->entry,(uint32_t)offset,value,84);
        if (status!=RF_OK) return status;
        memcpy(raw,value,84); return RF_OK;
    }
    return RF_RANGE;
}
int rf_model_file_batch(const rf_model_file *model,uint32_t lod_index,uint32_t index,rf_model_batch *batch)
{
    const rf_model_lod *lod;rf_model_batch value={0};uint64_t relative,end,descriptor;
    uint32_t i,j,info[7];uint8_t raw[18];int status;
    if(!model || !model->archive || !batch || lod_index>=model->lod_count || lod_index>=RF_MODEL_MAX_LODS) return RF_RANGE;
    lod=model->lods+lod_index;
    if(index>=lod->batch_count || lod->batch_count>65535) return RF_RANGE;
    end=(uint64_t)lod->offset+lod->size;
    if(end>model->entry.size || lod->attachment_offset<lod->offset || lod->attachment_offset>end) return RF_RANGE;
    relative=((uint64_t)lod->batch_count*56+15)&~(uint64_t)15;
    for(i=0;i<=index;++i) {
        descriptor=(uint64_t)lod->batch_offset+(uint64_t)i*18;
        if(descriptor>UINT32_MAX || descriptor+18>model->entry.size) return RF_RANGE;
        status=rf_vpp_read(model->archive,&model->entry,(uint32_t)descriptor,raw,18);if(status)return status;
        for(j=0;j<7;++j)info[j]=(uint32_t)raw[j*2]|(uint32_t)raw[j*2+1]<<8;
        memset(&value,0,sizeof(value));value.vertices=info[0];value.triangles=info[1];
        for(j=0;j<4;++j)value.format_bits|=(uint32_t)raw[14+j]<<(j*8);
        value.sizes[0]=value.sizes[1]=info[2];value.sizes[2]=info[6];value.sizes[3]=info[3];
        value.sizes[4]=(lod->flags&32)?info[1]*16:0;value.sizes[5]=info[4];value.sizes[6]=info[5];
        if((lod->flags&1) && lod->auxiliary>UINT32_MAX/2)return RF_RANGE;
        value.sizes[7]=(lod->flags&1)?lod->auxiliary*2:0;
        for(j=0;j<8;++j) {
            uint64_t offset=(uint64_t)lod->offset+relative;
            if(offset>lod->attachment_offset || value.sizes[j]>lod->attachment_offset-offset)return RF_FORMAT;
            if(value.sizes[j])value.offsets[j]=(uint32_t)offset;
            relative=(relative+value.sizes[j]+15)&~(uint64_t)15;
        }
    }
    *batch=value;return RF_OK;
}
static int batch_read(const rf_model_file *model,const rf_model_batch *batch,uint32_t region,uint32_t index,uint32_t stride,void *raw)
{
    uint64_t relative=(uint64_t)index*stride,offset=(uint64_t)batch->offsets[region]+relative;
    if(!batch->offsets[region] || relative+stride>batch->sizes[region] || offset>UINT32_MAX)return RF_FORMAT;
    return rf_vpp_read(model->archive,&model->entry,(uint32_t)offset,raw,stride);
}
int rf_model_file_vertex(const rf_model_file *model,const rf_model_batch *batch,uint32_t index,rf_model_vertex *vertex)
{
    rf_model_vertex value;uint8_t raw[32],links[8];uint32_t i,j;float f;int status;
    if(!model || !model->archive || !batch || !vertex || index>=batch->vertices)return RF_RANGE;
    if(batch->format_bits!=0x518c41)return RF_FORMAT;
    status=batch_read(model,batch,0,index,12,raw);if(status)return status;
    status=batch_read(model,batch,1,index,12,raw+12);if(status)return status;
    status=batch_read(model,batch,2,index,8,raw+24);if(status)return status;
    memset(&value,0,sizeof(value));memset(value.bones,255,4);
    for(i=0;i<8;++i) {
        uint32_t bits=0;for(j=0;j<4;++j)bits|=(uint32_t)raw[i*4+j]<<(j*8);
        /* Source assets contain non-finite normals. Preserve their exact bits;
         * resolving their runtime treatment belongs to the renderer/skinner. */
        if(i<3 || i>=6) { memcpy(&f,&bits,4);if(!isfinite(f))return RF_FORMAT; }
        if(i<3)memcpy(value.position+i,&bits,4);
        else if(i<6)memcpy(value.normal+i-3,&bits,4);
        else memcpy(value.uv+i-6,&bits,4);
    }
    if(batch->sizes[6]) {
        status=batch_read(model,batch,6,index,8,links);if(status)return status;
        memcpy(value.weights,links,4);memcpy(value.bones,links+4,4);
    }
    *vertex=value;return RF_OK;
}
int rf_model_file_triangle_plane(const rf_model_file *model,const rf_model_batch *batch,uint32_t index,float plane[4])
{
    uint8_t raw[16];float value[4];uint32_t i,j,bits;int status;
    if(!model || !model->archive || !batch || !plane || index>=batch->triangles)return RF_RANGE;
    if(!batch->sizes[4])return RF_NOT_FOUND;
    status=batch_read(model,batch,4,index,16,raw);if(status)return status;
    for(i=0;i<4;++i) {
        bits=0;for(j=0;j<4;++j)bits|=(uint32_t)raw[i*4+j]<<(8*j);
        memcpy(value+i,&bits,4);
    }
    memcpy(plane,value,16);return RF_OK;
}
int rf_model_file_triangle(const rf_model_file *model,const rf_model_batch *batch,uint32_t index,rf_model_triangle *triangle)
{
    rf_model_triangle value;uint8_t raw[8];uint32_t i;int status;
    if(!model || !model->archive || !batch || !triangle || index>=batch->triangles)return RF_RANGE;
    if(batch->format_bits!=0x518c41)return RF_FORMAT;
    status=batch_read(model,batch,3,index,8,raw);if(status)return status;
    for(i=0;i<3;++i) {
        value.indices[i]=(uint16_t)((uint32_t)raw[i*2]|(uint32_t)raw[i*2+1]<<8);
        if(value.indices[i]>=batch->vertices)return RF_FORMAT;
    }
    value.flags=(uint16_t)((uint32_t)raw[6]|(uint32_t)raw[7]<<8);
    *triangle=value;return RF_OK;
}
int rf_model_file_vertex_reuse(const rf_model_file *model,const rf_model_batch *batch,uint32_t index,int32_t *distance)
{
    uint8_t raw[2];int32_t value;int status;
    if(!model || !model->archive || !batch || !distance || index>=batch->vertices)return RF_RANGE;
    if(batch->format_bits!=0x518c41)return RF_FORMAT;
    status=batch_read(model,batch,5,index,2,raw);if(status)return status;
    value=(int32_t)((uint32_t)raw[0]|(uint32_t)raw[1]<<8);
    if(value&32768)value-=65536;
    if(value>0 && (uint32_t)value>index)return RF_FORMAT;
    *distance=value;return RF_OK;
}
int rf_model_file_batch_material(const rf_model_file *model,uint32_t lod_index,uint32_t batch,uint32_t *material)
{
    const rf_model_lod *lod;const rf_model_section *section;uint8_t raw[4],value=0,ch;
    uint32_t slot,i,j,cursor;uint64_t offset,flattened=0,end;int status;
    if(!model || !model->archive || !material || model->section_count>RF_MODEL_MAX_SECTIONS || lod_index>=model->lod_count || lod_index>=RF_MODEL_MAX_LODS)return RF_RANGE;
    lod=model->lods+lod_index;
    if(batch>=lod->batch_count || lod->section_index>=model->section_count)return RF_RANGE;
    section=model->sections+lod->section_index;end=(uint64_t)section->offset+section->size;
    offset=(uint64_t)lod->offset+(uint64_t)batch*56+32;
    if(section->type!=0x5355424d || offset+4>(uint64_t)lod->offset+lod->size || offset>UINT32_MAX)return RF_RANGE;
    status=rf_vpp_read(model->archive,&model->entry,(uint32_t)offset,raw,4);if(status)return status;
    slot=(uint32_t)raw[0]|(uint32_t)raw[1]<<8|(uint32_t)raw[2]<<16|(uint32_t)raw[3]<<24;
    if(slot&0x80000000u)return RF_NOT_FOUND;
    if(slot>=lod->texture_count)return RF_FORMAT;
    cursor=lod->texture_offset;
    for(i=0;i<=slot;++i) {
        if(cursor<section->offset || cursor>=end)return RF_FORMAT;
        status=rf_vpp_read(model->archive,&model->entry,cursor++,&value,1);if(status)return status;
        for(j=0;;++j) {
            if(j>256 || cursor>=end)return RF_FORMAT;
            status=rf_vpp_read(model->archive,&model->entry,cursor++,&ch,1);if(status)return status;
            if(!ch)break;
        }
    }
    if(value>=section->material_count)return RF_FORMAT;
    for(i=0;i<lod->section_index;++i)if(model->sections[i].type==0x5355424d)flattened+=model->sections[i].material_count;
    flattened+=value;if(flattened>UINT32_MAX)return RF_RANGE;
    *material=(uint32_t)flattened;return RF_OK;
}
int rf_model_file_part_metadata(const rf_model_file *model,uint32_t submesh,rf_model_part_metadata *metadata)
{
    uint32_t i,j,ordinal=0,count,relative;uint8_t header[8],raw[40];rf_model_part_metadata value={0};int status;
    if(!model || !model->archive || !metadata || model->section_count>RF_MODEL_MAX_SECTIONS || model->lod_count>RF_MODEL_MAX_LODS)return RF_RANGE;
    for(i=0;i<model->section_count;++i)if(model->sections[i].type==0x5355424d) {
        const rf_model_section *section=model->sections+i;
        if(ordinal++!=submesh)continue;
        if(section->size<56 || (uint64_t)section->offset+section->size>model->entry.size)return RF_FORMAT;
        status=rf_vpp_read(model->archive,&model->entry,section->offset+48,header,8);if(status)return status;
        memcpy(&count,header+4,4);if(count<1 || count>3)return RF_FORMAT;
        relative=56+count*4;if(relative+40>section->size)return RF_FORMAT;
        status=rf_vpp_read(model->archive,&model->entry,section->offset+relative,raw,40);if(status)return status;
        memcpy(value.offset,raw,12);memcpy(&value.radius,raw+12,4);memcpy(value.minimum,raw+16,12);memcpy(value.maximum,raw+28,12);
        if(!isfinite(value.radius) || value.radius<0)return RF_FORMAT;
        for(j=0;j<3;++j)if(!isfinite(value.offset[j]) || !isfinite(value.minimum[j]) || !isfinite(value.maximum[j]) || value.minimum[j]>value.maximum[j])return RF_FORMAT;
        for(j=0;j<model->lod_count;++j)if(model->lods[j].section_index==i)break;
        if(j==model->lod_count || count>model->lod_count-j)return RF_FORMAT;
        value.first_lod=j;value.lod_count=count;
        for(;j<value.first_lod+count;++j)if(model->lods[j].section_index!=i)return RF_FORMAT;
        *metadata=value;return RF_OK;
    }
    return RF_RANGE;
}

void rf_model_collision_geometry_close(rf_model_collision_geometry *g)
{
    if(!g)return;free((void *)g->view.batches);free(g->data);memset(g,0,sizeof(*g));
}
int rf_model_collision_geometry_open(rf_model_collision_geometry *g,const rf_model_file *model,uint32_t index,uint32_t budget)
{
    rf_model_collision_geometry next={0};const rf_model_lod *lod;rf_collision_model_batch_view *views;
    uint64_t bytes;uint32_t i,j,k;int status;
    if(!g || !model || !model->archive || index>=model->lod_count || index>=RF_MODEL_MAX_LODS || g->data || g->view.batches || g->accounted_bytes)return RF_RANGE;
    lod=model->lods+index;if(!(lod->flags&32u))return RF_NOT_FOUND;
    if(lod->batch_count>65535 || (uint64_t)lod->offset+lod->size>model->entry.size)return RF_FORMAT;
    bytes=sizeof(next)+(uint64_t)lod->size+(uint64_t)lod->batch_count*sizeof(*views);
    if(bytes>budget || bytes>SIZE_MAX)return RF_RANGE;
    next.accounted_bytes=(uint32_t)bytes;next.view.flags=lod->flags;next.view.batch_count=(uint16_t)lod->batch_count;
    views=lod->batch_count?calloc(lod->batch_count,sizeof(*views)):NULL;next.view.batches=views;
    next.data=lod->size?malloc(lod->size):NULL;
    if((lod->batch_count && !views) || (lod->size && !next.data)){status=RF_IO;goto fail;}
    if(lod->size){status=rf_vpp_read(model->archive,&model->entry,lod->offset,next.data,lod->size);if(status)goto fail;}
    for(i=0;i<lod->batch_count;++i) {
        rf_model_batch batch;rf_collision_model_batch_view *view=views+i;unsigned char *data=next.data;
        status=rf_model_file_batch(model,index,i,&batch);if(status)goto fail;
        if(batch.sizes[0]<(uint64_t)batch.vertices*12 || batch.sizes[3]<(uint64_t)batch.triangles*8 || batch.sizes[4]<(uint64_t)batch.triangles*16){status=RF_FORMAT;goto fail;}
        view->vertices=batch.vertices?(const float (*)[3])(data+batch.offsets[0]-lod->offset):NULL;
        view->planes=batch.triangles?(const float (*)[4])(data+batch.offsets[4]-lod->offset):NULL;
        view->triangles=batch.triangles?(const rf_collision_model_triangle_record *)(data+batch.offsets[3]-lod->offset):NULL;
        view->triangle_count=(uint16_t)batch.triangles;view->token_base=batch.offsets[3];
        for(j=0;j<batch.vertices;++j)for(k=0;k<3;++k)if(!isfinite(view->vertices[j][k])){status=RF_FORMAT;goto fail;}
        for(j=0;j<batch.triangles;++j)for(k=0;k<3;++k) {
            int32_t n=view->triangles[j].indices[k];if(n<0 || (uint32_t)n>=batch.vertices){status=RF_FORMAT;goto fail;}
        }
    }
    *g=next;return RF_OK;
 fail:
    rf_model_collision_geometry_close(&next);return status;
}

void rf_model_skin_geometry_close(rf_model_skin_geometry *g)
{
    if(!g)return;free(g->batches);free(g->data);memset(g,0,sizeof(*g));
}
int rf_model_skin_geometry_open(rf_model_skin_geometry *g,const rf_model_file *model,
    uint32_t index,uint32_t bone_count,uint32_t budget)
{
    rf_model_skin_geometry next={0};const rf_model_lod *lod;uint64_t bytes;uint32_t i,j,k;int status;
    if(!g || !model || !model->archive || index>=model->lod_count || index>=RF_MODEL_MAX_LODS ||
        bone_count>256 || g->data || g->batches || g->accounted_bytes || g->batch_count || g->max_vertices)return RF_RANGE;
    lod=model->lods+index;if(!(lod->flags&2u))return RF_NOT_FOUND;
    if(lod->batch_count>65535 || (uint64_t)lod->offset+lod->size>model->entry.size)return RF_FORMAT;
    bytes=sizeof(next)+(uint64_t)lod->size+(uint64_t)lod->batch_count*sizeof(*next.batches);
    if(bytes>budget || bytes>SIZE_MAX)return RF_RANGE;
    next.accounted_bytes=(uint32_t)bytes;next.batch_count=(uint16_t)lod->batch_count;
    next.batches=lod->batch_count?calloc(lod->batch_count,sizeof(*next.batches)):NULL;
    next.data=lod->size?malloc(lod->size):NULL;
    if((lod->batch_count && !next.batches) || (lod->size && !next.data)){status=RF_IO;goto fail;}
    if(lod->size){status=rf_vpp_read(model->archive,&model->entry,lod->offset,next.data,lod->size);if(status)goto fail;}
    for(i=0;i<lod->batch_count;++i) {
        rf_model_batch batch;rf_collision_model_skin_batch *view=next.batches+i;unsigned char *data=next.data;
        status=rf_model_file_batch(model,index,i,&batch);if(status)goto fail;
        if(batch.sizes[0]<(uint64_t)batch.vertices*12 || batch.sizes[3]<(uint64_t)batch.triangles*8 || batch.sizes[6]<(uint64_t)batch.vertices*8){status=RF_FORMAT;goto fail;}
        view->positions=batch.vertices?(const float (*)[3])(data+batch.offsets[0]-lod->offset):NULL;
        view->links=batch.vertices?(const rf_collision_model_skin_links *)(data+batch.offsets[6]-lod->offset):NULL;
        view->triangles=batch.triangles?(const rf_collision_model_triangle_record *)(data+batch.offsets[3]-lod->offset):NULL;
        view->vertex_count=(uint16_t)batch.vertices;view->triangle_count=(uint16_t)batch.triangles;
        if(batch.vertices>next.max_vertices)next.max_vertices=(uint16_t)batch.vertices;
        for(j=0;j<batch.vertices;++j) {
            for(k=0;k<3;++k)if(!isfinite(view->positions[j][k])){status=RF_FORMAT;goto fail;}
            for(k=0;k<4 && view->links[j].weights[k];++k)
                if(view->links[j].bones[k]>=bone_count){status=RF_FORMAT;goto fail;}
        }
        for(j=0;j<batch.triangles;++j)for(k=0;k<3;++k) {
            int32_t n=view->triangles[j].indices[k];if(n<0 || (uint32_t)n>=batch.vertices){status=RF_FORMAT;goto fail;}
        }
    }
    *g=next;return RF_OK;
 fail:
    rf_model_skin_geometry_close(&next);return status;
}

void rf_model_collision_resource_close(rf_model_collision_resource *resource)
{
    uint32_t i;if(!resource)return;
    if(resource->lods)for(i=0;i<resource->lod_count;++i)rf_model_collision_geometry_close(resource->lods+i);
    free(resource->lods);free(resource->parts);memset(resource,0,sizeof(*resource));
}
int rf_model_collision_resource_open(rf_model_collision_resource *resource,const rf_model_file *model,uint32_t budget)
{
    rf_model_collision_resource next={0};uint64_t bytes;uint32_t i;int status;
    if(!resource || !model || !model->archive || resource->parts || resource->lods || resource->accounted_bytes ||
        model->submeshes>RF_MODEL_MAX_SECTIONS || model->lod_count>RF_MODEL_MAX_LODS)return RF_RANGE;
    bytes=sizeof(next)+(uint64_t)model->submeshes*sizeof(*next.parts);
    for(i=0;i<model->lod_count;++i) {
        const rf_model_lod *lod=model->lods+i;if(!(lod->flags&32u))return RF_NOT_FOUND;
        if(lod->batch_count>65535)return RF_FORMAT;
        bytes+=sizeof(*next.lods)+(uint64_t)lod->size+(uint64_t)lod->batch_count*sizeof(rf_collision_model_batch_view);
    }
    if(bytes>budget || bytes>SIZE_MAX)return RF_RANGE;
    next.part_count=(int32_t)model->submeshes;next.lod_count=model->lod_count;next.accounted_bytes=(uint32_t)bytes;
    next.parts=next.part_count?calloc((size_t)next.part_count,sizeof(*next.parts)):NULL;
    next.lods=next.lod_count?calloc(next.lod_count,sizeof(*next.lods)):NULL;
    if((next.part_count && !next.parts) || (next.lod_count && !next.lods)){status=RF_IO;goto fail;}
    for(i=0;i<next.lod_count;++i) {
        status=rf_model_collision_geometry_open(next.lods+i,model,i,budget);if(status)goto fail;
    }
    for(i=0;i<(uint32_t)next.part_count;++i) {
        rf_model_part_metadata metadata;rf_collision_model_part_view *part=next.parts+i;
        status=rf_model_file_part_metadata(model,i,&metadata);if(status)goto fail;
        memcpy(part->offset,metadata.offset,12);memcpy(part->minimum,metadata.minimum,12);memcpy(part->maximum,metadata.maximum,12);
        part->selected=&next.lods[metadata.first_lod+metadata.lod_count-1].view;part->fallback=&next.lods[metadata.first_lod].view;
    }
    *resource=next;return RF_OK;
 fail:
    rf_model_collision_resource_close(&next);return status;
}

void rf_model_geometry_close(rf_model_geometry *g)
{
    if(!g)return;
    free(g->batches);free(g->vertices);free(g->triangles);free(g->reuse);memset(g,0,sizeof(*g));
}
int rf_model_geometry_open(rf_model_geometry *g,const rf_model_file *model,uint32_t lod,uint32_t budget)
{
    rf_model_geometry next={0};uint64_t bytes,vertices=0,triangles=0;uint32_t i,j;int status;
    if(!g || !model || !model->archive || lod>=model->lod_count || lod>=RF_MODEL_MAX_LODS ||
        g->batches || g->vertices || g->triangles || g->reuse || g->accounted_bytes)return RF_RANGE;
    next.batch_count=model->lods[lod].batch_count;
    bytes=sizeof(next)+(uint64_t)next.batch_count*sizeof(*next.batches);
    if(bytes>budget)return RF_RANGE;
    for(i=0;i<next.batch_count;++i) {
        rf_model_batch batch;status=rf_model_file_batch(model,lod,i,&batch);if(status)return status;
        vertices+=batch.vertices;triangles+=batch.triangles;
    }
    bytes+=vertices*(sizeof(*next.vertices)+sizeof(*next.reuse))+triangles*sizeof(*next.triangles);
    if(bytes>budget || bytes>SIZE_MAX || vertices>UINT32_MAX || triangles>UINT32_MAX)return RF_RANGE;
    next.vertex_count=(uint32_t)vertices;next.triangle_count=(uint32_t)triangles;next.accounted_bytes=(uint32_t)bytes;
    if(next.batch_count)next.batches=calloc(next.batch_count,sizeof(*next.batches));
    if(vertices) { next.vertices=malloc((size_t)vertices*sizeof(*next.vertices));next.reuse=malloc((size_t)vertices*sizeof(*next.reuse)); }
    if(triangles)next.triangles=malloc((size_t)triangles*sizeof(*next.triangles));
    if((next.batch_count && !next.batches) || (vertices && (!next.vertices || !next.reuse)) || (triangles && !next.triangles)) { status=RF_IO;goto fail; }
    vertices=triangles=0;
    for(i=0;i<next.batch_count;++i) {
        rf_model_batch batch;rf_model_draw_batch *draw=next.batches+i;
        status=rf_model_file_batch(model,lod,i,&batch);if(status)goto fail;
        draw->first_vertex=(uint32_t)vertices;draw->vertices=batch.vertices;
        draw->first_triangle=(uint32_t)triangles;draw->triangles=batch.triangles;draw->material=UINT32_MAX;
        status=rf_model_file_batch_material(model,lod,i,&draw->material);if(status && status!=RF_NOT_FOUND)goto fail;
        for(j=0;j<batch.vertices;++j) {
            status=rf_model_file_vertex(model,&batch,j,next.vertices+(size_t)vertices+j);if(status)goto fail;
            status=rf_model_file_vertex_reuse(model,&batch,j,next.reuse+(size_t)vertices+j);if(status)goto fail;
        }
        for(j=0;j<batch.triangles;++j) {
            status=rf_model_file_triangle(model,&batch,j,next.triangles+(size_t)triangles+j);if(status)goto fail;
        }
        vertices+=batch.vertices;triangles+=batch.triangles;
    }
    *g=next;return RF_OK;
fail:
    rf_model_geometry_close(&next);return status;
}

void rf_static_render_resource_close(rf_static_render_resource *resource)
{
    uint32_t i;if(!resource)return;
    if(resource->lods)for(i=0;i<resource->lod_count;++i) {
        rf_model_geometry_close(&resource->lods[i].geometry);free(resource->lods[i].planes);
    }
    free(resource->parts);free(resource->lods);free(resource->materials);memset(resource,0,sizeof(*resource));
}
int rf_static_render_resource_open(const rf_model_file *model,uint32_t budget,rf_static_render_resource *resource)
{
    rf_static_render_resource next={0};uint64_t bytes;uint32_t i,j,k,part=0,material=0;int status;
    if(!model || !model->archive || !resource || resource->parts || resource->lods || resource->materials ||
       resource->part_count || resource->lod_count || resource->material_count || resource->allocated_bytes ||
       !model->submeshes || model->section_count>RF_MODEL_MAX_SECTIONS || !model->lod_count || model->lod_count>RF_MODEL_MAX_LODS)return RF_RANGE;
    for(i=0;i<model->lod_count;++i)if(!(model->lods[i].flags&32))return RF_NOT_FOUND;
    for(i=0;i<model->section_count;++i)if(model->sections[i].type==0x5355424d) {
        if(model->sections[i].material_count>UINT32_MAX-next.material_count)return RF_RANGE;
        ++part;next.material_count+=model->sections[i].material_count;
    }
    if(part!=model->submeshes)return RF_FORMAT;
    next.part_count=part;next.lod_count=model->lod_count;
    bytes=sizeof(next)+(uint64_t)part*sizeof(*next.parts)+(uint64_t)next.lod_count*sizeof(*next.lods)+(uint64_t)next.material_count*sizeof(*next.materials);
    if(bytes>budget || bytes>SIZE_MAX)return RF_RANGE;
    next.parts=calloc(part,sizeof(*next.parts));next.lods=calloc(next.lod_count,sizeof(*next.lods));
    if(next.material_count)next.materials=malloc((size_t)next.material_count*sizeof(*next.materials));
    if(!next.parts || !next.lods || (next.material_count && !next.materials)){status=RF_IO;goto fail;}
    status=rf_model_file_static_bound_sphere(model,next.bound);if(status)goto fail;
    for(i=0;i<part;++i){status=rf_model_file_part_metadata(model,i,next.parts+i);if(status)goto fail;}
    part=0;
    for(i=0;i<model->section_count;++i)if(model->sections[i].type==0x5355424d) {
        for(j=0;j<model->sections[i].material_count;++j) {
            status=rf_model_file_material(model,part,j,next.materials[material++]);if(status)goto fail;
        }
        ++part;
    }
    for(i=0;i<next.lod_count;++i) {
        rf_static_render_lod *lod=next.lods+i;uint64_t allowance=(uint64_t)budget-bytes+sizeof(lod->geometry);
        status=rf_model_geometry_open(&lod->geometry,model,i,(uint32_t)(allowance>UINT32_MAX?UINT32_MAX:allowance));if(status)goto fail;
        bytes+=lod->geometry.accounted_bytes-sizeof(lod->geometry);
        bytes+=(uint64_t)lod->geometry.triangle_count*16;
        if(bytes>budget || bytes>SIZE_MAX){status=RF_RANGE;goto fail;}
        if(lod->geometry.triangle_count)lod->planes=malloc((size_t)lod->geometry.triangle_count*16);
        if(lod->geometry.triangle_count && !lod->planes){status=RF_IO;goto fail;}
        lod->threshold=model->lods[i].threshold;
        for(j=0;j<lod->geometry.batch_count;++j) {
            rf_model_batch batch;const rf_model_draw_batch *draw=lod->geometry.batches+j;
            status=rf_model_file_batch(model,i,j,&batch);if(status)goto fail;
            for(k=0;k<batch.triangles;++k) {
                status=rf_model_file_triangle_plane(model,&batch,k,lod->planes[draw->first_triangle+k]);if(status)goto fail;
            }
        }
    }
    next.allocated_bytes=(uint32_t)bytes;*resource=next;return RF_OK;
 fail:
    rf_static_render_resource_close(&next);return status;
}

int rf_model_origin_radius(const float sphere[4],float *radius)
{
    double x,y,z,value;float out;uint32_t i;
    if(!sphere || !radius)return RF_RANGE;
    for(i=0;i<4;i++)if(!isfinite(sphere[i]))return RF_RANGE;
    if(sphere[3]<0)return RF_RANGE;
    x=sphere[0];y=sphere[1];z=sphere[2];
    value=sqrt((x*x+y*y)+z*z)+(double)sphere[3];out=(float)value;
    if(!isfinite(out))return RF_RANGE;*radius=out;return RF_OK;
}
static int model_submesh_bound(const rf_model_file *model,const rf_model_section *section,float value[4])
{
    uint32_t lods,offset;unsigned char header[8];float radius;int status;
    if(section->size<56 || section->offset>model->entry.size || section->size>model->entry.size-section->offset)return RF_FORMAT;
    status=rf_vpp_read(model->archive,&model->entry,section->offset+48,header,8);if(status)return status;
    memcpy(&lods,header+4,4);if(lods<1 || lods>3)return RF_FORMAT;
    offset=56+lods*4;if(offset>section->size || section->size-offset<16)return RF_FORMAT;
    status=rf_vpp_read(model->archive,&model->entry,section->offset+offset,value,16);if(status)return status;
    return rf_model_origin_radius(value,&radius)?RF_FORMAT:RF_OK;
}
static int model_file_bound_fetch(void *context,uint32_t index,float value[4])
{
    const rf_model_file *model=context;uint32_t i;
    for(i=0;i<model->section_count;++i)if(model->sections[i].type==0x5355424d) {
        if(!index)return model_submesh_bound(model,model->sections+i,value);--index;
    }
    return RF_NOT_FOUND;
}
int rf_model_file_bound_sphere(const rf_model_file *model,float sphere[4])
{
    float value[4];int status;
    if(!model || !model->archive || !sphere || model->section_count>RF_MODEL_MAX_SECTIONS)return RF_RANGE;
    status=model_file_bound_fetch((void *)model,0,value);if(!status)memcpy(sphere,value,16);return status;
}
static int model_static_bound(int (*fetch)(void *,uint32_t,float[4]),void *context,uint32_t count,float sphere[4])
{
    float value[4]={0},row[4],difference[3],scale,distance,candidate;uint32_t i,j;int status;
    if(!sphere || !count || count>INT32_MAX)return RF_RANGE;
    for(i=0;i<count;++i) {
        status=fetch(context,i,row);if(status)return status;
        for(j=0;j<4;++j)if(!isfinite(row[j]))return RF_RANGE;
        if(row[3]<0)return RF_RANGE;
        for(j=0;j<3;++j){value[j]=(float)((double)value[j]+row[j]);if(!isfinite(value[j]))return RF_RANGE;}
    }
    scale=(float)(1.0/(double)(int32_t)count);
    for(j=0;j<3;++j)value[j]=(float)((double)value[j]*scale);
    for(i=0;i<count;++i) {
        status=fetch(context,i,row);if(status)return status;
        for(j=0;j<3;++j)difference[j]=(float)((double)value[j]-row[j]);
        distance=(float)sqrt(((double)difference[0]*difference[0]+(double)difference[1]*difference[1])+(double)difference[2]*difference[2]);
        candidate=(float)((double)distance+row[3]);if(!isfinite(candidate))return RF_RANGE;
        if(candidate>value[3])value[3]=candidate;
    }
    memcpy(sphere,value,16);return RF_OK;
}
static int model_array_bound_fetch(void *context,uint32_t index,float value[4])
{const float (*rows)[4]=context;memcpy(value,rows[index],16);return RF_OK;}
int rf_model_static_bound_sphere(const float (*submeshes)[4],uint32_t count,float sphere[4])
{
    if(!submeshes)return RF_RANGE;
    return model_static_bound(model_array_bound_fetch,(void *)submeshes,count,sphere);
}
int rf_model_file_static_bound_sphere(const rf_model_file *model,float sphere[4])
{
    if(!model || !model->archive || model->section_count>RF_MODEL_MAX_SECTIONS)return RF_RANGE;
    return model_static_bound(model_file_bound_fetch,(void *)model,model->submeshes,sphere);
}
