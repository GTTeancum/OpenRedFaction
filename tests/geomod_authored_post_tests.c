#include "rf/geomod_authored_post.h"
#include <math.h>
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
static uint32_t word(const unsigned char *p) {
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put_word(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
/* Locate semantic tokens in the valid installed editor stream; do not depend
 * on a hard-coded section offset or accidentally modify a corner/texture. */
static int source_offsets(const unsigned char *p, uint32_t bytes, uint32_t offsets[6]) {
    uint32_t at = 4, count, i, j, n, uid, found = 0;
    CHECK(bytes >= 4); count = word(p);
    for (i = 0; i < count; i++) {
        CHECK(at <= bytes && bytes - at >= 62);
        uid = word(p + at); at += 58; n = word(p + at); at += 4;
        for (j = 0; j < n; j++) {
            uint32_t length;
            CHECK(bytes - at >= 2); length = p[at] | (uint32_t)p[at + 1] << 8; at += 2;
            CHECK(length <= bytes - at); at += length;
        }
        CHECK(bytes - at >= 20); at += 16; n = word(p + at); at += 4;
        CHECK(n <= (bytes - at) / 12); at += n * 12;
        CHECK(bytes - at >= 4); n = word(p + at); at += 4;
        for (j = 0; j < n; j++) {
            uint32_t token, corners, stride;
            CHECK(bytes - at >= 56);
            token = word(p + at + 24); corners = word(p + at + 52);
            stride = word(p + at + 20) == UINT32_MAX ? 12 : 20;
            if (uid == 94) {
                CHECK(token >= 548 && token <= 553);
                CHECK(!(found & (1u << (token - 548))));
                offsets[token - 548] = at; found |= 1u << (token - 548);
            }
            at += 56; CHECK(corners <= (bytes - at) / stride); at += corners * stride;
        }
        CHECK(bytes - at >= 20); at += 20;
    }
    CHECK(at == bytes && found == 63); return 0;
}
static int eligibility_cases(unsigned char *payload, uint32_t bytes, rf_geometry *geometry,
                             const rf_level_geomod_settings *settings,
                             const rf_geomod_authored_post *published) {
    uint32_t offsets[6], i, j, rejected = 0;
    rf_geomod_authored_post *candidate = NULL;
    rf_geomod_authored_post_view before, after;
    rf_geomod_vertex vertices[24];
    CHECK(!source_offsets(payload, bytes, offsets));
    CHECK(!rf_geomod_authored_post_get(published, &before));
    CHECK(before.source.vertex_count == 24);
    memcpy(vertices, before.source.vertices, sizeof(vertices));
    /* All six faces includes both unpublished caps, not just visible windows. */
    for (i = 0; i < 10; i++) {
        unsigned char *face = i < 6 ? payload + offsets[i] : geometry->data + geometry->face_offsets[149 + i - 6];
        uint32_t flags = word(face + 40), portal = word(face + 36);
        for (j = 0; j < 3; j++) {
            put_word(face + 40, flags | (j == 0 ? 4u : j == 1 ? 8u : 0u));
            put_word(face + 36, j == 2 ? (portal & 0xffff0000u) | 1u : portal);
            CHECK(rf_geomod_authored_post_decode(payload, bytes, geometry, settings, 2 * 1024 * 1024,
                                                 &candidate) == RF_NOT_FOUND && !candidate);
            rejected++;
            put_word(face + 40, flags); put_word(face + 36, portal);
        }
        /* 0x100 is not permission; signed portal zero is still ordinary. */
        put_word(face + 40, flags & ~256u); put_word(face + 36, portal & 0xffff0000u);
        CHECK(!rf_geomod_authored_post_decode(payload, bytes, geometry, settings, 2 * 1024 * 1024,
                                              &candidate));
        rf_geomod_authored_post_close(&candidate);
        put_word(face + 40, flags); put_word(face + 36, portal);
    }
    {
        unsigned char *detail = geometry->data + geometry->room_offsets[3] + 34;
        unsigned char original = *detail;
        for (j = 1; j <= 2; j++) {
            *detail = (unsigned char)j;
            CHECK(rf_geomod_authored_post_decode(payload, bytes, geometry, settings, 2 * 1024 * 1024,
                                                 &candidate) == RF_NOT_FOUND && !candidate);
            rejected++;
        }
        *detail = original;
    }
    CHECK(!rf_geomod_authored_post_get(published, &after));
    CHECK(after.source.vertices == before.source.vertices && after.source.faces == before.source.faces);
    CHECK(!memcmp(after.source.vertices, vertices, sizeof(vertices)));
    CHECK(!rf_geomod_authored_post_decode(payload, bytes, geometry, settings, 2 * 1024 * 1024,
                                          &candidate));
    rf_geomod_authored_post_close(&candidate);
    printf("ELIGIBILITY %u rejections;10 non-permission variants;published owner preserved\n", rejected);
    return 0;
}
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
    center[0] = a.source.vertices[0].position[0];
    center[2] = 0;
    for (i = 0; i < a.source.vertex_count; i++) {
        if (a.source.vertices[i].position[0] > center[0])
            center[0] = a.source.vertices[i].position[0];
        center[2] += a.source.vertices[i].position[2] / a.source.vertex_count;
    }
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
    CHECK(output.face_count > a.windows.face_count);
    if (a.source_uid == 94)
        CHECK(output.face_count == 35 && output.vertex_count == 150);
    for (i = 0; i < output.face_count; i++) {
        CHECK(output_origins[i].reference != UINT32_MAX);
        uint32_t cap;
        for (cap = 0; cap < a.source.face_count; cap++)
            if (a.source_planes[cap][1] < -.99f || a.source_planes[cap][1] > .99f)
                CHECK(output_faces[i].source_face != a.source.faces[cap].source_face);
        if (output_origins[i].kind == 2) {
            CHECK(output_origins[i].owner == 71 && output_faces[i].source_face == 415 &&
                  output_faces[i].material == 10 && output_origins[i].reference == 141);
            floors++;
        }
    }
    CHECK(floors > 0);
    if (a.source_uid == 94) CHECK(floors == 8);
    printf("DIRECT_PUBLICATION faces%u vertices%u floorpieces%u\n", output.face_count, output.vertex_count,
           floors);
    rf_geomod_terrain_close(&terrain);
    return 0;
}
static int beam_source(const rf_level *level,const rf_geometry *geometry,const char *shape_path) {
    rf_geomod_authored_post *owner=NULL,*rejected=NULL;rf_geomod_authored_post_view a;
    rf_geomod_template shape;rf_geomod_terrain *terrain=NULL;rf_geomod_terrain_view view;
    rf_geomod_publication_job job={0};rf_geomod_publication_cut cut;rf_geomod_mesh_view output;
    rf_collision_face_filter generated;float center[3]={-4.75f,2.25f,0},basis[9]={1,0,0,0,1,0,0,0,1};
    uint32_t i,k;double roof_area=0;
    CHECK(!rf_geomod_authored_post_open_source(level,geometry,95,2*1024*1024,&owner));
    CHECK(!rf_geomod_authored_post_get(owner,&a));
    CHECK(a.source_uid==95 && a.windows.face_count==8 && a.source.face_count==6 && a.solid_count==3);
    CHECK(a.neighbor_void_count==1 && a.neighbor_voids[0].owner==80 && a.neighbor_voids[0].count==5);
    for(i=0;i<a.neighbors.face_count;i++)if(a.neighbor_origins[i].source_face==478) {
        const rf_geomod_face *f=a.neighbors.faces+i;double area=0;
        for(k=0;k<f->count;k++) {
            const float *p=a.neighbors.vertices[f->first+k].position,*q=a.neighbors.vertices[f->first+(k+1)%f->count].position;
            CHECK(fabsf(p[2])>=3-1e-5f);area+=(double)p[0]*q[2]-(double)p[2]*q[0];
        }
        roof_area+=fabs(area)*.5;
    }
    CHECK(fabs(roof_area-8)<1e-5);
    CHECK(rf_geomod_authored_post_open_source(level,geometry,95,a.peak_bytes-1,&rejected)==RF_RANGE && !rejected);
    generated=a.source_filters[0];
    CHECK(!rf_geomod_terrain_open(&a.source,a.source_filters,&generated,0,4096,768,1048576,&terrain));
    CHECK(!rf_geomod_template_load(shape_path,&shape));
    CHECK(!rf_geomod_terrain_cut_template(terrain,&shape,center,basis,1.05000007f,0));
    CHECK(!rf_geomod_terrain_get(terrain,&view));
    CHECK(!rf_geomod_terrain_cutter_get(terrain,0,&cut.mesh,cut.kernel,&cut.star));
    job.terrain=view.mesh;job.windows=a.windows;job.neighbors=a.neighbors;
    job.window_origins=a.window_origins;job.neighbor_origins=a.neighbor_origins;
    job.source_planes=a.source_planes;job.source_plane_count=a.source.face_count;
    job.solids=a.solids;job.solid_count=a.solid_count;job.neighbor_voids=a.neighbor_voids;job.neighbor_void_count=a.neighbor_void_count;
    job.crater_origin=(rf_geomod_publication_origin){1,95,UINT32_MAX,a.replaced_ids[0]};job.cuts=&cut;job.cut_count=1;
    CHECK(!rf_geomod_publication_build(&job,&publication_work,output_vertices,4096,output_faces,768,output_origins,&output));
    CHECK(output.face_count>0);
    for(i=0;i<output.face_count;i++) {
        CHECK(output_origins[i].kind!=2 || output_origins[i].owner!=80);
        CHECK(output_origins[i].reference!=UINT32_MAX);
    }
    printf("BEAM_LOADER resident%u peak%u neighbor_faces%u roof_area%.9g center_cut_faces%u\n",a.resident_bytes,a.peak_bytes,a.neighbors.face_count,roof_area,output.face_count);
    rf_geomod_terrain_close(&terrain);rf_geomod_authored_post_close(&owner);return 0;
}
static int selected_sources(const rf_level *level, const rf_geometry *geometry, const char *shape) {
    static const uint32_t uids[] = {93, 94, 96, 97};
    uint32_t n, i, k;
    for (n = 0; n < 4; n++) {
        rf_geomod_authored_post *owner = NULL;
        rf_geomod_authored_post_view v;
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        CHECK(!rf_geomod_authored_post_open_source(level, geometry, uids[n], 2 * 1024 * 1024, &owner));
        CHECK(!rf_geomod_authored_post_get(owner, &v));
        CHECK(v.source_uid == uids[n] && v.room == 3 && v.source.face_count == 6);
        CHECK(v.windows.face_count == 4 && v.solid_count == 3);
        for (i = 0; i < v.source.vertex_count; i++)
            for (k = 0; k < 3; k++) {
                float x = v.source.vertices[i].position[k];
                if (x < lo[k]) lo[k] = x;
                if (x > hi[k]) hi[k] = x;
            }
        CHECK(lo[0] == (n < 2 ? -5.25f : 5.75f));
        CHECK(hi[0] == (n < 2 ? -4.75f : 6.25f));
        CHECK(lo[1] == -1.5f && hi[1] == 2);
        CHECK(lo[2] == (n % 2 ? 2.25f : -2.75f));
        CHECK(hi[2] == (n % 2 ? 2.75f : -2.25f));
        for (i = 0; i < v.source.face_count; i++) CHECK(v.source_origins[i].owner == uids[n]);
        for (i = 0; i < v.windows.face_count; i++) CHECK(v.window_origins[i].owner == uids[n]);
        CHECK(!exercise_publication(owner, shape));
        printf("SELECTED_SOURCE uid%u resident%u peak%u\n", uids[n], v.resident_bytes, v.peak_bytes);
        rf_geomod_authored_post_close(&owner);
    }
    {
        rf_geomod_authored_post *owner = NULL;
        CHECK(rf_geomod_authored_post_open_source(level, geometry, 79, 2 * 1024 * 1024, &owner) == RF_NOT_FOUND);
        CHECK(!owner);
    }
    return 0;
}
static int grouped_posts(const rf_level *level,const rf_geometry *geometry,const char *shape_path) {
    rf_geomod_authored_post *owners[2]={0};rf_geomod_authored_post_view assets[2];
    rf_geomod_terrain *terrain[2]={0};rf_geomod_terrain_view views[2];
    rf_geomod_publication_job jobs[2]={{0}};rf_geomod_publication_cut cuts[2];
    rf_geomod_template shape;rf_geomod_mesh_view output,before;
    rf_geomod_vertex first_vertices[150];rf_geomod_face first_faces[35];
    rf_geomod_publication_origin first_origins[35];
    static rf_geomod_vertex saved_vertices[4096];static rf_geomod_face saved_faces[768];
    static rf_geomod_publication_origin saved_origins[768];
    float basis[9]={1,0,0,0,1,0,0,0,1};uint32_t i,j;
    CHECK(!rf_geomod_template_load(shape_path,&shape));
    for(i=0;i<2;i++) {
        rf_collision_face_filter generated;
        CHECK(!rf_geomod_authored_post_open_source(level,geometry,93+i,2*1024*1024,owners+i));
        CHECK(!rf_geomod_authored_post_get(owners[i],assets+i));generated=assets[i].source_filters[0];
        CHECK(!rf_geomod_terrain_open(&assets[i].source,assets[i].source_filters,&generated,0,4096,768,1048576,terrain+i));
        CHECK(!rf_geomod_terrain_get(terrain[i],views+i));
        jobs[i].terrain=views[i].mesh;jobs[i].windows=assets[i].windows;jobs[i].neighbors=assets[i].neighbors;
        jobs[i].window_origins=assets[i].window_origins;jobs[i].neighbor_origins=assets[i].neighbor_origins;
        jobs[i].source_planes=assets[i].source_planes;jobs[i].source_plane_count=assets[i].source.face_count;
        jobs[i].solids=assets[i].solids;jobs[i].solid_count=assets[i].solid_count;
        jobs[i].crater_origin=(rf_geomod_publication_origin){1,93+i,UINT32_MAX,assets[i].replaced_ids[0]};
    }
    CHECK(!rf_geomod_publication_build_groups(jobs,2,0,&publication_work,output_vertices,4096,output_faces,768,output_origins,&output));
    CHECK(output.face_count==8 && output.vertex_count==32);
    for(i=0;i<2;i++) {
        float center[3]={-4.75f,-.9f,i?2.5f:-2.5f};
        CHECK(!rf_geomod_terrain_cut_template(terrain[i],&shape,center,basis,1.05000007f,0));
        CHECK(!rf_geomod_terrain_get(terrain[i],views+i));
        CHECK(!rf_geomod_terrain_cutter_get(terrain[i],0,&cuts[i].mesh,cuts[i].kernel,&cuts[i].star));
        jobs[i].terrain=views[i].mesh;jobs[i].cuts=cuts+i;jobs[i].cut_count=1;
        CHECK(!rf_geomod_publication_build_groups(jobs,2,i+1,&publication_work,output_vertices,4096,output_faces,768,output_origins,&output));
        CHECK(output.generation==i+1 && output.face_count==(i?70:39) && output.vertex_count==(i?300:166));
        if(!i) {
            memcpy(first_vertices,output_vertices,sizeof(first_vertices));memcpy(first_faces,output_faces,sizeof(first_faces));
            memcpy(first_origins,output_origins,sizeof(first_origins));
            for(j=35;j<39;j++)CHECK(output_origins[j].owner==94 && output_origins[j].kind==0);
        } else {
            CHECK(!memcmp(first_vertices,output_vertices,sizeof(first_vertices)));
            CHECK(!memcmp(first_faces,output_faces,sizeof(first_faces)));
            CHECK(!memcmp(first_origins,output_origins,sizeof(first_origins)));
        }
    }
    before=output;memcpy(saved_vertices,output_vertices,sizeof(saved_vertices));
    memcpy(saved_faces,output_faces,sizeof(saved_faces));memcpy(saved_origins,output_origins,sizeof(saved_origins));
    CHECK(rf_geomod_publication_build_groups(jobs,2,3,&publication_work,output_vertices,4096,output_faces,1,output_origins,&output)==RF_RANGE);
    jobs[1].crater_origin.owner=93;
    CHECK(rf_geomod_publication_build_groups(jobs,2,3,&publication_work,output_vertices,4096,output_faces,768,output_origins,&output)==RF_FORMAT);
    jobs[1].crater_origin.owner=94;jobs[1].source_plane_count=0;
    CHECK(rf_geomod_publication_build_groups(jobs,2,3,&publication_work,output_vertices,4096,output_faces,768,output_origins,&output)!=RF_OK);
    CHECK(!memcmp(&before,&output,sizeof(output)) && !memcmp(saved_vertices,output_vertices,sizeof(saved_vertices)));
    CHECK(!memcmp(saved_faces,output_faces,sizeof(saved_faces)) && !memcmp(saved_origins,output_origins,sizeof(saved_origins)));
    for(i=0;i<2;i++){rf_geomod_terrain_close(terrain+i);rf_geomod_authored_post_close(owners+i);}
    puts("PASS grouped real posts93/94: intact8, first cut39, both cuts70 faces; first publication preserved; atomic rejection");
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
        CHECK(!eligibility_cases(payload, section->size, &geometry, &settings, owner));
        payload[4] = 94;
        payload[5] = payload[6] = payload[7] = 0;
        CHECK(rf_geomod_authored_post_decode(payload, section->size, &geometry, &settings, 2 * 1024 * 1024,
                                             &other) != RF_OK &&
              !other);
        free(payload);
    }
    CHECK(!beam_source(&level,&geometry,argc > 2 ? argv[2] : "build/data/geomod-template.bin"));
    CHECK(!selected_sources(&level, &geometry, argc > 2 ? argv[2] : "build/data/geomod-template.bin"));
    CHECK(!grouped_posts(&level,&geometry,argc > 2 ? argv[2] : "build/data/geomod-template.bin"));
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
