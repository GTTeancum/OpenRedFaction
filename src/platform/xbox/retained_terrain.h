/* Private renderer implementation; included once after retained_movers.h.
 *
 * Retained carved terrain (port-owned). The CPU path (preview.c geomod_lit)
 * clipped, projected and lit every carved face every frame (~9 ms while the
 * Driller digs in ctf06). Here each source mesh is expanded once per change
 * into world-space fan triangles in GPU memory and drawn with the retained
 * world program, like the static world. Per-face eligibility, lightmap
 * selection, UV/lightmap-UV and vertex colours follow generate_source for
 * generated meshes exactly; the per-frame back-face test is the same
 * rf_preview_plane_visible check; clipping is left to the GPU as for the
 * static world. Shading constants reproduce the CPU vertex rules:
 * textured non-vertex-lit faces use colour 1, vertex-lit faces keep their
 * colour, and unlit/vertex-lit faces are halved. */
#define RETAINED_TERRAIN_SLOTS 5u
#define RETAINED_TERRAIN_BYTES (2u*1024u*1024u)
typedef struct retained_terrain_face {
    uint32_t start,count,material,lightmap,vertex_lit;float plane[4];
} retained_terrain_face;
static struct {
    const void *vertices,*faces,*bound,*colors,*authored,*bindings;
    uint32_t vertex_count,face_count,generation,material_count,hash,ready,active;
    uint32_t count,faces_used,bytes;
    rf_preview_vertex *gpu;retained_terrain_face *table;
} retained_terrain[RETAINED_TERRAIN_SLOTS];
/* builds, frames drawn, faces drawn last frame, vertices resident, declines, GPU bytes, data changes, errors */
uint32_t rf_xbox_retained_terrain[8];
static uint32_t terrain_hash_words(uint32_t h,const void *data,uint32_t bytes)
{
    const unsigned char *b=data;uint32_t i,w;
    for(i=0;i+4<=bytes;i+=4){memcpy(&w,b+i,4);h=(h^w)*16777619u;}
    for(;i<bytes;i++)h=(h^b[i])*16777619u;
    return h;
}
static void retained_terrain_release(uint32_t slot)
{
    if(retained_terrain[slot].gpu)rf_xbox_gpu_free(retained_terrain[slot].gpu);
    free(retained_terrain[slot].table);
    memset(&retained_terrain[slot],0,sizeof(retained_terrain[slot]));
}
static void retained_terrain_close(void)
{uint32_t i;for(i=0;i<RETAINED_TERRAIN_SLOTS;i++)retained_terrain_release(i);}
/* Same input contract and errors as preview.c geomod_pose_lit. */
static int retained_terrain_validate(const rf_geomod_mesh_view *source,const rf_collision_face *bound,
    uint32_t material_count,const float (*vertex_colors)[3],const rf_preview_surface_lightmap *bindings)
{
    uint32_t i,j;
    if((source->vertex_count && !source->vertices) || (source->face_count && (!source->faces || !bound)))return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(retained_world.position[i]))return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(retained_world.orientation[i][j]))return RF_FORMAT;
    }
    for(i=0;i<source->vertex_count;i++) {
        for(j=0;j<3;j++)if(!isfinite(vertex_colors[i][j]) || vertex_colors[i][j]<0 || vertex_colors[i][j]>1)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(source->vertices[i].position[j]))return RF_FORMAT;
        for(j=0;j<2;j++)if(!isfinite(source->vertices[i].uv[j]))return RF_FORMAT;
    }
    for(i=0;i<source->face_count;i++) {
        const rf_geomod_face *f=source->faces+i;
        if(bindings && f->source_face==UINT32_MAX && bindings[i].image!=UINT32_MAX) {
            const rf_lightmap_projection *p=&bindings[i].projection;
            if(bindings[i].image>=RF_PREVIEW_VERTEX_LIT || p->axes[0]>2 || p->axes[1]>2 || p->axes[0]==p->axes[1])return RF_FORMAT;
            for(j=0;j<2;j++)if(!isfinite(p->scale[j]) || !isfinite(p->offset[j]))return RF_FORMAT;
        }
        if(f->count<3 || f->count>64 || f->first>source->vertex_count || f->count>source->vertex_count-f->first ||
           f->material>=material_count || bound[i].count!=f->count)return RF_FORMAT;
        for(j=0;j<4;j++)if(!isfinite(bound[i].plane[j]))return RF_FORMAT;
    }
    return RF_OK;
}
static uint32_t retained_terrain_data_hash(const rf_geomod_mesh_view *s,const rf_collision_face *bound,
    const float (*colors)[3],const rf_preview_surface_lightmap *bindings)
{
    uint32_t h=2166136261u,i;
    h=terrain_hash_words(h,s->vertices,s->vertex_count*(uint32_t)sizeof(*s->vertices));
    h=terrain_hash_words(h,s->faces,s->face_count*(uint32_t)sizeof(*s->faces));
    for(i=0;i<s->face_count;i++){h=terrain_hash_words(h,bound[i].plane,16);h=(h^bound[i].count)*16777619u;}
    h=terrain_hash_words(h,colors,s->vertex_count*12u);
    if(bindings)h=terrain_hash_words(h,bindings,s->face_count*(uint32_t)sizeof(*bindings));
    return h;
}
static int retained_terrain_build(uint32_t slot,const rf_geomod_mesh_view *s,const rf_collision_face *bound,
    const float (*colors)[3],const rf_geometry *authored,const rf_preview_surface_lightmap *bindings)
{
    uint32_t f,count=0,at=0,used=0;rf_preview_vertex *gpu;retained_terrain_face *table;int status;
    for(f=0;f<s->face_count;f++)count+=(s->faces[f].count-2)*3;
    if(!count || (uint64_t)count*sizeof(rf_preview_vertex)>RETAINED_TERRAIN_BYTES)return RF_NOT_FOUND;
    table=calloc(s->face_count,sizeof(*table));
    gpu=MmAllocateContiguousMemoryEx(count*sizeof(rf_preview_vertex),0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
    if(!table || !gpu){free(table);if(gpu)MmFreeContiguousMemory(gpu);return RF_NOT_FOUND;}
    for(f=0;f<s->face_count;f++) {
        const rf_geomod_face *face=s->faces+f;uint32_t mapping=UINT32_MAX,lightmap=UINT32_MAX,mapped=0,corner,j;
        rf_lightmap_projection projection;retained_terrain_face *out=table+used;
        if(authored && face->source_face!=UINT32_MAX) {
            rf_geometry_face parent;status=rf_geometry_get_face(authored,face->source_face,&parent);if(status)goto failed;
            if(parent.room<authored->rooms && authored->data[authored->room_offsets[parent.room]+28])continue;
            mapping=parent.lightmap_mapping;
            if(mapping!=UINT32_MAX){status=rf_geometry_lightmap_projection(authored,mapping,&projection);if(status)goto failed;mapped=1;}
        }
        if(face->source_face==UINT32_MAX && bindings && bindings[f].image!=UINT32_MAX){projection=bindings[f].projection;lightmap=bindings[f].image;mapped=2;}
        if(mapped!=2 && mapping!=UINT32_MAX && rf_geometry_lightmap(authored,mapping,UINT32_MAX,&lightmap)){status=RF_FORMAT;goto failed;}
        out->start=at;out->count=(face->count-2)*3;out->material=face->material;
        out->vertex_lit=!mapped;out->lightmap=mapped?lightmap:RF_PREVIEW_VERTEX_LIT;
        memcpy(out->plane,bound[f].plane,16);
        for(corner=1;corner+1<face->count;corner++) {
            uint32_t indices[3]={0,corner,corner+1};
            for(j=0;j<3;j++) {
                uint32_t index=face->first+indices[j];rf_preview_vertex v={0};float lightmap_uv[2]={0,0};
                memcpy(v.position,s->vertices[index].position,12);
                if(mapped){status=rf_lightmap_project(&projection,s->vertices[index].position,lightmap_uv);if(status)goto failed;}
                v.texture[0]=s->vertices[index].uv[0];v.texture[1]=s->vertices[index].uv[1];v.texture[2]=1;
                v.lightmap_texture[0]=lightmap_uv[0];v.lightmap_texture[1]=lightmap_uv[1];v.lightmap_texture[2]=1;
                memcpy(v.color,colors[index],12);v.material=out->material;v.lightmap=out->lightmap;
                gpu[at++]=v; /* Write once; never read WC memory. */
            }
        }
        ++used;
    }
    __asm__ volatile("sfence" ::: "memory");
    if(retained_terrain[slot].gpu)rf_xbox_gpu_free(retained_terrain[slot].gpu); /* GPU may still read last frame's copy. */
    free(retained_terrain[slot].table);
    retained_terrain[slot].gpu=gpu;retained_terrain[slot].table=table;retained_terrain[slot].count=at;
    retained_terrain[slot].faces_used=used;retained_terrain[slot].bytes=count*(uint32_t)sizeof(rf_preview_vertex);
    ++rf_xbox_retained_terrain[0];return RF_OK;
failed:
    free(table);MmFreeContiguousMemory(gpu);++rf_xbox_retained_terrain[7];return status;
}
/* slot UINT32_MAX: the world is being rebuilt; forget last build's slots.
 * RF_OK: drawn by the GPU pass this frame; RF_NOT_FOUND: use the CPU path. */
static int retained_terrain_prepare(uint32_t slot,const rf_geomod_mesh_view *source,const rf_collision_face *bound,
    uint32_t material_count,const float (*vertex_colors)[3],const rf_geometry *authored,
    const rf_preview_surface_lightmap *bindings)
{
    uint32_t i,hash;int status;
    if(slot==UINT32_MAX){for(i=0;i<RETAINED_TERRAIN_SLOTS;i++)retained_terrain[i].active=0;return RF_OK;}
    if(slot>=RETAINED_TERRAIN_SLOTS || !source || !vertex_colors || !authored)return RF_RANGE;
    if(retained_world.ready!=1 || (rf_xbox_experiment&8)){++rf_xbox_retained_terrain[4];return RF_NOT_FOUND;}
    status=retained_terrain_validate(source,bound,material_count,vertex_colors,bindings);if(status)return status;
    hash=retained_terrain_data_hash(source,bound,vertex_colors,bindings);
    if(!retained_terrain[slot].ready || retained_terrain[slot].vertices!=source->vertices || retained_terrain[slot].faces!=source->faces ||
       retained_terrain[slot].bound!=bound || retained_terrain[slot].colors!=vertex_colors || retained_terrain[slot].authored!=authored ||
       retained_terrain[slot].bindings!=bindings || retained_terrain[slot].vertex_count!=source->vertex_count ||
       retained_terrain[slot].face_count!=source->face_count || retained_terrain[slot].generation!=source->generation ||
       retained_terrain[slot].material_count!=material_count || retained_terrain[slot].hash!=hash) {
        ++rf_xbox_retained_terrain[6];
        retained_terrain[slot].ready=0;
        status=retained_terrain_build(slot,source,bound,vertex_colors,authored,bindings);
        if(status==RF_NOT_FOUND){retained_terrain_release(slot);++rf_xbox_retained_terrain[4];return RF_NOT_FOUND;}
        if(status)return status;
        retained_terrain[slot].vertices=source->vertices;retained_terrain[slot].faces=source->faces;retained_terrain[slot].bound=bound;
        retained_terrain[slot].colors=vertex_colors;retained_terrain[slot].authored=authored;retained_terrain[slot].bindings=bindings;
        retained_terrain[slot].vertex_count=source->vertex_count;retained_terrain[slot].face_count=source->face_count;
        retained_terrain[slot].generation=source->generation;retained_terrain[slot].material_count=material_count;
        retained_terrain[slot].hash=hash;retained_terrain[slot].ready=1;
    }
    retained_terrain[slot].active=1;
    {uint32_t total=0,bytes=0;for(i=0;i<RETAINED_TERRAIN_SLOTS;i++)if(retained_terrain[i].ready){total+=retained_terrain[i].count;bytes+=retained_terrain[i].bytes;}
     rf_xbox_retained_terrain[3]=total;rf_xbox_retained_terrain[5]=bytes;}
    return RF_OK;
}
static int retained_terrain_draw(const rf_materials *materials,const rf_lightmaps *lightmaps,const gpu_texture *textures)
{
    uint32_t slot,f,*p,drawn=0,any=0,k;
    for(slot=0;slot<RETAINED_TERRAIN_SLOTS;slot++)if(retained_terrain[slot].active && retained_terrain[slot].ready)any=1;
    rf_xbox_retained_terrain[2]=0;
    if(!any || (rf_xbox_experiment&1))return RF_OK;
    retained_world_setup();
    p=pb_begin();p=pb_push1(p,NV097_SET_CULL_FACE_ENABLE,0);pb_end(p);
    for(slot=0;slot<RETAINED_TERRAIN_SLOTS;slot++) {
        const rf_preview_vertex *gpu=retained_terrain[slot].gpu;
        if(!retained_terrain[slot].active || !retained_terrain[slot].ready)continue;
        p=pb_begin();
        for(k=0;k<4;k++) {
            uint32_t attribute=k==0?0:k==1?3:k==2?9:10;
            p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_OFFSET+attribute*4,((uint32_t)gpu+(k==3?40:k*12))&0x03ffffff);
        }
        pb_end(p);
        for(f=0;f<retained_terrain[slot].faces_used;f++) {
            const retained_terrain_face *face=retained_terrain[slot].table+f;
            const gpu_texture *texture,*lighting;uint32_t textured,start,count,lit;float shade_color,shade_bias,scale;
            if(!rf_preview_plane_visible(face->plane,retained_world.position))continue;
            textured=face->material<materials->count && textures[face->material].pixels;
            texture=textured?textures+face->material:textures+materials->count;
            lit=face->lightmap<lightmaps->count;
            lighting=lit?textures+materials->count+1+face->lightmap:textures+materials->count;
            /* CPU stream: colour=1 for textured faces unless vertex-lit, then
             * x0.5 when the tag is UINT32_MAX or vertex-lit. */
            shade_color=(face->vertex_lit || !textured)?1.f:0.f;shade_bias=1.f-shade_color;
            scale=(face->vertex_lit || face->lightmap==UINT32_MAX)?.5f:1.f;
            p=pb_begin();
            p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)texture->pixels&0x03ffffff);
            p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture->format);p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,TEXTURE_CONTROL(texture));
            p=pb_push1(p,NV097_SET_TEXTURE_OFFSET+0x40,(uint32_t)lighting->pixels&0x03ffffff);
            p=pb_push1(p,NV097_SET_TEXTURE_FORMAT+0x40,lighting->format);
            p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,99);
            p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,shade_color,shade_bias,scale,.1f);
            p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);
            for(start=face->start,count=face->count;count;) {
                uint32_t batch=count>252?252:count;
                p=pb_push1(p,0x40000000|NV097_DRAW_ARRAYS,field(NV097_DRAW_ARRAYS_COUNT,batch-1)|field(NV097_DRAW_ARRAYS_START_INDEX,start));
                count-=batch;start+=batch;
            }
            p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);pb_end(p);++drawn;
        }
    }
    rf_xbox_retained_terrain[2]=drawn;++rf_xbox_retained_terrain[1];return RF_OK;
}
