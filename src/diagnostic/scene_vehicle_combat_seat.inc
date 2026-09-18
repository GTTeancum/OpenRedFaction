#ifndef SCENE_VEHICLE_COMBAT_SEAT_INC
#define SCENE_VEHICLE_COMBAT_SEAT_INC
#include "rf/apc_checkpoint.h"
#include "rf/jeep_checkpoint.h"
#include "rf/player_checkpoint.h"
/* Pure saved-seat placement. Output borrows immutable world/player resources;
 * Jeep's one sphere lives in caller scratch, never a stack local. RFPL inventory
 * validation, pending-world fit and publication remain caller responsibilities. */
static int scene_vehicle_combat_seat_prepare(const scene_stream *s,const rf_vehicle_checkpoint *host,
    const rf_player_checkpoint *player,const char *model,const char *seat,const float *locked_basis,
    uint32_t head_only,rf_checkpoint_placement *out,rf_physics_sphere *scratch)
{
    rf_checkpoint_placement value={0};rf_physics_sphere head;scene_vehicle_pose pose;
    float tag[12],offset[3],distance=0;const float *eye;uint32_t i,j,index=0;int32_t tag_index=-1;int status;
    if(!s || !s->driller || !s->driller->tags.items || !s->collision || !host || !player || !out || !scratch ||
       !scene_actor_body.spheres.items || !scene_actor_body.spheres.count ||
       scene_actor_body.spheres.count>(head_only?64u:8u))return RF_RANGE;
    if(strcmp(s->driller->model,model) || !host->player_occupied)return RF_FORMAT;
    for(i=0;i<s->driller->tags.count;i++)if(!strcmp(s->driller->tags.items[i].name,seat)){tag_index=(int32_t)i;break;}
    if(tag_index<0)return RF_NOT_FOUND;
    status=rf_static_model_tag_place(&s->driller->tags,tag_index,host->orientation,host->position,tag);if(status)return status;
    memcpy(pose.basis,locked_basis?locked_basis:tag,36);memcpy(pose.position,tag+9,12);eye=scene_driller_player_eye_offset();
    for(i=0;i<3;i++)if(!isfinite(eye[i]) || !isfinite(player->position[i]))return RF_FORMAT;
    scene_driller_point((const float[3]){0},pose.basis,eye,offset);
    for(i=0;i<3;i++){float d;pose.position[i]-=offset[i];d=pose.position[i]-player->position[i];distance+=d*d;}
    if(!scene_vehicle_pose_valid(&pose) || !isfinite(distance) || distance>.0001f)return RF_FORMAT;
    for(i=0;i<scene_actor_body.spheres.count;i++){
        const rf_physics_sphere *sphere=scene_actor_body.spheres.items+i;
        if(!isfinite(sphere->radius) || sphere->radius<=.002f)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(sphere->center[j]))return RF_FORMAT;
        if(sphere->center[1]>scene_actor_body.spheres.items[index].center[1])index=i;
    }
    head=scene_actor_body.spheres.items[index];
    value.world=s->collision;value.replaced_room=s->terrain_collision.room;
    value.query_flags=(scene_actor_body.state.state_124&~0x1000u)|4u;
    memcpy(value.position,pose.position,12);memcpy(value.basis,pose.basis,36);
    value.spheres=head_only?scratch:scene_actor_body.spheres.items;
    value.count=head_only?1:scene_actor_body.spheres.count;
    if(head_only)*scratch=head;
    *out=value;return RF_OK;
}
static int scene_apc_checkpoint_seat_prepare(const scene_stream *s,const rf_apc_checkpoint *record,
    const rf_player_checkpoint *player,rf_checkpoint_placement *out,rf_physics_sphere *scratch)
{
    int status;if(!record)return RF_RANGE;status=rf_apc_checkpoint_validate(record);if(status)return status;
    return scene_vehicle_combat_seat_prepare(s,&record->vehicle,player,"APC.v3m","interface_1",NULL,0,out,scratch);
}
static int scene_jeep_checkpoint_seat_prepare(const scene_stream *s,const rf_jeep_checkpoint *record,
    const rf_player_checkpoint *player,rf_checkpoint_placement *out,rf_physics_sphere *scratch)
{
    int status;if(!record)return RF_RANGE;status=rf_jeep_checkpoint_validate(record);if(status)return status;
    return scene_vehicle_combat_seat_prepare(s,&record->vehicle,player,"Jeep01.v3m",record->role?"interface_2":"interface_1",
        record->role?record->aim_reference:NULL,1,out,scratch);
}
#endif
