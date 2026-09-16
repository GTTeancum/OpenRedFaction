#include "rf/geomod_authored_post.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "FAIL line%d %s\n", __LINE__, #x);                                               \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
static rf_geomod_publication_work publication_work;
static rf_geomod_vertex output_vertices[4096];
static rf_geomod_face output_faces[768];
static rf_geomod_publication_origin output_origins[768];
static int exercise_publication(const rf_geomod_authored_post *owner, const char *template_path) {
    rf_geomod_authored_post_view a;
    rf_geomod_terrain *terrain = NULL;
    rf_geomod_terrain_view t;
    rf_geomod_template shape;
    rf_geomod_publication_cut cutter;
    rf_geomod_publication_job job = {0};
    rf_geomod_mesh_view output;
    rf_collision_face_filter generated;
    float center[3] = {-4.75f, -.9f, 2.5f}, basis[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    uint32_t i, floors = 0;
    CHECK(!rf_geomod_authored_post_get(owner, &a));
    generated = a.source_filters[0];
    CHECK(!rf_geomod_terrain_open(&a.source, a.source_filters, &generated, 0, 4096, 768, 1048576, &terrain));
    CHECK(!rf_geomod_template_load(template_path, &shape));
    CHECK(!rf_geomod_terrain_cut_template(terrain, &shape, center, basis, 1.05000007f, 0));
    CHECK(!rf_geomod_terrain_get(terrain, &t));
    CHECK(!rf_geomod_terrain_cutter_get(terrain, 0, &cutter.mesh, cutter.kernel, &cutter.star));
    job.terrain = t.mesh;
    job.windows = a.windows;
    job.neighbors = a.neighbors;
    job.source_planes = a.source_planes;
    job.source_plane_count = a.source.face_count;
    job.solids = a.solids;
    job.solid_count = a.solid_count;
    job.window_origins = a.window_origins;
    job.neighbor_origins = a.neighbor_origins;
    job.crater_origin = (rf_geomod_publication_origin){1, a.source_uid, UINT32_MAX, a.replaced_ids[0]};
    job.cuts = &cutter;
    job.cut_count = 1;
    CHECK(!rf_geomod_publication_build(&job, &publication_work, output_vertices, 4096, output_faces, 768,
                                       output_origins, &output));
    CHECK(output.face_count == 35 && output.vertex_count == 150);
    for (i = 0; i < output.face_count; i++) {
        CHECK(output_origins[i].reference != UINT32_MAX);
        CHECK(output_faces[i].source_face != 548 && output_faces[i].source_face != 549);
        if (output_origins[i].kind == 2) {
            CHECK(output_origins[i].owner == 71 && output_faces[i].source_face == 415 &&
                  output_faces[i].material == 10 && output_origins[i].reference == 141);
            floors++;
        }
    }
    CHECK(floors == 8);
    printf("DIRECT_PUBLICATION faces%u vertices%u floorpieces%u\n", output.face_count, output.vertex_count,
           floors);
    rf_geomod_terrain_close(&terrain);
    return 0;
}
int main(int argc, char **argv) {
    rf_geomod_authored_post *owner = NULL, *other = NULL;
    rf_geomod_authored_post_view v;
    rf_vpp archive = {0};
    rf_level level;
    rf_geometry geometry = {0};
    rf_level_geomod_settings settings = {0};
    unsigned char invalid[4] = {0}, dummy[56] = {0};
    uint32_t i;
    int s;
    geometry.data = dummy;
    geometry.bytes = sizeof(dummy);
    CHECK(rf_geomod_authored_post_decode(invalid, 3, &geometry, &settings, 1024, &owner) == RF_FORMAT);
    CHECK(!owner);
    CHECK(rf_geomod_authored_post_decode(invalid, 4, &geometry, &settings, 1024, &owner) == RF_FORMAT);
    invalid[0] = 1;
    CHECK(rf_geomod_authored_post_decode(invalid, 4, &geometry, &settings, 1, &owner) == RF_RANGE);
    CHECK(!owner);
    memset(&geometry, 0, sizeof(geometry));
    if (argc < 2) {
        puts("PASS authored malformed boundaries; supply levelsm.vpp for installed integration");
        return 0;
    }
    CHECK(!rf_vpp_open(&archive, argv[1]));
    CHECK(!rf_level_open(&level, &archive, "ctf06.rfl"));
    CHECK(!rf_geometry_open(&geometry, &level, 8 * 1024 * 1024));
    s = rf_geomod_authored_post_open(&level, &geometry, 2 * 1024 * 1024, &owner);
    printf("LOADER status%d\n", s);
    CHECK(!s);
    CHECK(!rf_geomod_authored_post_get(owner, &v));
    CHECK(v.source_uid == 94 && v.room == 3 && v.source.face_count == 6 && v.source.vertex_count == 24 &&
          v.windows.face_count == 4 && v.replaced_count == 4 && v.solid_count == 3 && v.brush_count == 955 &&
          v.authored_face_count == 6168);
    CHECK(!strcmp(v.settings.texture, "rock02.tga") && v.settings.hardness == 100);
    for (i = 0; i < 4; i++)
        CHECK(v.replaced_ids[i] == 149 + i);
    for (i = 0; i < 6; i++) {
        CHECK(v.source.faces[i].source_face == 548 + i);
        CHECK(v.source.faces[i].material == 3);
    }
    for (i = 0; i < v.neighbors.face_count; i++)
        printf("NEIGHBOR owner%u source%u reference%u material%u\n", v.neighbor_origins[i].owner,
               v.neighbor_origins[i].source_face, v.neighbor_origins[i].reference,
               v.neighbors.faces[i].material);
    printf("OWNED source%u/%u windows%u/%u neighbors%u/%u resident%u peak%u\n", v.source.vertex_count,
           v.source.face_count, v.windows.vertex_count, v.windows.face_count, v.neighbors.vertex_count,
           v.neighbors.face_count, v.resident_bytes, v.peak_bytes);
    CHECK(rf_geomod_authored_post_open(&level, &geometry, v.peak_bytes - 1, &other) == RF_RANGE && !other);
    {
        const rf_level_section *section = rf_level_find(&level, 0x2000000);
        unsigned char *payload = malloc(section->size);
        CHECK(payload);
        CHECK(!rf_level_read(&level, section, 0, payload, section->size));
        CHECK(!rf_level_geomod_settings_read(&level, &settings));
        CHECK(rf_geomod_authored_post_decode(payload, section->size - 1, &geometry, &settings,
                                             2 * 1024 * 1024, &other) == RF_FORMAT &&
              !other);
        payload[4] = 94;
        payload[5] = payload[6] = payload[7] = 0;
        CHECK(rf_geomod_authored_post_decode(payload, section->size, &geometry, &settings, 2 * 1024 * 1024,
                                             &other) != RF_OK &&
              !other);
        free(payload);
    }
    rf_geometry_close(&geometry);
    rf_vpp_close(&archive);
    {
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        uint32_t k;
        for (i = 0; i < v.source.vertex_count; i++)
            for (k = 0; k < 3; k++) {
                float x = v.source.vertices[i].position[k];
                if (x < lo[k])
                    lo[k] = x;
                if (x > hi[k])
                    hi[k] = x;
            }
        CHECK(lo[0] == -5.25f && lo[1] == -1.5f && lo[2] == 2.25f && hi[0] == -4.75f && hi[1] == 2 &&
              hi[2] == 2.75f);
    }

    CHECK(v.windows.faces[0].source_face == 553);
    CHECK(v.source_filters[0].owner_present == 1);
    CHECK(!exercise_publication(owner, argc > 2 ? argv[2] : "build/data/geomod-template.bin"));
    rf_geomod_authored_post_close(&owner);
    CHECK(!owner);
    puts("PASS authored post asset loader");
    return 0;
}
