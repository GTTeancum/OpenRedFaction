#ifndef RF_GEOMOD_CAMPAIGN_ROOM_H
#define RF_GEOMOD_CAMPAIGN_ROOM_H
#include "rf/geometry.h"
#include "rf/geomod_solid_clip.h"

typedef struct rf_geomod_campaign_room_scope {
    uint32_t other_solid_faces,other_detail_faces,touching_portals,portal_links;
} rf_geomod_campaign_room_scope;
/* Conservative cutter-AABB admission. Detail faces (serialized flag 8) in
 * other rooms remain authored and are counted separately; other solid faces
 * and linked portals require grouped room publication. No allocation. The
 * caller supplies portal scratch and checks zero solid/portal crossings. */
int rf_geomod_campaign_room_scope_check(const rf_geometry *,uint32_t room,
    const rf_collision_face *cutter,uint32_t cutter_count,
    rf_geometry_portal *portal_work,uint32_t portal_capacity,
    rf_geomod_campaign_room_scope *out);

/* Retain the compiled polygons of one room outside a closed cutter. Retained
 * faces preserve original winding, UV, texture index and source face ID.
 * Caller must map texture indices to live material slots on publication.
 * unchanged[i] marks faces that can reuse the original collision finalizer;
 * changed fragments use the generated-face finalizer. Output/work/geometry
 * and cutter storage are disjoint and caller-owned.
 * No allocation, wall generation, tree bind, render bind or save mutation.
 * Output view is preserved on error; scratch/output arrays may change. */
int rf_geomod_campaign_room_retain(const rf_geometry *geometry,uint32_t room,
    const rf_collision_face *cutter,uint32_t cutter_count,
    rf_geomod_solid_clip_work *work,rf_geomod_vertex *vertices,
    uint32_t vertex_capacity,rf_geomod_face *faces,uint8_t *unchanged,uint32_t face_capacity,
    rf_geomod_mesh_view *out);
#endif
