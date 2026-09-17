#ifndef RF_GEOMOD_SOLID_CLIP_H
#define RF_GEOMOD_SOLID_CLIP_H
#include "rf/geomod.h"
typedef struct rf_geomod_solid_clip_work {
    rf_geomod_vertex *vertices[2];
    rf_geomod_fragment *fragments[2];
    uint32_t vertex_capacity,fragment_capacity;
} rf_geomod_solid_clip_work;
typedef struct rf_geomod_solid_clip_result {
    const rf_geomod_vertex *vertices;
    const rf_geomod_fragment *fragments;
    uint32_t vertex_count,fragment_count;
} rf_geomod_solid_clip_result;
/* Partition a convex polygon by a closed solid's face planes, then keep cells
 * inside (1) or outside (0) its actual volume using oriented solid angles.
 * Supports nonconvex/disconnected solids composed of convex planar faces.
 * Caller establishes closed, consistently outward topology. Boundary cells
 * are discarded in both modes. Uses the shared splitter's1e-5 tolerance.
 * Practical reconstruction, not original binary arithmetic. No allocation;
 * both work banks must be disjoint and large enough for intermediate cells.
 * Input, work and result must be disjoint. Failure may change work, never
 * result. Output borrows a work bank until reuse. No support-ID propagation,
 * mesh publication, cap winding reversal or automatic capacity growth. */
int rf_geomod_polygon_clip_solid(const rf_geomod_vertex *,uint32_t,
    const rf_collision_face *,uint32_t,uint32_t,
    rf_geomod_solid_clip_work *,rf_geomod_solid_clip_result *);
#endif
