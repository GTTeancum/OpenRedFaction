#include "rf/level.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_level_group_key key={0};rf_group_translation_runtime r={0},saved;
    rf_group_controller_view view={0};rf_group_attached_pose pose={0};
    rf_group_pose_slot slots[2]={{0}};rf_group_attached_pose controller={0};
    uint32_t handle=1,sounds=0,i;float angle;
    key.orientation[0][0]=key.orientation[1][1]=key.orientation[2][2]=1;
    key.rotation=-90;key.timing[1]=key.timing[2]=1;
    r.motion.flags=4|0x2000;r.motion.mode=1;r.motion.next_key=-1;r.motion.terminal_key=-1;r.deadline=-1;
    CHECK(rf_group_motion_activate(&r.motion,1)==0);
    for(i=0;i<4;i++)CHECK(rf_group_rotation_tick(&r,&key,.25f,i*250,&sounds)==0);
    CHECK(r.motion.next_key==-1 && sounds==RF_GROUP_SOUND_END);
    CHECK(fabsf(r.distance-1.57079632679f)<.00001f);
    pose.radius=.5f;pose.base_position[0]=pose.position[0]=pose.public_position[0]=2;
    pose.base_matrix[0]=pose.base_matrix[4]=pose.base_matrix[8]=1;
    view.runtime=&r;view.first_key=&key;view.mover_handles=&handle;view.mover_count=1;view.rotation_sign=1;
    CHECK(rf_group_translation_bind_pose(&pose,handle,&view,1,.25f,0)==0);
    CHECK(fabsf(pose.pending[0])<.00001f && fabsf(pose.pending[2]+2)<.00001f);
    CHECK(fabsf(pose.output_matrix[2]+1)<.00001f);
    CHECK(pose.minimum[2]<=-2.5f && pose.maximum[0]>=2.5f);
    slots[1].handle=handle;slots[1].pose=&pose;
    CHECK(rf_group_commit_positions(&r.motion.flags,&controller,&view,slots,2)==0);
    CHECK(!memcmp(pose.input_matrix,pose.pending_matrix,36));
    CHECK(fabsf(pose.position[2]+2)<.00001f);
    CHECK(rf_group_motion_activate(&r.motion,1)==0);
    for(i=0;i<4;i++)CHECK(rf_group_rotation_tick(&r,&key,.25f,1000+i*250,&sounds)==0);
    CHECK(r.motion.next_key==-1 && r.distance==0);
    CHECK(rf_group_translation_bind_pose(&pose,handle,&view,1,.25f,0)==0);
    CHECK(fabsf(pose.pending[0]-2)<.00001f && fabsf(pose.pending[2])<.00001f);
    /* Zero-duration doors snap finitely; malformed timing must not mutate. */
    key.timing[1]=0;CHECK(rf_group_motion_activate(&r.motion,1)==0);
    CHECK(rf_group_rotation_tick(&r,&key,.25f,2000,&sounds)==0);
    angle=r.distance;CHECK(isfinite(angle) && angle>1.5f);
    /* Mode2 opens, waits, and returns once; dwell holds the open angle. */
    memset(&r,0,sizeof(r));r.motion.flags=4|0x2000;r.motion.mode=2;
    r.motion.next_key=-1;r.motion.terminal_key=-1;r.deadline=-1;
    key.timing[0]=1;key.timing[1]=key.timing[2]=1;
    CHECK(rf_group_motion_activate(&r.motion,1)==0);
    for(i=0;i<4;i++)CHECK(rf_group_rotation_tick(&r,&key,.25f,3000+i*250,&sounds)==0);
    CHECK(r.motion.next_key==0 && r.deadline==4750);
    angle=r.distance;CHECK(rf_group_rotation_tick(&r,&key,.25f,4000,&sounds)==0 && r.distance==angle);
    for(i=0;i<4;i++)CHECK(rf_group_rotation_tick(&r,&key,.25f,4750+i*250,&sounds)==0);
    CHECK(r.motion.next_key==-1 && r.distance==0);
    saved=r;key.rotation=NAN;
    CHECK(rf_group_rotation_tick(&r,&key,.25f,2250,&sounds)!=0 && !memcmp(&saved,&r,sizeof(r)));
    puts("rotation open/close, hinge transform, swept bounds, pose commit and finite snap PASS");return 0;
}
