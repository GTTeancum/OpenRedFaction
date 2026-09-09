#include "rf/preview.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef struct point { float x, y, z, u, v, lu, lv; } point;
static float distance(point p, unsigned plane)
{
    switch (plane) {
    case 0: return p.z - 0.1f;
    case 1: return 1000.0f - p.z;
    case 2: return p.z + p.x;
    case 3: return p.z - p.x;
    case 4: return p.z * 0.75f + p.y;
    default: return p.z * 0.75f - p.y;
    }
}
static unsigned clip(const point *input, unsigned count, point *output, unsigned plane)
{
    unsigned i, used = 0;
    point previous = input[count - 1];
    float before = distance(previous, plane);
    for (i = 0; i < count; ++i) {
        point current = input[i];
        float after = distance(current, plane);
        if ((before >= 0) != (after >= 0)) {
            float t = before / (before - after);
            output[used++] = (point){previous.x + t * (current.x - previous.x), previous.y + t * (current.y - previous.y), previous.z + t * (current.z - previous.z), previous.u + t * (current.u - previous.u), previous.v + t * (current.v - previous.v), previous.lu + t * (current.lu - previous.lu), previous.lv + t * (current.lv - previous.lv)};
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
        uint32_t corner, lightmap = UINT32_MAX;
        float color;int status;
        rf_geometry_get_face(g, f, &face);
        if (face.portal || (face.flags & 1) || face.texture == UINT32_MAX) continue;
        if (face.lightmap_mapping != UINT32_MAX && rf_geometry_lightmap(g, face.lightmap_mapping, UINT32_MAX, &lightmap)) return RF_FORMAT;
        if(face.texture>=UINT32_MAX-material_base)return RF_RANGE;
        if(origin) {
            rf_collision_ray_hit local={0},world;memcpy(local.normal,face.plane,12);
            status=rf_collision_contact_world(&local,origin,matrix,&world);if(status)return status;memcpy(face.plane,world.normal,12);
        }
        color = 0.25f + 0.6f * fabsf(face.plane[0] * 0.3f + face.plane[1] * 0.8f + face.plane[2] * 0.5f);
        if (color > 1) color = 1;
        rf_geometry_get_corner(g, f, 0, &a);
        for (corner = 1; corner + 1 < face.corners; ++corner) {
            point buffers[2][12];
            unsigned count = 3, plane, current = 0, i, j;
            rf_geometry_get_corner(g, f, corner, &b); rf_geometry_get_corner(g, f, corner + 1, &c);
            if((status=camera(g,level,&a,origin,matrix,&buffers[0][0])) ||
               (status=camera(g,level,&b,origin,matrix,&buffers[0][1])) ||
               (status=camera(g,level,&c,origin,matrix,&buffers[0][2])))return status;
            for (plane = 0; plane < 6 && count; ++plane) {
                count = clip(buffers[current], count, buffers[1-current], plane);
                current = 1-current;
            }
            for (i = 1; i + 1 < count; ++i) {
                unsigned indices[3] = {0, i, i+1};
                if (used > capacity || capacity-used < 3) return RF_RANGE;
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
int rf_preview_build_world(rf_preview_mesh *mesh,const rf_geometry *world,
    const rf_geometry_movers *movers,const rf_group_attached_pose *poses,
    const rf_geometry_materials *materials,const rf_level *level,uint32_t budget)
{
    rf_preview_mesh next={0};uint32_t pass,i,j,total=0,capacity=budget/sizeof(rf_preview_vertex);int status;
    if(!mesh || mesh->vertices || mesh->bytes || !world || !world->data || !movers ||
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
    for(pass=0;pass<2;++pass) {
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
            if(next.bytes) {next.vertices=malloc(next.bytes);if(!next.vertices){status=RF_RANGE;goto fail;}}
        } else if(at!=total){status=RF_FORMAT;goto fail;}
    }
    *mesh=next;return RF_OK;
fail:
    rf_preview_close(&next);return status;
}
