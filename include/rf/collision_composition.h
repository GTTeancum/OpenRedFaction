#ifndef RF_COLLISION_COMPOSITION_H
#define RF_COLLISION_COMPOSITION_H
#include "rf/collision.h"
#include <stddef.h>
typedef struct rf_collision_composition rf_collision_composition;
typedef struct rf_collision_composition_allocator {
    void *(*allocate)(void *, size_t);
    void (*release)(void *, void *);
    void *context;
} rf_collision_composition_allocator;
typedef struct rf_collision_composition_view {
    const rf_collision_tree *tree;
    const uint32_t *face_ids; /* Source-order metadata IDs, for overlay_bind. */
    uint32_t count, generation, resident_bytes, peak_bytes;
} rf_collision_composition_view;
/* Base faces and their vertices remain immutable and alive through close.
 * base_ids has one compiled face ID per BASE TREE-ORDER face, NOT source-order.
 * Compiled IDs must be unique/non-UINT32_MAX; each replaced ID must occur once.
 * Unreplaced faces retain exact descriptors/filters/vertex pointers. Composition
 * restores ascending compiled-ID order before appending replacement faces.
 * Budget conservatively charges owner/maps, borrowed base tree allocated_bytes,
 * old and pending trees, and transient build scratch; excludes caller-owned
 * borrowed vertex storage, original geometry/room descriptors and allocator
 * overhead. Caller must charge those separately against the 64MiB global cap.
 * capacity includes all retained and replacement faces, not just edited faces.
 * Optional allocator applies to owner/work allocation; tree allocator is core's.
 * *out must be NULL. No global allocator mutation or allocation hook state.
 */
int rf_collision_composition_open(const rf_collision_tree *base, const uint32_t *base_ids,
                                  const uint32_t *replaced_ids, uint32_t replaced_count, uint32_t capacity,
                                  uint32_t budget, const rf_collision_composition_allocator *,
                                  rf_collision_composition **out);
void rf_collision_composition_close(rf_collision_composition **);
int rf_collision_composition_get(const rf_collision_composition *, rf_collision_composition_view *);
/* Builds one complete candidate. Existing pending candidate must be committed or
 * aborted first. Face descriptors are copied; replacement VERTICES are borrowed
 * until the candidate is aborted, superseded, reset or closed. Metadata IDs may
 * repeat (e.g. generated surfaces share a reference), but cannot be UINT32_MAX.
 * Invalid inputs, capacity, budget and allocation failure preserve active state.
 * No external overlay/render state is changed. */
int rf_collision_composition_prepare(rf_collision_composition *, const rf_collision_face *,
                                     const uint32_t *metadata_ids, uint32_t count);
/* A complete current publication for one source; an unedited source supplies
 * its original windows, while a fully removed source supplies zero faces.
 * Groups partition the owner's replaced IDs exactly once. Omitted/duplicate
 * ownership rejects the candidate rather than silently removing another source.
 * Replacement metadata may reference retained neighbors, as for single-source
 * publication. Vertices retain the same borrow lifetime as prepare(). */
typedef struct rf_collision_composition_group {
    const uint32_t *replaced_ids;
    uint32_t replaced_count;
    const rf_collision_face *faces;
    const uint32_t *metadata_ids;
    uint32_t face_count;
} rf_collision_composition_group;
/* Uses one room tree, one bounded scratch allocation and one pending commit.
 * All groups describe the complete current state, not incremental deltas. */
int rf_collision_composition_prepare_groups(rf_collision_composition *,
    const rf_collision_composition_group *, uint32_t group_count);
int rf_collision_composition_prepare_reset(rf_collision_composition *);
int rf_collision_composition_pending(const rf_collision_composition *, rf_collision_composition_view *);
/* First bind pending.tree/face_ids to the overlay, then commit, so no live
 * overlay borrows the tree being released. On bind failure, abort. Both reset
 * and ordinary candidates follow this protocol. Commit allocates nothing. */
int rf_collision_composition_commit(rf_collision_composition *);
void rf_collision_composition_abort(rf_collision_composition *);
#endif
