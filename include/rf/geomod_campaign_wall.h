#ifndef RF_GEOMOD_CAMPAIGN_WALL_H
#define RF_GEOMOD_CAMPAIGN_WALL_H
#include "rf/geomod_solid_clip.h"

/* Build inward-facing boundary of a closed cutter in rock outside authored
 * air volumes. Output polygon material/UV comes from the cutter. The caller
 * owns all buffers; union scratch, cutter and output arrays must be disjoint.
 * This is geometry preparation only: no room clipping, collision tree,
 * rendered-face replacement, transaction, or save journal. Output view is
 * unchanged on failure, though supplied output arrays may be modified. */
int rf_geomod_campaign_wall_build(const rf_geomod_mesh_view *cutter,
    const rf_geomod_solid_clip_source *air,uint32_t air_count,
    rf_geomod_solid_union_work *work,rf_geomod_vertex *vertices,
    uint32_t vertex_capacity,rf_geomod_face *faces,uint32_t face_capacity,
    rf_geomod_mesh_view *out);
#endif
