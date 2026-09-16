#include "rf/geomod_publication.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "FAIL line%d: %s\n", __LINE__, #x);                                              \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
static rf_geomod_publication_work work;
static rf_geomod_vertex ov[4096], source_v[24], window_v[24], floor_v[24], cut_v[2][36];
static rf_geomod_face of[768], source_f[6], window_f[6], floor_f[6], cut_f[2][12];
static rf_geomod_publication_origin origins[768];
static const unsigned corner[6][4] = {{0, 4, 6, 2}, {1, 3, 7, 5}, {0, 1, 5, 4},
                                      {2, 6, 7, 3}, {0, 2, 3, 1}, {4, 5, 7, 6}};
static void box(float lo[3], float hi[3], rf_geomod_vertex *v, rf_geomod_face *f) {
    uint32_t i, k, j;
    for (i = 0; i < 6; i++) {
        f[i] = (rf_geomod_face){i * 4, 4, 3, 550 + i};
        for (j = 0; j < 4; j++) {
            unsigned c = corner[i][j];
            for (k = 0; k < 3; k++)
                v[i * 4 + j].position[k] = (c & (1u << k)) ? hi[k] : lo[k];
            v[i * 4 + j].uv[0] = v[i * 4 + j].position[0] * .25f + v[i * 4 + j].position[1] * .5f;
            v[i * 4 + j].uv[1] = v[i * 4 + j].position[2] * .75f;
        }
    }
}
static void make_cut(uint32_t index, float x) {
    rf_geomod_vertex v[24];
    rf_geomod_face f[6];
    float lo[3] = {-.15f, -.6f, -.65f}, hi[3] = {.15f, .6f, .65f};
    uint32_t i, j, k;
    box(lo, hi, v, f);
    for (i = 0; i < 12; i++) {
        unsigned a = i / 2, b = i % 2;
        unsigned indices[3] = {0, b ? 2 : 1, b ? 3 : 2};
        cut_f[index][i] = (rf_geomod_face){i * 3, 3, 9, UINT32_MAX};
        for (j = 0; j < 3; j++) {
            rf_geomod_vertex p = v[a * 4 + indices[j]];
            float oldx = p.position[0], oldz = p.position[2];
            p.position[0] = oldx * .8f + oldz * .6f + x;
            p.position[2] = -oldx * .6f + oldz * .8f;
            p.position[1] -= .9f;
            cut_v[index][i * 3 + j] = p;
            for (k = 0; k < 2; k++)
                cut_v[index][i * 3 + j].uv[k] = 0;
        }
    }
}
static double area(const rf_geomod_mesh_view *m) {
    double sum = 0;
    uint32_t i, k;
    for (i = 0; i < m->face_count; i++)
        if (origins[i].kind == RF_GEOMOD_PUBLICATION_NEIGHBOR) {
            const rf_geomod_face *f = m->faces + i;
            double a = 0;
            for (k = 0; k < f->count; k++) {
                const float *p = m->vertices[f->first + k].position,
                            *q = m->vertices[f->first + (k + 1) % f->count].position;
                a += (double)p[0] * q[2] - (double)p[2] * q[0];
            }
            sum += fabs(a) * .5;
        }
    return sum;
}
int main(void) {
    float lo[3] = {-.25f, -1.5f, -.25f}, hi[3] = {.25f, 2, .25f}, fl[3] = {-10, -2, -10},
          fh[3] = {10, -1.25f, 10};
    float source_planes[6][4] = {{-1, 0, 0, -.25f}, {1, 0, 0, -.25f},  {0, -1, 0, -1.5f},
                                 {0, 1, 0, -2},     {0, 0, -1, -.25f}, {0, 0, 1, -.25f}};
    float floor_planes[6][4] = {{-1, 0, 0, -10},  {1, 0, 0, -10},  {0, -1, 0, -2},
                                {0, 1, 0, 1.25f}, {0, 0, -1, -10}, {0, 0, 1, -10}};
    rf_geomod_terrain *terrain = NULL;
    rf_geomod_terrain_view view;
    rf_geomod_mesh_view source, out, sentinel;
    rf_collision_face_filter filters[6], generated = {4, 256, -1, 1, 0, 0};
    rf_geomod_publication_job job = {0};
    rf_geomod_publication_solid solid = {floor_planes, 6, 71};
    rf_geomod_publication_origin win_origin[4], floor_origin = {2, 71, 415, 171};
    rf_geomod_publication_cut cuts[2];
    uint32_t i, j, n = 0, star = 77;
    float kernel[3] = {8, 9, 10};
    double first_area;
    int status;
    box(lo, hi, source_v, source_f);
    source_f[0].source_face = 552;
    source_f[1].source_face = 553;
    source_f[2].source_face = 548;
    source_f[3].source_face = 549;
    source_f[4].source_face = 550;
    source_f[5].source_face = 551;
    for (i = 0; i < 6; i++) {
        filters[i] = generated;
        if (i == 2 || i == 3)
            continue;
        window_f[n] = source_f[i];
        window_f[n].first = n * 4;
        win_origin[n] = (rf_geomod_publication_origin){0, 94, source_f[i].source_face, 149 + n};
        for (j = 0; j < 4; j++) {
            window_v[n * 4 + j] = source_v[i * 4 + j];
            if (window_v[n * 4 + j].position[1] < -1.25f) {
                window_v[n * 4 + j].position[1] = -1.25f;
                window_v[n * 4 + j].uv[0] = window_v[n * 4 + j].position[0] * .25f - .625f;
            }
        }
        n++;
    }
    box(fl, fh, floor_v, floor_f);
    floor_f[3].source_face = 415;
    floor_f[3].material = 10;
    source = (rf_geomod_mesh_view){source_v, source_f, 24, 6, 0};
    CHECK(!rf_geomod_terrain_open(&source, filters, &generated, 0, 4096, 768, 1048576, &terrain));
    memset(&sentinel, 0x5a, sizeof(sentinel));
    out = sentinel;
    CHECK(rf_geomod_terrain_cutter_get(terrain, 0, &out, kernel, &star) == RF_RANGE);
    CHECK(!memcmp(&out, &sentinel, sizeof(out)) && kernel[0] == 8 && star == 77);
    make_cut(0, 0);
    cuts[0].mesh = (rf_geomod_mesh_view){cut_v[0], cut_f[0], 36, 12, 0};
    cuts[0].kernel[0] = 0;
    cuts[0].kernel[1] = -.9f;
    cuts[0].kernel[2] = 0;
    cuts[0].star = 1;
    CHECK(!rf_geomod_terrain_cut_star(terrain, &cuts[0].mesh, cuts[0].kernel));
    CHECK(!rf_geomod_terrain_cutter_get(terrain, 0, &cuts[0].mesh, cuts[0].kernel, &cuts[0].star));
    CHECK(!rf_geomod_terrain_get(terrain, &view));
    job.terrain = view.mesh;
    job.windows = (rf_geomod_mesh_view){window_v, window_f, 16, 4, 0};
    job.neighbors = (rf_geomod_mesh_view){floor_v, floor_f + 3, 24, 1, 0};
    job.window_origins = win_origin;
    job.neighbor_origins = &floor_origin;
    job.crater_origin = (rf_geomod_publication_origin){1, 94, UINT32_MAX, 149};
    job.source_planes = source_planes;
    job.source_plane_count = 6;
    job.solids = &solid;
    job.solid_count = 1;
    job.cuts = cuts;
    job.cut_count = 1;
    CHECK(!rf_geomod_publication_build(&job, &work, ov, 4096, of, 768, origins, &out));
    first_area = area(&out);
    CHECK(fabs(first_area - 1.0 / 6.0) < 1e-6);
    for (i = 0; i < out.face_count; i++) {
        CHECK(of[i].source_face != 548 && of[i].source_face != 549);
        if (origins[i].kind == 2)
            CHECK(of[i].material == 10 && of[i].source_face == 415 && origins[i].reference == 171);
        for (j = 0; j < of[i].count; j++)
            CHECK(ov[of[i].first + j].position[1] >= -1.25001f);
    }
    for (i = 0; i < out.face_count; i++)
        if (origins[i].kind != RF_GEOMOD_PUBLICATION_CRATER) {
            for (j = 0; j < of[i].count; j++) {
                const rf_geomod_vertex *v = ov + of[i].first + j;
                CHECK(fabsf(v->uv[0] - (v->position[0] * .25f + v->position[1] * .5f)) < 1e-6f);
                CHECK(fabsf(v->uv[1] - v->position[2] * .75f) < 1e-6f);
            }
        }
    printf("single faces%u vertices%u revealed%.9g work%zu\n", out.face_count, out.vertex_count, first_area,
           sizeof(work));
    cuts[1] = cuts[0];
    job.cut_count = 2;
    CHECK(!rf_geomod_publication_build(&job, &work, ov, 4096, of, 768, origins, &out));
    CHECK(fabs(area(&out) - first_area) < 1e-6);
    printf("duplicate revealed%.9g\n", area(&out));
    /* Capacity/nonstar errors cannot partially publish. */
    memset(ov, 0x5a, sizeof(ov));
    memset(of, 0x5a, sizeof(of));
    memset(origins, 0x5a, sizeof(origins));
    out = sentinel;
    CHECK(rf_geomod_publication_build(&job, &work, ov, 1, of, 1, origins, &out) == RF_RANGE);
    CHECK(!memcmp(&out, &sentinel, sizeof(out)));
    for (i = 0; i < sizeof(ov); i++)
        CHECK(((unsigned char *)ov)[i] == 0x5a);
    for (i = 0; i < sizeof(of); i++)
        CHECK(((unsigned char *)of)[i] == 0x5a);
    for (i = 0; i < sizeof(origins); i++)
        CHECK(((unsigned char *)origins)[i] == 0x5a);
    cuts[1].star = 0;
    CHECK(rf_geomod_publication_build(&job, &work, ov, 4096, of, 768, origins, &out) == RF_NOT_FOUND);
    CHECK(!memcmp(&out, &sentinel, sizeof(out)));
    /* A distinct translated/rotated committed cutter retains its actual geometry. */
    make_cut(1, .3f);
    cuts[1].mesh = (rf_geomod_mesh_view){cut_v[1], cut_f[1], 36, 12, 0};
    cuts[1].kernel[0] = .3f;
    cuts[1].kernel[1] = -.9f;
    cuts[1].kernel[2] = 0;
    cuts[1].star = 1;
    CHECK(!rf_geomod_terrain_cut_star(terrain, &cuts[1].mesh, cuts[1].kernel));
    for (i = 0; i < 2; i++)
        CHECK(!rf_geomod_terrain_cutter_get(terrain, i, &cuts[i].mesh, cuts[i].kernel, &cuts[i].star));
    CHECK(cuts[1].kernel[0] == .3f);
    CHECK(!rf_geomod_terrain_get(terrain, &view));
    job.terrain = view.mesh;
    CHECK(!rf_geomod_publication_build(&job, &work, ov, 4096, of, 768, origins, &out));
    CHECK(fabs(area(&out) - 5.0 / 24.0) < 1e-6);
    printf("two transformed faces%u revealed%.9g\n", out.face_count, area(&out));
    status = rf_geomod_terrain_reset(terrain);
    CHECK(!status);
    out = sentinel;
    CHECK(rf_geomod_terrain_cutter_get(terrain, 0, &out, kernel, &star) == RF_RANGE);
    CHECK(!memcmp(&out, &sentinel, sizeof(out)));
    /* Mixed committed history is explicitly unsupported, with no forged kernel. */
    make_cut(0, 0);
    cuts[0].mesh = (rf_geomod_mesh_view){cut_v[0], cut_f[0], 36, 12, 0};
    cuts[0].kernel[0] = 0;
    cuts[0].kernel[1] = -.9f;
    cuts[0].kernel[2] = 0;
    CHECK(!rf_geomod_terrain_cut_star(terrain, &cuts[0].mesh, cuts[0].kernel));
    {
        float box_center[3] = {0, 1, 0}, half[3] = {.1f, .1f, .1f};
        CHECK(!rf_geomod_terrain_cut_box(terrain, box_center, half, 9));
    }
    for (i = 0; i < 2; i++)
        CHECK(!rf_geomod_terrain_cutter_get(terrain, i, &cuts[i].mesh, cuts[i].kernel, &cuts[i].star));
    CHECK(cuts[0].star == 1 && cuts[1].star == 0);
    CHECK(cuts[1].kernel[0] == 0 && cuts[1].kernel[1] == 0 && cuts[1].kernel[2] == 0);
    CHECK(!rf_geomod_terrain_get(terrain, &view));
    job.terrain = view.mesh;
    out = sentinel;
    CHECK(rf_geomod_publication_build(&job, &work, ov, 4096, of, 768, origins, &out) == RF_NOT_FOUND);
    CHECK(!memcmp(&out, &sentinel, sizeof(out)));
    /* Failed mutation and out-of-range snapshot leave committed data intact. */
    {
        rf_geomod_mesh_view before = cuts[0].mesh, after;
        rf_geomod_vertex vertex = before.vertices[0];
        float bad[3] = {0, 0, 0};
        CHECK(rf_geomod_terrain_cut_box(terrain, bad, bad, 9) != RF_OK);
        CHECK(!rf_geomod_terrain_cutter_get(terrain, 0, &after, kernel, &star));
        CHECK(after.vertices == before.vertices && !memcmp(after.vertices, &vertex, sizeof(vertex)));
        out = sentinel;
        kernel[0] = 8;
        star = 77;
        CHECK(rf_geomod_terrain_cutter_get(terrain, 2, &out, kernel, &star) == RF_RANGE);
        CHECK(!memcmp(&out, &sentinel, sizeof(out)) && kernel[0] == 8 && star == 77);
    }
    rf_geomod_terrain_close(&terrain);
    puts("PASS publication");
    return 0;
}
