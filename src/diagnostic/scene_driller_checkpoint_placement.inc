/* Include after collection staging and Driller physics definitions. No live
 * runtime or registry registration is required. This tests static terrain and
 * staged detached pieces; the caller separately admits player/actor overlap. */
#ifndef SCENE_DRILLER_CHECKPOINT_PLACEMENT_INC
#define SCENE_DRILLER_CHECKPOINT_PLACEMENT_INC
#include "rf/vehicle_checkpoint.h"
static int scene_driller_checkpoint_placement_world(const scene_stream *scene,const scene_authored_checkpoint_stage *stage,
    const scene_driller_physics *physics,const rf_vehicle_checkpoint *record,rf_checkpoint_placement *out)
{
    rf_collision_composition_view composition;rf_geomod_terrain_view full;
    rf_checkpoint_placement body={0};int status;
    if(!scene || !stage || !physics || !record || !out || !stage->owns_pending ||
       !stage->lighting || !stage->lighting->draw_ready || stage->lighting->committed ||
       !scene->terrain_publication || !scene->terrain_publication->has_pending ||
       !scene->collision || !physics->sphere_count || physics->sphere_count>8)return RF_RANGE;
    status=rf_vehicle_checkpoint_validate(record);if(status)return status;
    status=rf_collision_composition_pending(scene->terrain_publication->composition,&composition);if(status)return status;
    /* Pending composition is the complete replacement room, including retained
     * neighbors. The source publication alone omits those physical surfaces. */
    full=stage->publication;full.faces=composition.tree->faces;
    full.tree=composition.tree;full.mesh.face_count=composition.count;
    body.world=scene->collision;body.replaced_room=scene->terrain_collision.room;
    body.query_flags=4; /* Same physical-face eligibility as live Driller. */
    body.spheres=physics->spheres;body.count=physics->sphere_count;
    memcpy(body.position,record->position,sizeof(body.position));
    memcpy(body.basis,record->orientation,sizeof(body.basis));
    /* Airborne rigid bodies are valid. Do not use player standing/support or
     * sweep/resolve to alter a saved pose during precommit admission. */
    status=rf_checkpoint_placement_check(&full,&body,NULL);if(status)return status;
    *out=body;return RF_OK;
}
static int scene_driller_checkpoint_placement(scene_authored_collection_stage *stage,
    const scene_driller_physics *physics,const rf_vehicle_checkpoint *record)
{
    rf_checkpoint_placement body;int status;if(!stage)return RF_RANGE;
    status=scene_driller_checkpoint_placement_world(&stage->candidate,&stage->shared,physics,record,&body);if(status)return status;
    return scene_collection_player_placement(&stage->candidate,&body);
}
static int scene_driller_checkpoint_placement_single(scene_authored_checkpoint_stage *stage,
    const scene_driller_physics *physics,const rf_vehicle_checkpoint *record)
{
    rf_checkpoint_placement body;int status;
    if(!stage || !stage->scene || stage->scene->terrain_source_count>1)return RF_RANGE;
    status=scene_driller_checkpoint_placement_world(stage->scene,stage,physics,record,&body);if(status)return status;
    /* Never consult the live detached alias: only this load's reconstructed bank. */
    return rf_geomod_piece_registry_placement_check(stage->pieces,&body);
}
#endif
