#ifndef RF_GEOMOD_SOLID_CLIP_H
#define RF_GEOMOD_SOLID_CLIP_H
#include "rf/geomod.h"
typedef struct rf_geomod_solid_clip_work {
    rf_geomod_vertex *vertices[2];
    rf_geomod_fragment *fragments[2];
    uint32_t vertex_capacity,fragment_capacity;
    uint16_t *edges[2]; /* Optional outgoing-edge support IDs, per corner. */
} rf_geomod_solid_clip_work;
typedef struct rf_geomod_solid_clip_result {
    const rf_geomod_vertex *vertices;
    const rf_geomod_fragment *fragments;
    uint32_t vertex_count,fragment_count;
    const uint16_t *edges;
} rf_geomod_solid_clip_result;
/* Partition a convex polygon by a closed solid's face planes, then keep cells
 * inside (1) or outside (0) its actual volume using oriented solid angles.
 * Supports nonconvex/disconnected solids composed of convex planar faces.
 * Caller establishes closed, consistently outward topology. Boundary cells
 * are discarded in both modes. Uses the shared splitter's1e-5 tolerance.
 * Practical reconstruction, not original binary arithmetic. No allocation;
 * both work banks must be disjoint and large enough for intermediate cells.
 * Input, work and result must be disjoint. Failure may change work, never
 * result. Output borrows a work bank until reuse. No
 * mesh publication, cap winding reversal or automatic capacity growth. */
int rf_geomod_polygon_clip_solid(const rf_geomod_vertex *,uint32_t,
    const rf_collision_face *,uint32_t,uint32_t,
    rf_geomod_solid_clip_work *,rf_geomod_solid_clip_result *);
/* Same geometry with outgoing-edge provenance. input_edges has polygon-count
 * entries; solid_planes has solid-face-count entries. Both work edge banks
 * must have vertex_capacity entries. IDs are opaque and are not renumbered.
 * This propagates support IDs; it does not snap intersections to three planes. */
int rf_geomod_polygon_clip_solid_tracked(const rf_geomod_vertex *,uint32_t,
    const rf_collision_face *,uint32_t,uint32_t,const uint16_t *input_edges,
    const uint16_t *solid_planes,rf_geomod_solid_clip_work *,rf_geomod_solid_clip_result *);
#endif
