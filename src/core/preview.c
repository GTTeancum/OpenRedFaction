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
static point camera(const rf_geometry *g, const rf_level *level, const rf_geometry_corner *corner)
{
    float p[3], v[3];
    unsigned i;
    point result = {0, 0, 0, 0, 0, 0, 0};
    result.u = corner->uv[0]; result.v = corner->uv[1];
    result.lu = corner->lightmap_uv[0]; result.lv = corner->lightmap_uv[1];
    rf_geometry_vertex(g, corner->vertex, p);
    for (i = 0; i < 3; ++i) v[i] = p[i] - level->player_position[i];
    for (i = 0; i < 3; ++i) {
        result.x += v[i] * level->player_orientation[0][i];
        result.y += v[i] * level->player_orientation[1][i];
        result.z += v[i] * level->player_orientation[2][i];
    }
    return result;
}
static int generate(rf_preview_mesh *mesh, const rf_geometry *g, const rf_level *level, uint32_t capacity)
{
    uint32_t f, used = 0;
    for (f = 0; f < g->faces; ++f) {
        rf_geometry_face face;
        rf_geometry_corner a, b, c;
        uint32_t corner, lightmap = UINT32_MAX;
        float color;
        rf_geometry_get_face(g, f, &face);
        if (face.portal || (face.flags & 1) || face.texture == UINT32_MAX) continue;
        if (face.lightmap_mapping != UINT32_MAX && rf_geometry_lightmap(g, face.lightmap_mapping, UINT32_MAX, &lightmap)) return RF_FORMAT;
        color = 0.25f + 0.6f * fabsf(face.plane[0] * 0.3f + face.plane[1] * 0.8f + face.plane[2] * 0.5f);
        if (color > 1) color = 1;
        rf_geometry_get_corner(g, f, 0, &a);
        for (corner = 1; corner + 1 < face.corners; ++corner) {
            point buffers[2][12];
            unsigned count = 3, plane, current = 0, i, j;
            rf_geometry_get_corner(g, f, corner, &b); rf_geometry_get_corner(g, f, corner + 1, &c);
            buffers[0][0] = camera(g, level, &a);
            buffers[0][1] = camera(g, level, &b);
            buffers[0][2] = camera(g, level, &c);
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
                        out->material = face.texture;
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
int rf_preview_build(rf_preview_mesh *mesh, const rf_geometry *g, const rf_level *level, uint32_t budget)
{
    int result;
    uint32_t count;
    if (!mesh || !g || !g->data || !level) return RF_RANGE;
    memset(mesh, 0, sizeof(*mesh));
    result = generate(mesh, g, level, budget / sizeof(rf_preview_vertex));
    if (result != RF_OK || !mesh->count) return result;
    count = mesh->count;
    mesh->bytes = count * sizeof(rf_preview_vertex);
    mesh->vertices = (rf_preview_vertex *)malloc(mesh->bytes);
    if (!mesh->vertices) { rf_preview_close(mesh); return RF_RANGE; }
    result = generate(mesh, g, level, count);
    if (result != RF_OK) rf_preview_close(mesh);
    return result;
}
void rf_preview_close(rf_preview_mesh *mesh)
{
    if (mesh) { free(mesh->vertices); memset(mesh, 0, sizeof(*mesh)); }
}
