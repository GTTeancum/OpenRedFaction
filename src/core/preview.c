#include "rf/preview.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef struct point { float x, y, z, u, v, lu, lv; } point;
/* Last world-projection capacity failure: valid, face, fan corner, used,
 * capacity (vertices), geometry face count, writing pass, required vertices. */
uint32_t rf_preview_failure[8];
uint32_t rf_preview_plane_visible(const float plane[4],const float viewer[3])
{
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned short saved,control,comparison;
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        mov ecx,plane
        mov edx,viewer
        fld dword ptr [ecx+8]
        fmul dword ptr [edx+8]
        fld dword ptr [ecx+4]
        fmul dword ptr [edx+4]
        faddp st(1),st(0)
        fld dword ptr [ecx]
        fmul dword ptr [edx]
        faddp st(1),st(0)
        fadd dword ptr [ecx+12]
        fldz
        fxch st(1)
        fcompp
        fnstsw comparison
        fldcw saved
    }
    return (comparison&0x4100u)==0;
#elif defined(__i386__)
    unsigned short saved,control,comparison;
    __asm__ volatile("fnstcw %0":"=m"(saved));control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %0"::"m"(control));
    __asm__ volatile("flds 8(%1); fmuls 8(%2); flds 4(%1); fmuls 4(%2); faddp; flds (%1); fmuls (%2); faddp; fadds 12(%1); fldz; fxch; fcompp; fnstsw %0"
        :"=m"(comparison):"r"(plane),"r"(viewer):"st","st(1)");
    __asm__ volatile("fldcw %0"::"m"(saved));return (comparison&0x4100u)==0;
#else
    return (((long double)plane[2]*viewer[2]+(long double)plane[1]*viewer[1])+
        (long double)plane[0]*viewer[0])+plane[3]>0;
#endif
}
/* Keep clipping arithmetic at float precision on both SSE and x87 builds. */
static float interpolate(float a,float b,float t)
{
    volatile float delta=b-a,scaled=t*delta;
    return a+scaled;
}
static float distance(point p, unsigned plane)
{
    volatile float vertical=p.z*0.75f;
    switch (plane) {
    case 0: return p.z - 0.1f;
    case 1: return 1000.0f - p.z;
    case 2: return p.z + p.x;
    case 3: return p.z - p.x;
    case 4: return vertical + p.y;
    default: return vertical - p.y;
    }
}
static unsigned clip(const point *input, unsigned count, point *output, unsigned plane)
{
    unsigned i, used = 0;
    point previous = input[count - 1];
    volatile float before = distance(previous, plane);
    for (i = 0; i < count; ++i) {
        point current = input[i];
        volatile float after = distance(current, plane);
        if ((before >= 0) != (after >= 0)) {
            volatile float span=before-after,t=before/span;
            output[used++] = (point){interpolate(previous.x,current.x,t),interpolate(previous.y,current.y,t),interpolate(previous.z,current.z,t),interpolate(previous.u,current.u,t),interpolate(previous.v,current.v,t),interpolate(previous.lu,current.lu,t),interpolate(previous.lv,current.lv,t)};
        }
        if (after >= 0) output[used++] = current;
        previous = current; before = after;
    }
    return used;
}
static int camera(const rf_geometry *g, const rf_level *level, const rf_geometry_corner *corner,
    const float *origin,const float matrix[3][3],point *out)
{
    float p[3], v[3];
    unsigned i;
    point result = {0, 0, 0, 0, 0, 0, 0};
    result.u = corner->uv[0]; result.v = corner->uv[1];
    result.lu = corner->lightmap_uv[0]; result.lv = corner->lightmap_uv[1];
    if(rf_geometry_vertex(g, corner->vertex, p))return RF_FORMAT;
    if(origin) {
        rf_collision_ray_hit local={0},world;int status;memcpy(local.point,p,12);
        status=rf_collision_contact_world(&local,origin,matrix,&world);if(status)return status;memcpy(p,world.point,12);
    }
    for (i = 0; i < 3; ++i) v[i] = p[i] - level->player_position[i];
    for (i = 0; i < 3; ++i) {
        result.x += v[i] * level->player_orientation[0][i];
        result.y += v[i] * level->player_orientation[1][i];
        result.z += v[i] * level->player_orientation[2][i];
    }
    *out=result;return RF_OK;
}
static int generate(rf_preview_mesh *mesh, const rf_geometry *g, const rf_level *level, uint32_t capacity,
    const float *origin,const float matrix[3][3],uint32_t material_base)
{
    uint32_t f, used = 0;
    for (f = 0; f < g->faces; ++f) {
        rf_geometry_face face;
        rf_geometry_corner a, b, c;
        point anchor,previous;
        uint32_t corner, lightmap = UINT32_MAX;
        float color;int status;
        rf_geometry_get_face(g, f, &face);
        if (face.portal || (face.flags & 1) || face.texture == UINT32_MAX) continue;
        if(face.texture>=g->textures)return RF_FORMAT;
        if (face.lightmap_mapping != UINT32_MAX && rf_geometry_lightmap(g, face.lightmap_mapping, UINT32_MAX, &lightmap)) return RF_FORMAT;
        if(face.texture>=UINT32_MAX-material_base)return RF_RANGE;
        /* Static cached-solid branch rejects nonpositive camera/plane distance.
         * Mover-local camera preparation remains separate. */
        if(!origin && !rf_preview_plane_visible(face.plane,level->player_position))continue;
        if(origin) {
            rf_collision_ray_hit local={0},world;memcpy(local.normal,face.plane,12);
            status=rf_collision_contact_world(&local,origin,matrix,&world);if(status)return status;memcpy(face.plane,world.normal,12);
        }
        color = 0.25f + 0.6f * fabsf(face.plane[0] * 0.3f + face.plane[1] * 0.8f + face.plane[2] * 0.5f);
        if (color > 1) color = 1;
        rf_geometry_get_corner(g, f, 0, &a);
        if(face.corners<3)continue;
        rf_geometry_get_corner(g,f,1,&b);
        if((status=camera(g,level,&a,origin,matrix,&anchor)) ||
           (status=camera(g,level,&b,origin,matrix,&previous)))return status;
        for (corner = 1; corner + 1 < face.corners; ++corner) {
            point buffers[2][12];
            unsigned count = 3, plane, current = 0, i, j, crossing=0;
            /* A polygon fan reuses its anchor and the previous corner. Keep
             * their rounded camera-space values rather than transforming each
             * occurrence again. UVs belong to these same face corners. */
            rf_geometry_get_corner(g, f, corner + 1, &c);
            buffers[0][0]=anchor;buffers[0][1]=previous;
            if((status=camera(g,level,&c,origin,matrix,&buffers[0][2])))return status;
            previous=buffers[0][2];
            /* Convex frustum: triangles wholly outside one plane cannot
             * contribute, and wholly inside triangles need no polygon copies.
             * Crossing triangles retain the original six-plane clip order. */
            for(plane=0;plane<6;++plane) {
                unsigned outside=0;
                for(j=0;j<3;++j)outside+=distance(buffers[0][j],plane)<0;
                if(outside==3){count=0;break;}
                crossing|=outside;
            }
            for (plane = 0; crossing && plane < 6 && count; ++plane) {
                count = clip(buffers[current], count, buffers[1-current], plane);
                current = 1-current;
            }
            for (i = 1; i + 1 < count; ++i) {
                unsigned indices[3] = {0, i, i+1};
                if (used > capacity || capacity-used < 3) {
                    uint32_t failure[8]={1,f,corner,used,capacity,g->faces,mesh->vertices!=NULL,3};
                    memcpy(rf_preview_failure,failure,sizeof(failure));return RF_RANGE;
                }
                for (j = 0; j < 3; ++j) {
                    point p = buffers[current][indices[j]];
                    if (mesh->vertices) {
                        rf_preview_vertex *out = &mesh->vertices[used];
                        out->position[0] = 320 + p.x / p.z * 320;
                        out->position[1] = 240 - p.y / p.z * 320;
                        /* Shared raster precision: both backends receive the same
                         * 1/16-pixel grid, avoiding host-dependent edge sampling. */
                        out->position[0] = floorf(out->position[0]*16.0f)/16.0f;
                        out->position[1] = floorf(out->position[1]*16.0f)/16.0f;
                        out->position[2] = (1000.0f / 999.9f) * (1 - 0.1f / p.z) * 16777215;
                        out->color[0] = color; out->color[1] = color * 0.85f; out->color[2] = color * 0.65f;
                        out->texture[0] = p.u / p.z; out->texture[1] = p.v / p.z; out->texture[2] = 1.0f / p.z;
                        out->material = face.texture+material_base;
                        out->lightmap_texture[0] = p.lu / p.z; out->lightmap_texture[1] = p.lv / p.z; out->lightmap_texture[2] = 1.0f / p.z;
                        out->lightmap = lightmap;
                    }
                    ++used;
                }
            }
        }
    }
    mesh->count = used;
    return RF_OK;
}
static int build(rf_preview_mesh *mesh, const rf_geometry *g, const rf_level *level,
    const float *origin,const float matrix[3][3],uint32_t material_base,uint32_t budget)
{
    int result;
    uint32_t count;
    if (!mesh || !g || !g->data || !level) return RF_RANGE;
    memset(mesh, 0, sizeof(*mesh));
    result = generate(mesh, g, level, budget / sizeof(rf_preview_vertex),origin,matrix,material_base);
    if (result != RF_OK || !mesh->count) return result;
    count = mesh->count;
    mesh->bytes = count * sizeof(rf_preview_vertex);
    mesh->vertices = (rf_preview_vertex *)malloc(mesh->bytes);
    if (!mesh->vertices) { rf_preview_close(mesh); return RF_RANGE; }
    result = generate(mesh, g, level, count,origin,matrix,material_base);
    if (result != RF_OK) rf_preview_close(mesh);
    return result;
}
int rf_preview_build(rf_preview_mesh *mesh,const rf_geometry *g,const rf_level *level,uint32_t budget)
{
    return build(mesh,g,level,NULL,NULL,0,budget);
}
int rf_preview_build_transformed(rf_preview_mesh *mesh,const rf_geometry *g,const rf_level *level,
    const float origin[3],const float matrix[3][3],uint32_t material_base,uint32_t budget)
{
    rf_collision_ray_hit local={0},world;int status;
    if(!origin || !matrix)return RF_RANGE;
    status=rf_collision_contact_world(&local,origin,matrix,&world);if(status)return status;
    return build(mesh,g,level,origin,matrix,material_base,budget);
}
void rf_preview_close(rf_preview_mesh *mesh)
{
    if (mesh) { free(mesh->vertices); memset(mesh, 0, sizeof(*mesh)); }
}
static int world_mesh(rf_preview_mesh *mesh,const rf_geometry *world,
    const rf_geometry_movers *movers,const rf_group_attached_pose *poses,
    const rf_geometry_materials *materials,const rf_level *level,uint32_t budget,int reuse,rf_preview_vertex *scratch)
{
    rf_preview_mesh next={0};uint32_t pass,i,j,total=0,capacity=budget/sizeof(rf_preview_vertex);int status;
    memset(rf_preview_failure,0,sizeof(rf_preview_failure));
    if(!mesh || (!reuse && (mesh->vertices || mesh->bytes)) ||
        (reuse && ((budget && !mesh->vertices) || mesh->bytes>budget ||
            (uint64_t)mesh->count*sizeof(rf_preview_vertex)!=mesh->bytes)) ||
        !world || !world->data || !movers ||
        (movers->count && !movers->items) || movers->count==UINT32_MAX || !materials ||
        materials->count!=movers->count+1 || !materials->offsets || materials->offsets[0] || !level)return RF_RANGE;
    for(i=0;i<materials->count;++i) {
        const rf_geometry *g=i?&movers->items[i-1].geometry:world;
        if(!g->data || materials->offsets[i+1]<materials->offsets[i] ||
            materials->offsets[i+1]-materials->offsets[i]!=g->textures ||
            (g->textures && !materials->slots))return RF_RANGE;
        for(j=materials->offsets[i];j<materials->offsets[i+1];++j)
            if(materials->slots[j]>=materials->textures.count)return RF_RANGE;
    }
    next.vertices=scratch;
    for(pass=scratch?1:0;pass<2;++pass) {
        uint32_t at=0;
        for(i=0;i<materials->count;++i) {
            const rf_geometry *g=i?&movers->items[i-1].geometry:world;
            const float *origin=i?(poses?poses[i-1].position:movers->items[i-1].position):NULL;
            const float (*matrix)[3]=i?(poses?(const float (*)[3])poses[i-1].output_matrix:(const float (*)[3])movers->items[i-1].orientation):NULL;
            rf_preview_mesh part={0};
            if(origin) {
                rf_collision_ray_hit local={0},hit;
                status=rf_collision_contact_world(&local,origin,matrix,&hit);if(status)goto fail;
            }
            if(pass && next.vertices)part.vertices=next.vertices+at;
            status=generate(&part,g,level,capacity-at,origin,matrix,0);if(status)goto fail;
            if(pass)for(j=0;j<part.count;++j) {
                if(part.vertices[j].material>=g->textures){status=RF_FORMAT;goto fail;}
                part.vertices[j].material=materials->slots[materials->offsets[i]+part.vertices[j].material];
            }
            at+=part.count;
        }
        if(!pass) {
            total=at;capacity=total;next.count=total;next.bytes=total*sizeof(rf_preview_vertex);
            if(reuse)next.vertices=mesh->vertices;
            else if(next.bytes) {next.vertices=malloc(next.bytes);if(!next.vertices){status=RF_RANGE;goto fail;}}
        } else if(scratch){next.count=at;next.bytes=at*sizeof(rf_preview_vertex);}
        else if(at!=total){status=RF_FORMAT;goto fail;}
    }
    if(scratch) {if(next.bytes)memcpy(mesh->vertices,scratch,next.bytes);next.vertices=mesh->vertices;}
    *mesh=next;return RF_OK;
fail:
    if(!reuse)rf_preview_close(&next);return status;
}
int rf_preview_build_world(rf_preview_mesh *mesh,const rf_geometry *world,
    const rf_geometry_movers *movers,const rf_group_attached_pose *poses,
    const rf_geometry_materials *materials,const rf_level *level,uint32_t budget)
{
    return world_mesh(mesh,world,movers,poses,materials,level,budget,0,NULL);
}
int rf_preview_update_world(rf_preview_mesh *mesh,uint32_t capacity_bytes,
    const rf_geometry *world,const rf_geometry_movers *movers,
    const rf_group_attached_pose *poses,const rf_geometry_materials *materials,
    const rf_level *level)
{
    return world_mesh(mesh,world,movers,poses,materials,level,capacity_bytes,1,NULL);
}
int rf_preview_update_world_staged(rf_preview_mesh *mesh,uint32_t capacity_bytes,
    rf_preview_vertex *scratch,uint32_t scratch_bytes,const rf_geometry *world,
    const rf_geometry_movers *movers,const rf_group_attached_pose *poses,
    const rf_geometry_materials *materials,const rf_level *level)
{
    uintptr_t source=(uintptr_t)scratch,destination;
    if(!mesh || !scratch || scratch_bytes<capacity_bytes)return RF_RANGE;
    destination=(uintptr_t)mesh->vertices;
    if(source>UINTPTR_MAX-scratch_bytes || destination>UINTPTR_MAX-capacity_bytes ||
       (source<destination+capacity_bytes && destination<source+scratch_bytes))return RF_RANGE;
    return world_mesh(mesh,world,movers,poses,materials,level,capacity_bytes,1,scratch);
}

static int preview_model_emit(const rf_model_geometry *geometry,uint32_t batch,
    rf_model_render_buffers *buffers,uint16_t *indices,rf_model_clip_pool *pool,
    const rf_model_projection *view,const rf_model_clip_planes *planes,
    const rf_model_clip_projection *projection,const rf_model_render_output *attributes,
    rf_preview_mesh *mesh,uint32_t capacity_bytes,uint32_t *emitted,
    const float (*face_planes)[4],const uint8_t (*colors)[3])
{
    const rf_model_draw_batch *draw;rf_model_triangle_output output;uint32_t n,start;int status;
    if(!geometry || !geometry->batches || batch>=geometry->batch_count || !buffers ||
       !indices || !pool || !view || !planes || !projection || !attributes || !emitted)return RF_RANGE;
    if(mesh && (!mesh->vertices || mesh->bytes!=(uint64_t)mesh->count*sizeof(rf_preview_vertex) ||
        mesh->bytes>capacity_bytes))return RF_RANGE;
    draw=geometry->batches+batch;
    if(draw->vertices>4096)return RF_RANGE;
    output.vertices=buffers->vertices;output.indices=indices;output.vertex_count=draw->vertices;
    output.vertex_capacity=4096;output.index_count=0;output.index_capacity=24576;
    if(planes->near_depth>0) {
        status=rf_model_geometry_clip_near(geometry,batch,buffers,planes->near_depth);if(status)return status;
    }
    if(face_planes)
        status=rf_model_geometry_emit_static_batch(geometry,batch,buffers,view,planes,projection,attributes,0,pool,&output,face_planes,colors);
    else
        status=rf_model_geometry_emit_batch(geometry,batch,buffers,view,planes,projection,attributes,0,pool,&output);
    if(status)return status;
    if(output.index_count%3)return RF_FORMAT;
    for(n=0;n<output.index_count;++n)if(indices[n]>=output.vertex_count)return RF_FORMAT;
    if(mesh) {
        if(output.index_count>capacity_bytes/sizeof(rf_preview_vertex)-mesh->count)return RF_RANGE;
        start=mesh->count;
        for(n=0;n<output.index_count;++n) {
            const uint8_t *v=buffers->vertices[indices[n]];float xy[2],q,uv[2];
            rf_preview_vertex *p=mesh->vertices+start+n;
            memcpy(xy,v,8);memcpy(&q,v+12,4);memcpy(uv,v+24,8);
            if(!isfinite(xy[0]) || !isfinite(xy[1]) || !isfinite(q) || q<=0)return RF_FORMAT;
            p->position[0]=floorf(xy[0]*16)/16;p->position[1]=floorf(xy[1]*16)/16;
            p->position[2]=(1000.0f/999.9f)*(1-.1f*q)*16777215;
            p->color[0]=p->color[1]=p->color[2]=1;
            p->texture[0]=uv[0]*q;p->texture[1]=uv[1]*q;p->texture[2]=q;p->material=draw->material;
            p->lightmap_texture[0]=p->lightmap_texture[1]=0;p->lightmap_texture[2]=q;p->lightmap=UINT32_MAX;
        }
        mesh->count+=output.index_count;mesh->bytes=mesh->count*sizeof(rf_preview_vertex);
    }
    *emitted=output.index_count;return RF_OK;
}

int rf_preview_model_emit(const rf_model_geometry *geometry,uint32_t batch,
    rf_model_render_buffers *buffers,uint16_t *indices,rf_model_clip_pool *pool,
    const rf_model_projection *view,const rf_model_clip_planes *planes,
    const rf_model_clip_projection *projection,const rf_model_render_output *attributes,
    rf_preview_mesh *mesh,uint32_t capacity_bytes,uint32_t *emitted)
{
    return preview_model_emit(geometry,batch,buffers,indices,pool,view,planes,projection,
        attributes,mesh,capacity_bytes,emitted,NULL,NULL);
}

int rf_preview_static_model_emit(const rf_model_geometry *geometry,uint32_t batch,
    rf_model_render_buffers *buffers,uint16_t *indices,rf_model_clip_pool *pool,
    const rf_model_projection *view,const rf_model_clip_planes *planes,
    const rf_model_clip_projection *projection,const rf_model_render_output *attributes,
    rf_preview_mesh *mesh,uint32_t capacity_bytes,uint32_t *emitted,
    const float (*face_planes)[4],const uint8_t (*colors)[3])
{
    if(!face_planes)return RF_RANGE;
    return preview_model_emit(geometry,batch,buffers,indices,pool,view,planes,projection,
        attributes,mesh,capacity_bytes,emitted,face_planes,colors);
}
