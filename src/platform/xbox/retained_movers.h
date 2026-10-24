/* Private GPU-retained mover (brush solid) geometry for the Xbox renderer.
 * Included by renderer.c after retained_world_draw; uses its file-static
 * helpers (retained_world, vertex_program, renderer_reserve, field, ...).
 *
 * Scaffolding, not reconstructed original-renderer behaviour. The CPU path
 * (src/core/preview.c world_mesh -> mover_render_family -> generate) projects
 * every face of a visible mover each drawn frame. This keeps each mover's
 * faces once in contiguous write-combined memory, in mover-local space, with
 * the same rf_preview_vertex fields as the retained static world, and draws
 * them with world_vertex.vs.cg using camera-from-local rows built from the
 * mover's current world pose. preview.c still does the hidden/suppressed/
 * frustum decisions (mover_render_outside) before calling submit.
 *
 * Equivalence with the CPU mover path (generate_source with origin set):
 * - same face filter: portal, flags&0x2001, texture UINT32_MAX skipped; no
 *   room visibility, sky-room or backface test for movers; face order kept
 *   (no material sort), so coplanar depth ties resolve as before;
 * - material = materials->slots[offsets[mover+1]+face.texture], lightmap from
 *   rf_geometry_lightmap on the mover geometry, corner UV / lightmap UV;
 * - opaque state: mover vertices are in the CPU world prefix (index below
 *   world_vertices), which preview() draws with blend off and depth writes on;
 * - any face/vertex the CPU path would reject (bad texture index, lightmap
 *   mapping, non-finite plane or vertex) leaves that mover on the CPU path,
 *   which then reports the identical error.
 * Known difference (unverified visually): vertex RGB is the directional shade
 * of the authored pose normal, not the live pose. world_vertex ignores vertex
 * RGB for textured materials (shade.x=0), so only untextured fallback faces
 * on rotating movers can differ. GPU clipping replaces CPU six-plane
 * clipping exactly as it already does for the static world. */
#define RETAINED_MOVER_LIMIT 256u
#define RETAINED_MOVER_BYTES (1024u*1024u) /* GPU vertex cap; rest stays CPU */
typedef struct retained_mover_group { uint32_t start,count,material,lightmap; } retained_mover_group;
typedef struct retained_mover_entry { uint32_t first_group,group_count,vertices,ready; } retained_mover_entry;
typedef struct retained_mover_pose { uint32_t mover;float origin[3],matrix[3][3]; } retained_mover_pose;
static struct {
    const void *items,*offsets,*slots;uint32_t count;int state; /* 0 empty, 1 uploaded, -1 failed */
    rf_preview_vertex *vertices;uint32_t vertex_count;
    retained_mover_entry *movers;retained_mover_group *groups;uint32_t group_count;
    retained_mover_pose pending[RETAINED_MOVER_LIMIT],published[RETAINED_MOVER_LIMIT];
    uint32_t pending_count,published_count;
} retained_movers;
/* state, GPU bytes, descriptor bytes, retained movers, CPU-only movers,
 * published poses, movers drawn last frame, vertices drawn last frame. */
uint32_t rf_xbox_retained_movers[8];
static void retained_movers_close(void)
{
    if(retained_movers.vertices){if(stream_device_ready)while(pb_busy()){} MmFreeContiguousMemory(retained_movers.vertices);}
    free(retained_movers.movers);free(retained_movers.groups);
    memset(&retained_movers,0,sizeof(retained_movers));
    memset(rf_xbox_retained_movers,0,sizeof(rf_xbox_retained_movers));
}
/* One mover: count (write NULL) or write its faces. Returns RF_OK with
 * *usable=0 when the CPU path must keep the mover (it reproduces any error). */
static int retained_mover_scan(const rf_geometry_mover *m,const uint32_t *slots,
    rf_preview_vertex *write,retained_mover_group *groups,uint32_t base,
    uint32_t *vertices,uint32_t *group_count,uint32_t *usable)
{
    const rf_geometry *g=&m->geometry;uint32_t f,at=0,used=0,material=UINT32_MAX,lightmap_bound=UINT32_MAX;
    *usable=0;*vertices=0;*group_count=0;
    if(!g->data)return RF_OK;
    for(f=0;f<g->faces;f++) {
        rf_geometry_face face;uint32_t lightmap=UINT32_MAX,corner,i,slot;float normal[3],color;
        if(rf_geometry_get_face(g,f,&face))return RF_OK;
        if(face.portal || (face.flags&0x2001u) || face.texture==UINT32_MAX)continue;
        if(face.texture>=g->textures)return RF_OK;
        if(face.lightmap_mapping!=UINT32_MAX && rf_geometry_lightmap(g,face.lightmap_mapping,UINT32_MAX,&lightmap))return RF_OK;
        for(i=0;i<4;i++)if(!isfinite(face.plane[i]))return RF_OK;
        if(face.corners<3)continue;
        /* at never exceeds the cap, so this bounds the sum without overflow. */
        if(face.corners-2>(RETAINED_MOVER_BYTES/sizeof(rf_preview_vertex)-at)/3)return RF_OK;
        slot=slots[face.texture];
        if(!used || slot!=material || lightmap!=lightmap_bound) {
            if(write){groups[used].start=base+at;groups[used].count=0;groups[used].material=slot;groups[used].lightmap=lightmap;}
            ++used;material=slot;lightmap_bound=lightmap;
        }
        /* Authored-pose directional shade, as generate_source with the file
         * pose; textured draws ignore it in world_vertex. */
        for(i=0;i<3;i++)normal[i]=face.plane[0]*m->orientation[0][i]+face.plane[1]*m->orientation[1][i]+face.plane[2]*m->orientation[2][i];
        color=.25f+.6f*fabsf(normal[0]*.3f+normal[1]*.8f+normal[2]*.5f);if(color>1)color=1;
        for(corner=1;corner+1<face.corners;corner++) {
            uint32_t j,indices[3]={0,corner,corner+1};
            for(j=0;j<3;j++) {
                rf_geometry_corner c;rf_preview_vertex v={0};
                if(rf_geometry_get_corner(g,f,indices[j],&c) || rf_geometry_vertex(g,c.vertex,v.position))return RF_OK;
                if(!isfinite(v.position[0]) || !isfinite(v.position[1]) || !isfinite(v.position[2]))return RF_OK;
                if(write) {
                    v.texture[0]=c.uv[0];v.texture[1]=c.uv[1];v.texture[2]=1;
                    v.lightmap_texture[0]=c.lightmap_uv[0];v.lightmap_texture[1]=c.lightmap_uv[1];v.lightmap_texture[2]=1;
                    v.color[0]=color;v.color[1]=color*.85f;v.color[2]=color*.65f;
                    v.material=slot;v.lightmap=lightmap;
                    write[base+at]=v; /* Write once; never read WC memory. */
                }
                ++at;
            }
        }
        if(write)groups[used-1].count+=(face.corners-2)*3;
    }
    *vertices=at;*group_count=used;*usable=1;return RF_OK;
}
static int retained_movers_upload(const rf_geometry_movers *movers,const rf_geometry_materials *materials)
{
    uint32_t m,total=0,groups=0,at=0,group_at=0,vertices,count,usable;uint64_t descriptor;
    retained_movers_close();
    retained_movers.items=movers->items;retained_movers.count=movers->count;
    retained_movers.offsets=materials->offsets;retained_movers.slots=materials->slots;retained_movers.state=-1;
    if(!movers->count || movers->count>RETAINED_MOVER_LIMIT || !materials->slots)return RF_NOT_FOUND;
    retained_movers.movers=calloc(movers->count,sizeof(retained_mover_entry));
    if(!retained_movers.movers)return RF_NOT_FOUND;
    for(m=0;m<movers->count;m++) {
        retained_mover_entry *e=retained_movers.movers+m;
        retained_mover_scan(movers->items+m,materials->slots+materials->offsets[m+1],NULL,NULL,0,&vertices,&count,&usable);
        if(!usable || vertices>RETAINED_MOVER_BYTES/sizeof(rf_preview_vertex)-total)continue;
        e->ready=1;e->vertices=vertices;e->group_count=count;total+=vertices;groups+=count;
    }
    descriptor=(uint64_t)groups*sizeof(retained_mover_group)+(uint64_t)movers->count*sizeof(retained_mover_entry);
    if(groups)retained_movers.groups=malloc(groups*sizeof(retained_mover_group));
    if(total)retained_movers.vertices=MmAllocateContiguousMemoryEx(total*sizeof(rf_preview_vertex),0,0x03ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
    if((groups && !retained_movers.groups) || (total && !retained_movers.vertices)) {
        free(retained_movers.groups);retained_movers.groups=NULL;
        if(retained_movers.vertices)MmFreeContiguousMemory(retained_movers.vertices);retained_movers.vertices=NULL;
        for(m=0;m<movers->count;m++)retained_movers.movers[m].ready=0;
        return RF_NOT_FOUND;
    }
    for(m=0;m<movers->count;m++) {
        retained_mover_entry *e=retained_movers.movers+m;
        if(!e->ready)continue;
        retained_mover_scan(movers->items+m,materials->slots+materials->offsets[m+1],retained_movers.vertices,
            retained_movers.groups+group_at,at,&vertices,&count,&usable);
        if(!usable || vertices!=e->vertices || count!=e->group_count){e->ready=0;continue;} /* left unused; still CPU */
        e->first_group=group_at;group_at+=count;at+=vertices;
    }
    __asm__ volatile("sfence" ::: "memory");
    retained_movers.vertex_count=total;retained_movers.group_count=group_at;retained_movers.state=1;
    rf_xbox_retained_movers[0]=1;rf_xbox_retained_movers[1]=total*sizeof(rf_preview_vertex);
    rf_xbox_retained_movers[2]=(uint32_t)descriptor;
    for(m=0;m<movers->count;m++)++rf_xbox_retained_movers[retained_movers.movers[m].ready?3:4];
    return RF_OK;
}
static int retained_movers_begin(void *context,const rf_geometry_movers *movers,const rf_geometry_materials *materials)
{
    (void)context;retained_movers.pending_count=0;
    /* Draw happens only beside the retained world (preview(), model 4). */
    if(!movers || !materials || retained_world.ready!=1)return RF_NOT_FOUND;
    if(retained_movers.items!=movers->items || retained_movers.count!=movers->count ||
       retained_movers.offsets!=materials->offsets || retained_movers.slots!=materials->slots)
        (void)retained_movers_upload(movers,materials);
    return retained_movers.state==1?RF_OK:RF_NOT_FOUND;
}
static int retained_movers_submit(void *context,uint32_t mover,const float origin[3],const float matrix[3][3])
{
    retained_mover_pose *p;(void)context;
    if(retained_movers.state!=1 || mover>=retained_movers.count || !retained_movers.movers[mover].ready)return RF_NOT_FOUND;
    if(retained_movers.pending_count>=RETAINED_MOVER_LIMIT)return RF_RANGE; /* count<=limit: unreachable */
    p=retained_movers.pending+retained_movers.pending_count++;
    p->mover=mover;memcpy(p->origin,origin,12);memcpy(p->matrix,matrix,36);return RF_OK;
}
static void retained_movers_finish(void *context,int status)
{
    (void)context;
    if(status==RF_OK) {
        memcpy(retained_movers.published,retained_movers.pending,retained_movers.pending_count*sizeof(retained_mover_pose));
        retained_movers.published_count=retained_movers.pending_count;rf_xbox_retained_movers[5]=retained_movers.published_count;
    }
    retained_movers.pending_count=0;
}
static void retained_movers_clear(void *context)
{(void)context;retained_movers.pending_count=retained_movers.published_count=0;rf_xbox_retained_movers[5]=0;}
static void retained_movers_enable(void)
{
    static const rf_preview_mover_backend backend={retained_movers_begin,retained_movers_submit,
        retained_movers_finish,retained_movers_clear,NULL};
    (void)rf_preview_set_mover_backend(&backend);
}
/* Called right after retained_world_draw, before preview() restores its own
 * program. Installs the complete world_vertex state itself, so it does not
 * depend on whether the world took the indexed or array path. */
static int retained_movers_draw(const rf_materials *materials,const rf_lightmaps *lightmaps,const gpu_texture *textures)
{
    const uint32_t program[]={
#include "world_vertex.inl"
    };
    uint32_t i,j,k,n,*p,active=0,bound_material=UINT32_MAX,bound_lightmap=UINT32_MAX,drawn=0,vertices=0;
    float camera[3][3];renderer_command_batch commands={NULL,NULL,rf_xbox_command_blocks};
    rf_xbox_retained_movers[6]=rf_xbox_retained_movers[7]=0;
    if(retained_movers.state!=1 || !retained_movers.published_count || retained_world.ready!=1 || (rf_xbox_experiment&1))return RF_OK;
    if(!isfinite(rf_scene_scope_projection) || rf_scene_scope_projection<1)return RF_RANGE;
    for(i=0;i<3;i++)for(j=0;j<3;j++)camera[i][j]=retained_world.orientation[i][j]*(i<2?rf_scene_scope_projection:1);
    vertex_program(program,sizeof(program)/4);
    p=pb_begin();
    p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,100);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,320,240,(1000.0f/999.9f)*16777215.0f,1);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,0,0,0,0); /* Cg literal c5. */
    p=pb_push1(p,NV097_SET_BLEND_ENABLE,0);p=pb_push1(p,NV097_SET_DEPTH_MASK,1);
    p=pb_push1(p,NV097_SET_CONTROL0,NV097_SET_CONTROL0_Z_FORMAT_FIXED|NV097_SET_CONTROL0_TEXTURE_PERSPECTIVE_ENABLE);
    for(i=0;i<16;i++)p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+4*i,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F);
    for(i=0;i<4;i++) {
        uint32_t attribute=i==0?0:i==1?3:i==2?9:10;
        p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+attribute*4,
            field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE,NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F)|
            field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE,3)|field(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE,sizeof(rf_preview_vertex)));
        p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_OFFSET+attribute*4,((uint32_t)retained_movers.vertices+(i==3?40:i*12))&0x03ffffff);
    }
    p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS,0x00010101);p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,NV097_SET_TEXTURE_CONTROL0_ENABLE);
    p=pb_push1(p,NV097_SET_TEXTURE_FILTER,RF_TEXTURE_FILTER_LINEAR);p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS+0x40,0x00030303);
    p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0+0x40,NV097_SET_TEXTURE_CONTROL0_ENABLE);p=pb_push1(p,NV097_SET_TEXTURE_FILTER+0x40,RF_TEXTURE_FILTER_LINEAR);pb_end(p);
    for(n=0;n<retained_movers.published_count;n++) {
        const retained_mover_pose *pose=retained_movers.published+n;const retained_mover_entry *e;float rows[3][4];
        if(pose->mover>=retained_movers.count)continue;
        e=retained_movers.movers+pose->mover;if(!e->ready || !e->group_count)continue;
        /* camera-from-local = C*(origin + sum_j local_j*axis_j - eye):
         * rows_ij = C_i.axis_j, rows_i3 = C_i.(origin-eye). X/Y rows carry
         * the scope scale, as retained_world_draw scales its rows. */
        for(i=0;i<3;i++) {
            for(j=0;j<3;j++){rows[i][j]=0;for(k=0;k<3;k++)rows[i][j]+=camera[i][k]*pose->matrix[j][k];}
            rows[i][3]=0;for(k=0;k<3;k++)rows[i][3]+=camera[i][k]*(pose->origin[k]-retained_world.position[k]);
        }
        if(active){p=renderer_reserve(&commands,2);commands.p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);active=0;++rf_xbox_command_blocks[1];}
        p=renderer_reserve(&commands,15);p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96);
        pb_push(p++,NV097_SET_TRANSFORM_CONSTANT,12);memcpy(p,rows,sizeof(rows));p+=12;commands.p=p;
        for(i=0;i<e->group_count;i++) {
            const retained_mover_group *group=retained_movers.groups+e->first_group+i;
            uint32_t start=group->start,count=group->count,material=group->material,lightmap=group->lightmap;
            if(!count)continue;
            if(material!=bound_material || lightmap!=bound_lightmap) {
                uint32_t textured=material<materials->count && textures[material].pixels;
                const gpu_texture *texture=textured?textures+material:textures+materials->count;
                /* CPU-stream lookup: an out-of-range image falls back, no error. */
                const gpu_texture *lighting=lightmap<lightmaps->count?textures+materials->count+1+lightmap:textures+materials->count;
                if(active){p=renderer_reserve(&commands,2);commands.p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);active=0;++rf_xbox_command_blocks[1];}
                p=renderer_reserve(&commands,15);p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)texture->pixels&0x03ffffff);
                p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture->format);
                p=pb_push1(p,NV097_SET_TEXTURE_OFFSET+0x40,(uint32_t)lighting->pixels&0x03ffffff);
                p=pb_push1(p,NV097_SET_TEXTURE_FORMAT+0x40,lighting->format);
                p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,99);
                p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,textured?0:1,textured?1:0,lightmap==UINT32_MAX?.5f:1,.1f);
                commands.p=p;bound_material=material;bound_lightmap=lightmap;
            }
            if(!active){p=renderer_reserve(&commands,2);commands.p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);active=1;++rf_xbox_command_blocks[1];}
            while(count) {
                uint32_t batch=count>252?252:count;p=renderer_reserve(&commands,2);
                p=pb_push1(p,0x40000000|NV097_DRAW_ARRAYS,field(NV097_DRAW_ARRAYS_COUNT,batch-1)|field(NV097_DRAW_ARRAYS_START_INDEX,start));commands.p=p;
                count-=batch;start+=batch;vertices+=batch;
            }
        }
        ++drawn;
    }
    if(active){p=renderer_reserve(&commands,2);commands.p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);++rf_xbox_command_blocks[1];}
    renderer_flush(&commands);
    rf_xbox_retained_movers[6]=drawn;rf_xbox_retained_movers[7]=vertices;return RF_OK;
}
