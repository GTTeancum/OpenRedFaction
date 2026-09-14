/* Private renderer implementation; included once after vertex_program(). */
#define RETAINED_MODEL_LIMIT 128u
#define RETAINED_MODEL_BUDGET (4u*1024u*1024u)
typedef struct retained_skin_vertex {float position[3],weights[4],bones[4],uv[2];} retained_skin_vertex;
_Static_assert(sizeof(retained_skin_vertex)==52,"Retained skin attribute stride");
#define RETAINED_SKIN_BONES 28u
typedef struct retained_skin_part {uint32_t start,count,bones,two_sided;uint8_t ids[RETAINED_SKIN_BONES];} retained_skin_part;
typedef struct retained_model_entry {
    const rf_model_geometry *geometry;uint32_t batch,bones,vertices,part_count,bytes;
    retained_skin_vertex *gpu;retained_skin_part *parts;
} retained_model_entry;
typedef struct retained_model_draw {
    uint32_t entry,at_vertex,material,front_face;float rows[3][4],screen[4],palette[150][4];
} retained_model_draw;
static retained_model_entry retained_models[RETAINED_MODEL_LIMIT];
static retained_model_draw *retained_draws;
static uint32_t retained_model_count,retained_model_bytes;
/* queued, rendered, cached batches, resident bytes, submitted vertices,
 * submitted palette parts, frame fallbacks, cache misses. */
uint32_t rf_xbox_retained_models[8];
static void retained_models_close(void)
{
    uint32_t i;if(stream_device_ready)while(pb_busy()){}
    for(i=0;i<retained_model_count;i++) {
        if(retained_models[i].gpu)MmFreeContiguousMemory(retained_models[i].gpu);
        free(retained_models[i].parts);
    }
    free(retained_draws);retained_draws=NULL;memset(retained_models,0,sizeof(retained_models));
    retained_model_count=retained_draw_count=retained_model_bytes=0;
    memset(rf_xbox_retained_models,0,sizeof(rf_xbox_retained_models));
}
static int retained_skin_source(const rf_model_geometry *geometry,const rf_model_draw_batch *draw,
    uint32_t index,uint32_t bones,retained_skin_vertex *v)
{
    const rf_model_vertex *original,*source;uint32_t j;
    if(index>=draw->vertices)return RF_FORMAT;original=geometry->vertices+draw->first_vertex+index;
    while(geometry->reuse[draw->first_vertex+index]>0) {
        uint32_t distance=(uint32_t)geometry->reuse[draw->first_vertex+index];
        if(distance>index)return RF_FORMAT;index-=distance;
    }
    source=geometry->vertices+draw->first_vertex+index;memset(v,0,sizeof(*v));
    memcpy(v->position,source->position,12);memcpy(v->uv,original->uv,8);
    for(j=0;j<3;j++)if(!isfinite(v->position[j]))return RF_FORMAT;
    for(j=0;j<2;j++)if(!isfinite(v->uv[j]))return RF_FORMAT;
    for(j=0;j<4 && source->weights[j];j++) {
        if(source->bones[j]>=bones)return RF_FORMAT;
        v->weights[j]=(float)source->weights[j]*(1.0f/256.0f);v->bones[j]=(float)source->bones[j];
    }
    return RF_OK;
}
static int retained_skin_add(retained_skin_part *part,const retained_skin_vertex vertices[3])
{
    uint32_t i,j,k;retained_skin_part next=*part;
    for(i=0;i<3;i++)for(j=0;j<4 && vertices[i].weights[j];j++) {
        uint32_t bone=(uint32_t)vertices[i].bones[j];
        for(k=0;k<next.bones;k++)if(next.ids[k]==bone)break;
        if(k==next.bones){if(k==RETAINED_SKIN_BONES)return 0;next.ids[next.bones++]=(uint8_t)bone;}
    }
    *part=next;return 1;
}
static int retained_model_cache(const rf_model_geometry *geometry,uint32_t batch,uint32_t bones,uint32_t *slot)
{
    const rf_model_draw_batch *draw;retained_model_entry entry={0};uint32_t pass,i,j,k;uint64_t bytes;int status;
    if(!geometry || !geometry->batches || batch>=geometry->batch_count || !geometry->vertices || !geometry->triangles || !geometry->reuse)return RF_RANGE;
    for(i=0;i<retained_model_count;i++)if(retained_models[i].geometry==geometry && retained_models[i].batch==batch && retained_models[i].bones==bones) {
        if(!retained_models[i].gpu)return RF_NOT_FOUND;*slot=i;return RF_OK;
    }
    if(retained_model_count==RETAINED_MODEL_LIMIT)return RF_NOT_FOUND;
    draw=geometry->batches+batch;
    if(draw->first_vertex>geometry->vertex_count || draw->vertices>geometry->vertex_count-draw->first_vertex ||
       draw->first_triangle>geometry->triangle_count || draw->triangles>geometry->triangle_count-draw->first_triangle)return RF_RANGE;
    entry.geometry=geometry;entry.batch=batch;entry.bones=bones;
    *slot=retained_model_count;retained_models[retained_model_count++]=entry;++rf_xbox_retained_models[7];
    bytes=((uint64_t)draw->triangles*3*sizeof(retained_skin_vertex)+4095u)&~4095ull;
    if(!draw->vertices || !draw->triangles || bytes>RETAINED_MODEL_BUDGET-retained_model_bytes)return RF_NOT_FOUND;
    entry.vertices=draw->triangles*3;
    for(pass=0;pass<2;pass++) {
        retained_skin_part part={0};uint32_t parts=0;
        part.bones=1; /* Bone0 also makes all zero-weight shader fetches valid. */
        for(i=0;i<draw->triangles;i++) {
            retained_skin_vertex v[3];
            uint32_t two_sided=(geometry->triangles[draw->first_triangle+i].flags&0x20u)!=0;
            for(j=0;j<3;j++) {
                status=retained_skin_source(geometry,draw,geometry->triangles[draw->first_triangle+i].indices[j],bones,v+j);
                if(status)goto failed;
            }
            if((part.count && part.two_sided!=two_sided) || !retained_skin_add(&part,v)) {
                if(pass)entry.parts[parts]=part;
                ++parts;memset(&part,0,sizeof(part));part.start=i*3;part.bones=1;
                if(!retained_skin_add(&part,v)){status=RF_FORMAT;goto failed;}
            }
            part.two_sided=two_sided;
            if(pass)for(j=0;j<3;j++) {
                uint32_t influence;
                for(influence=0;influence<4;influence++) {
                    for(k=0;k<part.bones;k++)if(part.ids[k]==(uint32_t)v[j].bones[influence])break;
                    if(k==part.bones){status=RF_FORMAT;goto failed;}v[j].bones[influence]=(float)(k*3);
                }
                entry.gpu[i*3+j]=v[j];
            }
            part.count+=3;
        }
        if(pass)entry.parts[parts]=part;
        ++parts;
        if(!pass) {
            entry.part_count=parts;
            bytes=(((uint64_t)entry.vertices*sizeof(retained_skin_vertex)+4095u)&~4095ull)+(uint64_t)parts*sizeof(retained_skin_part);
            if(bytes>RETAINED_MODEL_BUDGET-retained_model_bytes)return RF_NOT_FOUND;entry.bytes=(uint32_t)bytes;
            entry.gpu=MmAllocateContiguousMemoryEx(entry.vertices*sizeof(retained_skin_vertex),0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
            entry.parts=calloc(parts,sizeof(retained_skin_part));
            if(!entry.gpu || !entry.parts){status=RF_NOT_FOUND;goto failed;}
        } else if(parts!=entry.part_count){status=RF_FORMAT;goto failed;}
    }
    __asm__ volatile("sfence" ::: "memory");retained_models[*slot]=entry;retained_model_bytes+=entry.bytes;
    ++rf_xbox_retained_models[2];rf_xbox_retained_models[3]=retained_model_bytes;return RF_OK;
failed:
    if(entry.gpu)MmFreeContiguousMemory(entry.gpu);free(entry.parts);return status;
}
static int retained_model_prepare(const rf_model_geometry *geometry,uint32_t batch,const float (*matrices)[12],uint32_t bones,
    const rf_model_projection *view,uint32_t material,uint32_t at_vertex)
{
    retained_model_draw *draw;uint32_t slot,i,j;float determinant;int status;
    if(!view || !matrices)return RF_RANGE;
    if(!bones || bones>50 || !view->perspective || retained_draw_count==RETAINED_MODEL_LIMIT)goto fallback;
    for(i=0;i<9;i++)if(!isfinite(view->rotation[i]))return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(view->camera[i]))return RF_RANGE;
    for(i=0;i<4;i++)if(!isfinite(view->screen[i]))return RF_RANGE;
    {const float *m=view->rotation;
     determinant=m[0]*(m[4]*m[8]-m[5]*m[7])-m[1]*(m[3]*m[8]-m[5]*m[6])+m[2]*(m[3]*m[7]-m[4]*m[6]);}
    determinant*=view->screen[0]*view->screen[1];
    if(!isfinite(determinant) || determinant==0)goto fallback;
    if(!retained_draws) {
        retained_draws=calloc(RETAINED_MODEL_LIMIT,sizeof(*retained_draws));if(!retained_draws)goto fallback;
        retained_model_bytes=RETAINED_MODEL_LIMIT*sizeof(*retained_draws)+sizeof(retained_models);
        rf_xbox_retained_models[3]=retained_model_bytes;
    }
    status=retained_model_cache(geometry,batch,bones,&slot);
    if(status==RF_NOT_FOUND)goto fallback;if(status)return status;
    if(retained_draw_count && retained_draws[retained_draw_count-1].at_vertex>at_vertex)return RF_RANGE;
    draw=retained_draws+retained_draw_count;draw->entry=slot;draw->at_vertex=at_vertex;draw->material=material;
    /* Original accepts dot(camera-a, cross(b-a,c-b)) > 0. Screen Y is
     * downward: a positive signed screen area is clockwise on NV2A. */
    draw->front_face=determinant<0?NV097_SET_FRONT_FACE_V_CW:NV097_SET_FRONT_FACE_V_CCW;
    for(i=0;i<3;i++) {
        memcpy(draw->rows[i],view->rotation+i*3,12);draw->rows[i][3]=0;
        for(j=0;j<3;j++)draw->rows[i][3]-=draw->rows[i][j]*view->camera[j];
    }
    memcpy(draw->screen,view->screen,16);
    for(i=0;i<bones;i++)for(j=0;j<3;j++) {
        float *row=draw->palette[i*3+j];uint32_t k;
        row[0]=matrices[i][j];row[1]=matrices[i][3+j];row[2]=matrices[i][6+j];row[3]=matrices[i][9+j];
        for(k=0;k<4;k++)if(!isfinite(row[k]))return RF_RANGE;
    }
    ++retained_draw_count;rf_xbox_retained_models[0]=retained_draw_count;return RF_OK;
fallback:
    ++rf_xbox_retained_models[6];return RF_NOT_FOUND;
}
static int retained_model_render(uint32_t request,const rf_materials *materials,const gpu_texture *textures,uint32_t install_program)
{
    const uint32_t program[]={
#include "skinned_vertex.inl"
    };
    const retained_model_draw *draw=retained_draws+request;const retained_model_entry *entry=retained_models+draw->entry;
    const gpu_texture *texture=draw->material<materials->count && textures[draw->material].pixels?textures+draw->material:textures+materials->count;
    const gpu_texture *white=textures+materials->count;uint32_t i,part_index,*p;
    if(install_program)vertex_program(program,sizeof(program)/4);
    p=pb_begin();p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96);
    pb_push(p++,NV097_SET_TRANSFORM_CONSTANT,12);memcpy(p,draw->rows,48);p+=12;
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,draw->screen[0],draw->screen[1],draw->screen[2],draw->screen[3]);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,(1000.0f/999.9f)*16777215.0f,.1f,1,0);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,.5f,.5f,.5f,1);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,0,1,0,0); /* Cg literal c6. */
    pb_end(p);
    p=pb_begin();p=pb_push1(p,NV097_SET_FRONT_FACE,draw->front_face);
    p=pb_push1(p,NV097_SET_CULL_FACE,NV097_SET_CULL_FACE_V_BACK);
    p=pb_push1(p,NV097_SET_BLEND_ENABLE,texture->transparent);p=pb_push1(p,NV097_SET_DEPTH_MASK,!texture->transparent);
    p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)texture->pixels&0x03ffffff);p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture->format);
    p=pb_push1(p,NV097_SET_TEXTURE_OFFSET+0x40,(uint32_t)white->pixels&0x03ffffff);p=pb_push1(p,NV097_SET_TEXTURE_FORMAT+0x40,white->format);
    for(i=0;i<16;i++)p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+4*i,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F);
    {const uint32_t attribute[4]={0,1,10,9},offset[4]={0,12,28,44},size[4]={3,4,4,2};
     for(i=0;i<4;i++) {
        p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+attribute[i]*4,
            field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F)|
            field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE,size[i])|field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE,sizeof(retained_skin_vertex)));
        p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_OFFSET+attribute[i]*4,((uint32_t)entry->gpu+offset[i])&0x03ffffff);
     }}
    pb_end(p);
    for(part_index=0;part_index<entry->part_count;part_index++) {
        const retained_skin_part *part=entry->parts+part_index;float palette[84][4];uint32_t start=part->start,remaining=part->count;
        for(i=0;i<part->bones;i++)memcpy(palette+i*3,draw->palette+part->ids[i]*3,48);
        for(i=0;i<part->bones*3;) {
            /* The constant method window is 32 dwords (eight float4 rows). */
            uint32_t count=part->bones*3-i;if(count>8)count=8;
            p=pb_begin();p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,104+i);
            pb_push(p++,NV097_SET_TRANSFORM_CONSTANT,count*4);memcpy(p,palette+i,count*16);p+=count*4;pb_end(p);i+=count;
        }
        p=pb_begin();p=pb_push1(p,NV097_SET_CULL_FACE_ENABLE,!part->two_sided);
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);pb_end(p);
        while(remaining) {
            uint32_t count=remaining>252?252:remaining;p=pb_begin();
            p=pb_push1(p,0x40000000|NV097_DRAW_ARRAYS,field(NV097_DRAW_ARRAYS_COUNT,count-1)|field(NV097_DRAW_ARRAYS_START_INDEX,start));
            pb_end(p);start+=count;remaining-=count;
        }
        p=pb_begin();p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);pb_end(p);
    }
    ++rf_xbox_retained_models[1];rf_xbox_retained_models[4]+=entry->vertices;rf_xbox_retained_models[5]+=entry->part_count;return RF_OK;
}
