#include "rf/collision_composition.h"
#include "rf/vpp.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
struct rf_collision_composition {
    rf_collision_tree base, active, pending;
    rf_collision_composition_allocator allocator;
    uint32_t *base_ids, *identity, *order, *replaced, *active_ids, *pending_ids;
    uint32_t base_count, replaced_count, capacity, budget, owner_bytes, base_bytes, peak_bytes;
    uint32_t active_owned, pending_kind, generation;
};
static void *allocate(void *context, size_t n) {
    (void)context;
    return malloc(n);
}
static void release(void *context, void *p) {
    (void)context;
    free(p);
}
static uint32_t resident(const rf_collision_composition *o) {
    return o->owner_bytes + o->base_bytes + (o->active_owned ? o->active.allocated_bytes : 0) +
           (o->pending_kind == 1 ? o->pending.allocated_bytes : 0);
}
static int replaced(const rf_collision_composition *o, uint32_t id) {
    uint32_t i;
    for (i = 0; i < o->replaced_count; i++)
        if (o->replaced[i] == id)
            return 1;
    return 0;
}
int rf_collision_composition_open(const rf_collision_tree *base, const uint32_t *ids, const uint32_t *replace,
                                  uint32_t n, uint32_t capacity, uint32_t budget,
                                  const rf_collision_composition_allocator *allocator,
                                  rf_collision_composition **out) {
    rf_collision_composition *o;
    rf_collision_composition_allocator a = {allocate, release, NULL};
    uint64_t bytes;
    uint32_t i, j, found, *p;
    if (!base || !out || *out || (base->face_count && (!base->faces || !ids)) ||
        (base->node_count && !base->nodes) || (n && !replace) || n > base->face_count ||
        capacity < base->face_count)
        return RF_RANGE;
    if (allocator) {
        if (!allocator->allocate || !allocator->release)
            return RF_RANGE;
        a = *allocator;
    }
    for (i = 0; i < base->face_count; i++) {
        if (ids[i] == UINT32_MAX)
            return RF_FORMAT;
        for (j = 0; j < i; j++)
            if (ids[i] == ids[j])
                return RF_FORMAT;
    }
    for (i = 0; i < n; i++) {
        found = 0;
        for (j = 0; j < i; j++)
            if (replace[i] == replace[j])
                return RF_FORMAT;
        for (j = 0; j < base->face_count; j++)
            found += ids[j] == replace[i];
        if (found != 1)
            return RF_FORMAT;
    }
    bytes = sizeof(*o) + (uint64_t)(3ull * base->face_count + n + 2ull * capacity) * sizeof(uint32_t);
    if (bytes + base->allocated_bytes > budget || bytes > UINT32_MAX)
        return RF_RANGE;
    o = a.allocate(a.context, (size_t)bytes);
    if (!o)
        return RF_IO;
    memset(o, 0, (size_t)bytes);
    o->allocator = a;
    o->base = *base;
    o->base_count = base->face_count;
    o->replaced_count = n;
    o->capacity = capacity;
    o->budget = budget;
    o->owner_bytes = (uint32_t)bytes;
    o->base_bytes = base->allocated_bytes;
    p = (uint32_t *)(o + 1);
    o->base_ids = p;
    p += base->face_count;
    o->identity = p;
    p += base->face_count;
    o->order = p;
    p += base->face_count;
    o->replaced = p;
    p += n;
    o->active_ids = p;
    p += capacity;
    o->pending_ids = p;
    if (base->face_count)
        memcpy(o->base_ids, ids, base->face_count * 4);
    if (n)
        memcpy(o->replaced, replace, n * 4);
    for (i = 0; i < base->face_count; i++) {
        uint32_t at = i;
        o->identity[i] = i;
        while (at && ids[o->order[at - 1]] > ids[i]) {
            o->order[at] = o->order[at - 1];
            at--;
        }
        o->order[at] = i;
    }
    o->base.source_indices = o->identity;
    o->peak_bytes = resident(o);
    *out = o;
    return RF_OK;
}
void rf_collision_composition_abort(rf_collision_composition *o) {
    if (!o)
        return;
    if (o->pending_kind == 1)
        rf_collision_tree_close(&o->pending);
    o->pending_kind = 0;
}
void rf_collision_composition_close(rf_collision_composition **owner) {
    rf_collision_composition *o;
    rf_collision_composition_allocator a;
    if (!owner || !*owner)
        return;
    o = *owner;
    a = o->allocator;
    rf_collision_composition_abort(o);
    if (o->active_owned)
        rf_collision_tree_close(&o->active);
    a.release(a.context, o);
    *owner = NULL;
}
int rf_collision_composition_get(const rf_collision_composition *o, rf_collision_composition_view *view) {
    rf_collision_composition_view v;
    if (!o || !view)
        return RF_RANGE;
    v.tree = o->active_owned ? &o->active : &o->base;
    v.face_ids = o->active_owned ? o->active_ids : o->base_ids;
    v.count = v.tree->face_count;
    v.generation = o->generation;
    v.resident_bytes = resident(o);
    v.peak_bytes = o->peak_bytes;
    *view = v;
    return RF_OK;
}
int rf_collision_composition_pending(const rf_collision_composition *o, rf_collision_composition_view *view) {
    rf_collision_composition_view v;
    if (!o || !view || !o->pending_kind)
        return RF_RANGE;
    v.tree = o->pending_kind == 1 ? &o->pending : &o->base;
    v.face_ids = o->pending_kind == 1 ? o->pending_ids : o->base_ids;
    v.count = v.tree->face_count;
    v.generation = o->generation + 1;
    v.resident_bytes = resident(o);
    v.peak_bytes = o->peak_bytes;
    *view = v;
    return RF_OK;
}
int rf_collision_composition_prepare(rf_collision_composition *o, const rf_collision_face *replacement,
                                     const uint32_t *ids, uint32_t count) {
    uint64_t total, work_bytes, used;
    uint32_t n, i, j, scratch_bytes;
    unsigned char *work;
    rf_collision_face *faces;
    rf_collision_tree tree = {0};
    int status;
    if (!o || o->pending_kind || (count && (!replacement || !ids)))
        return RF_RANGE;
    total = (uint64_t)o->base_count - o->replaced_count + count;
    if (total > o->capacity)
        return RF_RANGE;
    n = (uint32_t)total;
    for (i = 0; i < count; i++)
        if (ids[i] == UINT32_MAX)
            return RF_FORMAT;
    work_bytes = (uint64_t)n * (2 * sizeof(rf_collision_face) + sizeof(uint32_t) + 1);
    used = resident(o);
    /* Non-null scratch is required even for an empty tree. */
    if (!work_bytes)
        work_bytes = sizeof(void *);
    if (work_bytes > UINT32_MAX || used + work_bytes + sizeof(tree) > o->budget)
        return RF_RANGE;
    work = o->allocator.allocate(o->allocator.context, (size_t)work_bytes);
    if (!work)
        return RF_IO;
    faces = (rf_collision_face *)work;
    j = 0;
    for (i = 0; i < o->base_count; i++) {
        uint32_t index = o->order[i];
        if (replaced(o, o->base_ids[index]))
            continue;
        faces[j] = o->base.faces[index];
        o->pending_ids[j++] = o->base_ids[index];
    }
    for (i = 0; i < count; i++) {
        faces[j] = replacement[i];
        o->pending_ids[j++] = ids[i];
    }
    scratch_bytes = n * (uint32_t)(sizeof(rf_collision_face) + sizeof(uint32_t) + 1);
    status = rf_collision_tree_open_scratch(faces, n, o->budget - (uint32_t)(used + work_bytes), &tree,
                                            work + (size_t)n * sizeof(rf_collision_face), scratch_bytes);
    o->allocator.release(o->allocator.context, work);
    if (status)
        return status;
    o->pending = tree;
    o->pending_kind = 1;
    used += work_bytes + tree.peak_bytes;
    if (used > o->peak_bytes)
        o->peak_bytes = (uint32_t)used;
    return RF_OK;
}
int rf_collision_composition_prepare_reset(rf_collision_composition *o) {
    if (!o || o->pending_kind)
        return RF_RANGE;
    o->pending_kind = 2;
    return RF_OK;
}
int rf_collision_composition_commit(rf_collision_composition *o) {
    uint32_t *ids;
    if (!o || !o->pending_kind)
        return RF_RANGE;
    if (o->active_owned)
        rf_collision_tree_close(&o->active);
    if (o->pending_kind == 1) {
        o->active = o->pending;
        memset(&o->pending, 0, sizeof(o->pending));
        ids = o->active_ids;
        o->active_ids = o->pending_ids;
        o->pending_ids = ids;
        o->active_owned = 1;
    } else
        o->active_owned = 0;
    o->pending_kind = 0;
    o->generation++;
    return RF_OK;
}
