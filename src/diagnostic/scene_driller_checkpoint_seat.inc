#ifndef SCENE_DRILLER_CHECKPOINT_SEAT_INC
#define SCENE_DRILLER_CHECKPOINT_SEAT_INC
/* Pure precommit preparation. Include after Driller runtime. Inventory and
 * RFPL3 validation remain the caller's responsibility; no registration, pose
 * publication, ground support or host-self collision is performed here. */
#include "rf/player_checkpoint.h"
static int scene_driller_checkpoint_seat_prepare(const scene_stream *s,
    const rf_vehicle_checkpoint *vehicle,const rf_player_checkpoint *player,
    rf_checkpoint_placement *out)
{
    scene_driller_physics physics;rf_checkpoint_placement value={0};scene_vehicle_pose pose;
    float tag[12],offset[3],distance=0;const float *eye;uint32_t i;int status;
    if(!s || !s->driller || !s->collision || !vehicle || !player || !out ||
       !scene_actor_body.spheres.items || !scene_actor_body.spheres.count || scene_actor_body.spheres.count>8)return RF_RANGE;
    status=rf_vehicle_checkpoint_validate(vehicle);if(status)return status;
    if(!vehicle->player_occupied)return RF_FORMAT;
    status=scene_driller_physics_initialize(s->driller,vehicle->position,vehicle->orientation,
        scene_gravity.acceleration,&physics);if(status)return status;
    status=scene_driller_seat_pose(s->driller,physics.state.position,physics.state.orientation,tag);if(status)return status;
    memcpy(pose.basis,tag,36);memcpy(pose.position,tag+9,12);eye=scene_driller_player_eye_offset();
    for(i=0;i<3;i++)if(!isfinite(eye[i]) || !isfinite(player->position[i]))return RF_FORMAT;
    scene_driller_point((const float[3]){0,0,0},pose.basis,eye,offset);
    for(i=0;i<3;i++){float difference;pose.position[i]-=offset[i];difference=pose.position[i]-player->position[i];distance+=difference*difference;}
    if(!scene_vehicle_pose_valid(&pose) || !isfinite(distance) || distance>.01f*.01f)return RF_FORMAT;
    value.world=s->collision;value.replaced_room=s->terrain_collision.room;
    value.query_flags=(scene_actor_body.state.state_124&~0x1000u)|4u;
    value.spheres=scene_actor_body.spheres.items;value.count=scene_actor_body.spheres.count;
    /* Validate the exact authored pose that runtime attachment will publish,
     * rather than retain checkpoint rounding error as a new seat offset. */
    memcpy(value.position,pose.position,12);memcpy(value.basis,pose.basis,36);
    *out=value;return RF_OK;
}
#endif
