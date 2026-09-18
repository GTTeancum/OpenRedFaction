/* Typed public records + real shared inertia math. No live scene mutation,
 * geometry or ownership publication is claimed by this bounded stage test. */
#include <stdio.h>
#define RF_SCENE_DRILLER_CHECKPOINT_MATH_ONLY
#include "../src/diagnostic/scene_driller_checkpoint_adapter.inc"
#include "../src/diagnostic/scene_vehicle_combat_restore.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"vehicle restore line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vehicle_rigid_state current={0},before;rf_apc_checkpoint apc={0},apc_before;rf_jeep_checkpoint jeep={0},jeep_before;
    scene_vehicle_combat_candidate staged,sentinel;float omega[3];uint32_t i;
    const float identity[9]={1,0,0,0,1,0,0,0,1},yaw[9]={0,0,-1,0,1,0,1,0,0};
    memcpy(current.orientation,identity,36);current.inverse_inertia[0]=.5f;current.inverse_inertia[4]=.25f;current.inverse_inertia[8]=.125f;
    current.position[0]=999;current.velocity[0]=99;current.momentum[0]=33;current.force[0]=4;current.torque[1]=5;current.skip_forces=1;
    before=current;
    memcpy(apc.vehicle.orientation,yaw,36);apc.vehicle.position[0]=30;apc.vehicle.velocity[2]=6.5f;
    apc.vehicle.angular_velocity[0]=1;apc.vehicle.angular_velocity[1]=2;apc.vehicle.angular_velocity[2]=3;
    apc.vehicle.health=5000;apc.vehicle.alive=1;apc.vehicle.player_occupied=1;
    apc.primary_reserve=999;apc.secondary_reserve=15;apc.primary_random=0x12345678;apc.aim_pitch=.5f;apc_before=apc;
    CHECK(!scene_apc_checkpoint_stage_rigid(&current,&apc,&staged));
    CHECK(staged.profile==2&&staged.vehicle.health==5000&&staged.primary_reserve==999&&staged.secondary_reserve==15);
    CHECK(staged.primary_random==0x12345678&&staged.aim_pitch==.5f&&!staged.role);
    CHECK(!memcmp(staged.aim_reference,yaw,36)&&!memcmp(staged.vehicle.physics.inverse_inertia,current.inverse_inertia,36));
    /* yaw rotates diag inertia(2,4,8) into worlddiag(8,4,2). */
    CHECK(staged.vehicle.physics.momentum[0]==8&&staged.vehicle.physics.momentum[1]==8&&staged.vehicle.physics.momentum[2]==6);
    CHECK(!scene_driller_checkpoint_rotate_tensor(staged.vehicle.physics.orientation,staged.vehicle.physics.inverse_inertia,staged.vehicle.physics.momentum,omega));
    for(i=0;i<3;i++)CHECK(fabsf(omega[i]-apc.vehicle.angular_velocity[i])<.0001f);
    CHECK(!staged.vehicle.physics.force[0]&&!staged.vehicle.physics.torque[1]&&!staged.vehicle.physics.skip_forces);
    CHECK(!staged.vehicle.drill_spin&&!staged.vehicle.accepted_drill_cuts);
    CHECK(!memcmp(&current,&before,sizeof(current))&&!memcmp(&apc,&apc_before,sizeof(apc)));
    jeep.vehicle=apc.vehicle;jeep.vehicle.health=400;jeep.primary_reserve=321;jeep.role=1;jeep.primary_random=0x87654321;
    jeep.aim_pitch=-.3f;jeep.aim_yaw=1.2f;memcpy(jeep.aim_reference,identity,36);jeep_before=jeep;
    CHECK(!scene_jeep_checkpoint_stage_rigid(&current,&jeep,&staged));
    CHECK(staged.profile==3&&staged.vehicle.health==400&&staged.role==1&&staged.primary_reserve==321&&!staged.secondary_reserve);
    CHECK(staged.aim_pitch==-.3f&&staged.aim_yaw==1.2f&&staged.primary_random==0x87654321);
    CHECK(!memcmp(staged.aim_reference,identity,36)&&memcmp(staged.aim_reference,staged.vehicle.physics.orientation,36));
    CHECK(!memcmp(&current,&before,sizeof(current))&&!memcmp(&jeep,&jeep_before,sizeof(jeep)));
    memset(&sentinel,0xa5,sizeof(sentinel));staged=sentinel;
    apc.vehicle.health=5001;CHECK(scene_apc_checkpoint_stage_rigid(&current,&apc,&staged)==RF_FORMAT&&!memcmp(&staged,&sentinel,sizeof(staged)));
    apc=apc_before;apc.secondary_reserve=16;CHECK(scene_apc_checkpoint_stage_rigid(&current,&apc,&staged)==RF_FORMAT&&!memcmp(&staged,&sentinel,sizeof(staged)));
    jeep.vehicle.health=401;CHECK(scene_jeep_checkpoint_stage_rigid(&current,&jeep,&staged)==RF_FORMAT&&!memcmp(&staged,&sentinel,sizeof(staged)));
    jeep=jeep_before;jeep.aim_yaw=3.2f;CHECK(scene_jeep_checkpoint_stage_rigid(&current,&jeep,&staged)==RF_FORMAT&&!memcmp(&staged,&sentinel,sizeof(staged)));
    jeep=jeep_before;jeep.vehicle.player_occupied=0;CHECK(scene_jeep_checkpoint_stage_rigid(&current,&jeep,&staged)==RF_FORMAT&&!memcmp(&staged,&sentinel,sizeof(staged)));
    jeep=jeep_before;current.inverse_inertia[8]=0;CHECK(scene_jeep_checkpoint_stage_rigid(&current,&jeep,&staged)==RF_FORMAT&&!memcmp(&staged,&sentinel,sizeof(staged)));
    puts("PASS typed APC5000/Jeep400 pure staging, angular momentum, independent aim reference and rejection preservation");return 0;
}
