#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene_mover_checkpoint.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"scene mover checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_object_registry registry;rf_level_group_key keys[2]={{0}};rf_level_owned_group source={0};
    rf_group_runtime_entry entry={0},before;rf_group_runtime_collection runtime={0};
    rf_group_registered_controller controller={0};rf_group_registration registration={0};
    rf_group_attached_pose pose={0},pose_before;rf_group_registered_mover mover={0};
    rf_collision_solid_view view={0},view_before;rf_geometry_collision_movers geometry={0};
    rf_group_controller_view binding={0};rf_mover_checkpoint_record row;
    scene_mover_checkpoint_stage stage={0},empty={0};uint32_t handle,j;
    rf_object_registry_init(&registry);keys[0].uid=100;keys[1].uid=101;keys[1].position[0]=10;
    source.keys=keys;source.record.key_count=2;entry.source=&source;entry.kind=RF_GROUP_RUNTIME_TRANSLATION;
    entry.translation.motion=(rf_group_motion_state){0x2000,2,0,1,.3f,-1};entry.translation.deadline=-1;
    entry.translation.position[0]=entry.translation.pending[0]=3;entry.translation.distance=3;
    CHECK(!rf_mover_checkpoint_capture(&entry,0,&row));entry.translation.position[0]=entry.translation.pending[0]=0;
    runtime.items=&entry;runtime.count=1;controller.object_kind=8;controller.runtime=&entry;
    CHECK(!rf_object_registry_insert(&registry,&controller,&controller.handle));
    registration.registry=&registry;registration.controllers=&controller;registration.count=1;
    pose.base_position[0]=pose.position[0]=pose.public_position[0]=pose.pending[0]=2;pose.radius=1;
    for(j=0;j<3;j++)pose.base_matrix[j*3+j]=pose.input_matrix[j*3+j]=pose.output_matrix[j*3+j]=pose.pending_matrix[j*3+j]=1;
    mover.object_kind=9;mover.pose=&pose;CHECK(!rf_object_registry_insert(&registry,&mover,&mover.handle));handle=mover.handle;
    view.object_id=handle;geometry.poses=&pose;geometry.views=&view;geometry.count=1;
    binding.runtime=&entry.translation;binding.first_key=keys;binding.mover_handles=&handle;binding.mover_count=1;binding.rotation_sign=1;
    before=entry;pose_before=pose;view_before=view;
    CHECK(!scene_mover_checkpoint_prepare(&runtime,&registration,&binding,&geometry,&row,1,100,65536,&stage));
    CHECK(stage.entries[0].pose.position[0]==3 && stage.entries[0].translation.position[0]==3);
    CHECK(stage.poses[0].position[0]==5 && stage.poses[0].pending[0]==5 && stage.poses[0].minimum[0]==4 && stage.poses[0].maximum[0]==6);
    CHECK(stage.views[0].input_origin[0]==5 && stage.views[0].output_origin[0]==5 && stage.views[0].object_id==handle);
    CHECK(!memcmp(&entry,&before,sizeof(entry))&&!memcmp(&pose,&pose_before,sizeof(pose))&&!memcmp(&view,&view_before,sizeof(view)));
    /* Mutating a borrowed membership after prepare must not alter its snapshot. */
    handle^=0x10000;
    CHECK(scene_mover_checkpoint_commit(&runtime,&registration,&binding,&geometry,&stage)==RF_FORMAT);
    CHECK(!memcmp(&entry,&before,sizeof(entry))&&!memcmp(&pose,&pose_before,sizeof(pose))&&!memcmp(&view,&view_before,sizeof(view)));
    handle^=0x10000;
    CHECK(!scene_mover_checkpoint_commit(&runtime,&registration,&binding,&geometry,&stage));
    CHECK(entry.translation.position[0]==3&&entry.pose.position[0]==3&&pose.position[0]==5&&view.input_origin[0]==5);
    CHECK(runtime.items==&entry&&entry.source==&source&&controller.runtime==&entry&&mover.pose==&pose);
    CHECK(binding.runtime==&entry.translation&&binding.mover_handles==&handle&&geometry.poses==&pose&&geometry.views==&view);
    CHECK(rf_object_registry_lookup(&registry,controller.handle)==&controller&&rf_object_registry_lookup(&registry,mover.handle)==&mover);
    /* Independent rejected preparation cases below retain their original fixture. */
    entry=before;pose=pose_before;view=view_before;
    scene_mover_checkpoint_close(&stage);CHECK(!memcmp(&stage,&empty,sizeof(stage)));
    row.uid=999;
    CHECK(scene_mover_checkpoint_prepare(&runtime,&registration,&binding,&geometry,&row,1,100,65536,&stage)==RF_FORMAT);
    CHECK(!memcmp(&stage,&empty,sizeof(stage))&&!memcmp(&entry,&before,sizeof(entry))&&!memcmp(&pose,&pose_before,sizeof(pose)));
    row.uid=100;binding.general_count=1;
    CHECK(scene_mover_checkpoint_prepare(&runtime,&registration,&binding,&geometry,&row,1,100,65536,&stage)==RF_FORMAT);
    binding.general_count=0;handle^=0x10000;
    CHECK(scene_mover_checkpoint_prepare(&runtime,&registration,&binding,&geometry,&row,1,100,65536,&stage)==RF_FORMAT);
    CHECK(!memcmp(&stage,&empty,sizeof(stage))&&!memcmp(&view,&view_before,sizeof(view)));
    puts("Scene mover restore staging: UID binding, reconstructed geometry, source immutability and rejected stale/unsupported attachments passed");return 0;
}
