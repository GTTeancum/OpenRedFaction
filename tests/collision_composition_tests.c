#include "rf/collision_composition.h"
#include "rf/geometry.h"
#include "rf/geomod.h"
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
typedef struct allocation_state {
    uint32_t fail, live, calls;
} allocation_state;
static void *test_allocate(void *v, size_t n) {
    allocation_state *s = v;
    void *p;
    s->calls++;
    if (s->fail) {
        s->fail = 0;
        return NULL;
    }
    p = malloc(n);
    if (p)
        s->live++;
    return p;
}
static void test_release(void *v, void *p) {
    allocation_state *s = v;
    if (p)
        s->live--;
    free(p);
}
static rf_geomod_vertex vertices[28];
static rf_geomod_face descriptors[7];
static float positions[28][3];
static rf_collision_face faces[7];
static rf_collision_face_filter filters[7];
static void quad(uint32_t id, const float points[4][3], uint32_t flags) {
    uint32_t i;
    descriptors[id] = (rf_geomod_face){id * 4, 4, 0, id};
    for (i = 0; i < 4; i++)
        memcpy(vertices[id * 4 + i].position, points[i], 12);
    filters[id] = (rf_collision_face_filter){0x1004, flags, -1, 1, 0, 0};
}
static int query(const rf_collision_composition_view *v, const float p[3], const float d[3], uint32_t flags,
                 uint32_t expected, uint32_t metadata, float *y) {
    rf_collision_tree_hit h = {0};
    uint32_t yes;
    const rf_collision_tree *t = v->tree;
    int s = rf_collision_thin_tree(t->nodes, t->node_count, t->faces, t->face_count, flags, p, d, 1, t->stack,
                                   t->node_capacity, &h, &yes);
    if (s || yes != expected)
        return 0;
    if (yes && v->face_ids[t->source_indices[h.face_index]] != metadata)
        return 0;
    if (yes && y)
        *y = h.hit.point[1];
    return 1;
}
int main(void) {
    static const float points[7][4][3] = {{{1, -1, -1}, {1, 1, -1}, {1, 1, 1}, {1, -1, 1}},
                                          {{-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}},
                                          {{-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, -1, -1}},
                                          {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}},
                                          {{-4, -1, -4}, {-4, -1, 4}, {4, -1, 4}, {4, -1, -4}},
                                          {{5, -2, -1}, {5, -2, 1}, {7, -2, 1}, {7, -2, -1}},
                                          {{1, .5f, -1}, {1, 1, -1}, {1, 1, 1}, {1, .5f, 1}}};
    uint32_t source_ids[6] = {149, 150, 151, 152, 171, 6656}, tree_ids[6], replace[4] = {149, 150, 151, 152},
             id = 149, i, k;
    rf_geomod_mesh_view mesh;
    rf_collision_tree base = {0};
    rf_collision_composition *owner = NULL, *limited = NULL;
    rf_collision_composition_view initial, active, pending, saved;
    allocation_state alloc = {0};
    rf_collision_composition_allocator allocator = {test_allocate, test_release, &alloc};
    rf_geometry_collision_room room = {0};
    rf_collision_room_view room_view = {0};
    rf_geometry_collision_world world = {0};
    rf_geometry_collision_overlay overlay = {0};
    float p[3] = {2, 0, 0}, d[3] = {-4, 0, 0}, floor_p[3] = {0, 0, 0}, down[3] = {0, -3, 0},
          water_p[3] = {6, -1, 0}, y;
    for (i = 0; i < 7; i++)
        quad(i, points[i], i == 5 ? 260 : 256);
    mesh = (rf_geomod_mesh_view){vertices, descriptors, 28, 7, 0};
    CHECK(!rf_geomod_collision_faces(&mesh, filters, positions, 28, faces, 7));
    CHECK(!rf_collision_tree_open(faces, 6, 100000, &base));
    for (i = 0; i < 6; i++)
        tree_ids[i] = source_ids[base.source_indices[i]];
    alloc.fail = 1;
    CHECK(rf_collision_composition_open(&base, tree_ids, replace, 4, 8, 100000, &allocator, &owner) == RF_IO);
    CHECK(!owner && alloc.live == 0);
    CHECK(!rf_collision_composition_open(&base, tree_ids, replace, 4, 8, 100000, &allocator, &owner));
    CHECK(alloc.live == 1);
    CHECK(!rf_collision_composition_get(owner, &initial));
    CHECK(query(&initial, p, d, 4, 1, 149, NULL));
    CHECK(query(&initial, floor_p, down, 4, 1, 171, &y) && y == -1);
    CHECK(query(&initial, water_p, down, 0x1004, 1, 6656, &y) && y == -2);
    CHECK(query(&initial, water_p, down, 4, 0, 0, NULL));
    room.tree = base;
    room_view.tree = &room.tree;
    world.rooms = &room;
    world.views = &room_view;
    world.room_count = 1;
    CHECK(!rf_geometry_collision_overlay_open(&world, 0, 8, 10000, &overlay));
    CHECK(!rf_collision_composition_prepare(owner, faces + 6, &id, 1));
    CHECK(!rf_collision_composition_pending(owner, &pending));
    CHECK(pending.count == 3);
    CHECK(!rf_collision_composition_get(owner, &active));
    CHECK(query(&active, p, d, 4, 1, 149, NULL));
    CHECK(query(&pending, p, d, 4, 0, 0, NULL));
    p[1] = .75f;
    CHECK(query(&pending, p, d, 4, 1, 149, NULL));
    p[1] = 0;
    CHECK(query(&pending, floor_p, down, 4, 1, 171, &y) && y == -1);
    CHECK(query(&pending, water_p, down, 0x1004, 1, 6656, &y) && y == -2);
    CHECK(query(&pending, water_p, down, 4, 0, 0, NULL));
    for (i = 0; i < pending.tree->face_count; i++) {
        uint32_t meta = pending.face_ids[pending.tree->source_indices[i]];
        if (meta != 171 && meta != 6656)
            continue;
        for (k = 0; k < base.face_count; k++)
            if (tree_ids[k] == meta)
                break;
        CHECK(k < base.face_count);
        CHECK(!memcmp(pending.tree->faces + i, base.faces + k, sizeof(rf_collision_face)));
    }
    CHECK(!rf_geometry_collision_overlay_bind(&overlay, pending.tree, pending.face_ids, pending.count));
    CHECK(overlay.world.liquids[0].face_count == 3);
    CHECK(!rf_collision_composition_commit(owner));
    CHECK(!rf_collision_composition_get(owner, &active));
    CHECK(active.generation == 1 && active.count == 3);
    saved = active;
    alloc.fail = 1;
    CHECK(rf_collision_composition_prepare(owner, faces + 6, &id, 1) == RF_IO);
    CHECK(!rf_collision_composition_get(owner, &active));
    CHECK(active.tree->storage == saved.tree->storage && active.generation == saved.generation);
    CHECK(alloc.live == 1);
    CHECK(rf_collision_composition_prepare(owner, faces + 6, &id, 7) == RF_RANGE);
    CHECK(query(&active, p, d, 4, 0, 0, NULL));
    {
        rf_collision_face invalid = faces[6];
        invalid.minimum[0] = invalid.maximum[0] + 1;
        CHECK(rf_collision_composition_prepare(owner, &invalid, &id, 1) == RF_FORMAT);
        CHECK(alloc.live == 1);
        CHECK(!rf_collision_composition_get(owner, &active));
        CHECK(active.generation == saved.generation);
    }
    CHECK(!rf_collision_composition_prepare(owner, NULL, NULL, 0));
    CHECK(!rf_collision_composition_pending(owner, &pending));
    CHECK(pending.count == 2);
    CHECK(pending.peak_bytes >= pending.resident_bytes);
    rf_collision_composition_abort(owner);
    CHECK(!rf_collision_composition_get(owner, &active));
    CHECK(active.count == 3 && active.generation == 1);
    CHECK(!rf_collision_composition_open(&base, tree_ids, replace, 4, 8, initial.resident_bytes + 1,
                                         &allocator, &limited));
    CHECK(rf_collision_composition_prepare(limited, faces + 6, &id, 1) == RF_RANGE);
    CHECK(!rf_collision_composition_get(limited, &active));
    CHECK(active.generation == 0 && active.count == 6);
    rf_collision_composition_close(&limited);
    CHECK(!rf_collision_composition_prepare_reset(owner));
    CHECK(!rf_collision_composition_pending(owner, &pending));
    CHECK(pending.count == 6);
    CHECK(query(&pending, p, d, 4, 1, 149, NULL));
    CHECK(!rf_geometry_collision_overlay_bind(&overlay, pending.tree, pending.face_ids, pending.count));
    CHECK(!rf_collision_composition_commit(owner));
    CHECK(!rf_collision_composition_get(owner, &active));
    CHECK(active.count == 6 && active.generation == 2);
    CHECK(query(&active, water_p, down, 0x1004, 1, 6656, &y) && y == -2);
    CHECK(overlay.world.liquids[0].face_count == 6);
    printf("PASS composition baseline%u restored%u peak%u allocator_calls%u\n", initial.count, active.count,
           active.peak_bytes, alloc.calls);
    rf_geometry_collision_overlay_close(&overlay);
    rf_collision_composition_close(&owner);
    CHECK(!owner && alloc.live == 0);
    rf_collision_tree_close(&base);
    return 0;
}
