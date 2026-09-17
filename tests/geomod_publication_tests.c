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
/* Live UID94 rocket at Y0.3: clipped boundary contains a tiny float bend.
 * Its short edge violates convexity by about2.5e-5 world units. */
static int rounded_boundary(void) {
    static const float points[7][3]={{-5.25f,-.933783472f,2.75f},{-5.25f,-.901053548f,2.75f},
        {-5.25f,-.868632793f,2.32153749f},{-5.25f,-.963883638f,2.30476499f},
        {-5.25f,-1.24846113f,2.25465441f},{-5.25f,-1.25f,2.25438333f},{-5.25f,-1.25f,2.75f}};
    float planes[1][4]={{1,0,0,5.25f}},positions[64][3];
    rf_geomod_vertex v[7];rf_geomod_face f={0,7,3,UINT32_MAX};
    rf_geomod_mesh_view mesh={v,&f,7,1,0},out;
    rf_geomod_publication_job job={0};rf_collision_face bound[16];
    rf_collision_face_filter filters[16]={{0}};uint32_t i,j,k;double before=0,after=0;
    for(i=0;i<7;i++){memcpy(v[i].position,points[i],12);v[i].uv[0]=points[i][1];v[i].uv[1]=points[i][2];
        before+=(double)points[i][1]*points[(i+1)%7][2]-(double)points[i][2]*points[(i+1)%7][1];}
    CHECK(rf_geomod_collision_faces(&mesh,filters,positions,64,bound,16)==RF_FORMAT);
    job.terrain=mesh;job.source_planes=planes;job.source_plane_count=1;
    job.crater_origin=(rf_geomod_publication_origin){RF_GEOMOD_PUBLICATION_CRATER,94,UINT32_MAX,150};
    CHECK(!rf_geomod_publication_build(&job,&work,ov,4096,of,768,origins,&out));
    CHECK(out.face_count>1 && out.face_count<=16 && out.vertex_count<=64);
    CHECK(!rf_geomod_collision_faces(&out,filters,positions,64,bound,16));
    for(i=0;i<out.face_count;i++) {
        const rf_geomod_face *face=out.faces+i;
        CHECK(face->material==3 && face->source_face==UINT32_MAX);
        CHECK(origins[i].owner==94 && origins[i].reference==150 && origins[i].kind==RF_GEOMOD_PUBLICATION_CRATER);
        for(j=0;j<face->count;j++) {
            const rf_geomod_vertex *a=out.vertices+face->first+j,*b=out.vertices+face->first+(j+1)%face->count;
            CHECK(fabsf(a->uv[0]-a->position[1])<1e-6f && fabsf(a->uv[1]-a->position[2])<1e-6f);
            after+=(double)a->position[1]*b->position[2]-(double)a->position[2]*b->position[1];
        }
    }
    CHECK(fabs(before-after)<1e-9);
    for(i=0;i<7;i++){for(k=0;k<out.vertex_count;k++)if(!memcmp(points[i],out.vertices[k].position,12))break;CHECK(k<out.vertex_count);}
    puts("PASS publication rounded boundary: strict collision, preserved vertices/area/UV/provenance");return 0;
}
static int hollow_roof_boundary(void) {
    /* Actual roof80 bottom: the earlier air85 prism opens its center. */
    const float points[4][3]={{-4,2.5f,-4},{-4,2.5f,4},{-8,2.5f,4},{-8,2.5f,-4}};
    float planes[5][4]={{1,0,0,4},{-1,0,0,-8},{0,-1,0,2.5f},{0,6,1,-18},{0,6,-1,-18}};
    rf_geomod_vertex v[8],output[64],saved_v[64];rf_geomod_face f[2]={{0,4,10,478},{4,4,3,900}},faces[16],saved_f[16];
    rf_geomod_publication_origin input[2]={{2,80,478,164},{2,999,900,200}},out_origins[16],saved_o[16];
    rf_geomod_mesh_view mesh={v,f,8,2,17},out,before;
    rf_geomod_publication_solid air={planes,5,80};
    rf_collision_face bound[16];rf_collision_face_filter filters[16]={{0}};float positions[64][3];
    uint32_t i,j;double roof_area=0,other_area=0;
    for(i=3;i<5;i++)for(j=0;j<4;j++)planes[i][j]/=sqrtf(37);
    for(i=0;i<8;i++){memcpy(v[i].position,points[i%4],12);v[i].uv[0]=points[i%4][0];v[i].uv[1]=points[i%4][2];}
    memset(output,0xa5,sizeof(output));memset(faces,0xa5,sizeof(faces));memset(out_origins,0xa5,sizeof(out_origins));
    CHECK(!rf_geomod_publication_clip_neighbors(&mesh,input,&air,1,&work,output,64,faces,16,out_origins,&out));
    CHECK(out.generation==17 && out.face_count==3);
    CHECK(!rf_geomod_collision_faces(&out,filters,positions,64,bound,16));
    for(i=0;i<out.face_count;i++) {
        const rf_geomod_face *face=faces+i;double a=0;
        for(j=0;j<face->count;j++) {
            const rf_geomod_vertex *p=output+face->first+j,*q=output+face->first+(j+1)%face->count;
            CHECK(fabsf(p->uv[0]-p->position[0])<1e-6f && fabsf(p->uv[1]-p->position[2])<1e-6f);
            a+=(double)p->position[0]*q->position[2]-(double)p->position[2]*q->position[0];
            if(out_origins[i].owner==80)CHECK(fabsf(p->position[2])>=3-1e-5f);
        }
        if(out_origins[i].owner==80){roof_area+=fabs(a)*.5;CHECK(face->material==10 && face->source_face==478 && out_origins[i].reference==164);}
        else {other_area+=fabs(a)*.5;CHECK(out_origins[i].owner==999 && face->material==3 && face->source_face==900);}
    }
    CHECK(fabs(roof_area-8)<1e-5 && fabs(other_area-32)<1e-5);
    /* Output exhaustion and malformed planes must not partially publish. */
    memcpy(saved_v,output,sizeof(output));memcpy(saved_f,faces,sizeof(faces));memcpy(saved_o,out_origins,sizeof(out_origins));before=out;
    CHECK(rf_geomod_publication_clip_neighbors(&mesh,input,&air,1,&work,output,1,faces,16,out_origins,&out)==RF_RANGE);
    CHECK(!memcmp(saved_v,output,sizeof(output)) && !memcmp(saved_f,faces,sizeof(faces)) && !memcmp(saved_o,out_origins,sizeof(out_origins)) && !memcmp(&before,&out,sizeof(out)));
    planes[4][0]=NAN;
    CHECK(rf_geomod_publication_clip_neighbors(&mesh,input,&air,1,&work,output,64,faces,16,out_origins,&out)==RF_FORMAT);
    CHECK(!memcmp(saved_v,output,sizeof(output)) && !memcmp(saved_f,faces,sizeof(faces)) && !memcmp(saved_o,out_origins,sizeof(out_origins)) && !memcmp(&before,&out,sizeof(out)));
    CHECK(!rf_geomod_publication_clip_neighbors(&mesh,input,NULL,0,&work,output,64,faces,16,out_origins,&out));
    CHECK(out.face_count==2 && out.vertex_count==8);
    puts("PASS hollow roof boundary: real air-prism slice, strict collision, owner isolation, UV/provenance and atomic rejection");return 0;
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
    CHECK(!rounded_boundary());
    CHECK(!hollow_roof_boundary());
    puts("PASS publication");
    return 0;
}
