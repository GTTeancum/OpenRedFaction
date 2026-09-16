#ifndef RF_AUTHORED_RESET_PLACEMENT_H
#define RF_AUTHORED_RESET_PLACEMENT_H
#include "rf/checkpoint_placement.h"

/* Authored subset reset admission. original_room is the IMMUTABLE FULL compiled
 * room tree, including source sides and unchanged floor/beam/liquid faces.
 * source_planes describe the validated closed OUTWARD convex authored solid;
 * never supply the inward room shell or merely the published post fragment.
 * placement.world may be the current overlay world: only replaced_room is
 * substituted with original_room for this pure query; all other rooms remain.
 *
 * Uses actual1..8 body spheres, full original-room surface/air classification
 * and standing support from checkpoint_placement, including its .002 clearance.
 * Adds explicit source-solid center rejection (inside means all signed plane
 * distances<=0, NOT inward-cavity containment). No allocation, scratch mutation,
 * source edits, callbacks, body movement, teleports or collision publication.
 * Caller verifies grounded upright/unattached mode and absence of unsupported actors/
 * movers exactly as checkpoint placement requires. Source/tree/world/body views
 * are borrowed and must remain stable. Actual grounded crouched spheres are
 * supported too; this query does not replace them with a standing capsule.
 *1..32 unit outward source planes.
 * RF_OK admits reset; RF_NOT_FOUND rejects with optional sphere/reason result.
 * Other errors preserve result. Invoke before clone/reset transaction publish.
 */
int rf_authored_reset_standing_check(const rf_collision_tree *original_room,
    const float (*source_planes)[4],uint32_t source_plane_count,
    const rf_checkpoint_placement *placement,float dt,float class_speed,
    rf_checkpoint_placement_result *result);
#endif
