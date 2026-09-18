#define RF_SCENE_DRILLER_CHECKPOINT_MATH_ONLY
#include "../src/diagnostic/scene_driller_checkpoint_adapter.inc"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller checkpoint adapter line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vehicle_rigid_state live={0},saved;rf_vehicle_checkpoint record={0};
    rf_vehicle_rigid_parameters parameters={100,6,3,1.2f,2,9.8f};
    rf_vehicle_rigid_command command={0};rf_vehicle_rigid_support support={0};rf_vehicle_rigid_proposal proposal;
    scene_driller_checkpoint_candidate staged,sentinel;float inverse[9]={.5f,.1f,0,.1f,.4f,0,0,0,.25f},tensor[9],omega[3];uint32_t i;
    live.orientation[2]=-1;live.orientation[4]=1;live.orientation[6]=1;
    memcpy(live.inverse_inertia,inverse,sizeof(inverse));live.momentum[0]=2;live.momentum[1]=3;live.momentum[2]=-4;
    live.position[0]=30;live.position[1]=5.4f;live.position[2]=-167;live.velocity[0]=2;
    live.force[0]=99;live.torque[1]=88;live.skip_forces=1;saved=live;
    CHECK(!scene_driller_checkpoint_inertia(inverse,tensor));
    CHECK(!scene_driller_checkpoint_rotate_tensor(live.orientation,inverse,live.momentum,omega));
    CHECK(!rf_vehicle_rigid_propose(&live,&parameters,&command,&support,0,&proposal));
    for(i=0;i<3;i++)CHECK(fabsf(omega[i]-proposal.angular_velocity[i])<1e-6f);
    memcpy(record.position,live.position,12);memcpy(record.orientation,live.orientation,36);
    memcpy(record.velocity,live.velocity,12);memcpy(record.angular_velocity,omega,12);record.health=900;record.alive=record.player_occupied=1;
    record.accepted_drill_cuts=2;record.drill_spin=7;
    /* Restore from a differently oriented current host: use saved orientation. */
    memset(live.orientation,0,sizeof(live.orientation));live.orientation[0]=live.orientation[4]=live.orientation[8]=1;
    CHECK(!scene_driller_checkpoint_stage_rigid(&live,&record,&staged));
    for(i=0;i<3;i++)CHECK(fabsf(staged.physics.momentum[i]-saved.momentum[i])<1e-5f);
    CHECK(staged.health==900 && staged.alive && staged.player_occupied && staged.accepted_drill_cuts==2 && staged.drill_spin==7);
    CHECK(!memcmp(staged.physics.inverse_inertia,live.inverse_inertia,sizeof(inverse)) && !staged.physics.skip_forces);
    for(i=0;i<3;i++)CHECK(staged.physics.force[i]==0 && staged.physics.torque[i]==0);
    CHECK(live.force[0]==99 && live.torque[1]==88 && live.skip_forces==1);
    memset(&sentinel,0xa5,sizeof(sentinel));staged=sentinel;
    memset(live.inverse_inertia,0,sizeof(inverse));
    CHECK(scene_driller_checkpoint_stage_rigid(&live,&record,&staged)==RF_FORMAT && !memcmp(&staged,&sentinel,sizeof(staged)));
    memcpy(live.inverse_inertia,inverse,sizeof(inverse));record.orientation[0]=1;
    CHECK(scene_driller_checkpoint_stage_rigid(&live,&record,&staged)==RF_FORMAT && !memcmp(&staged,&sentinel,sizeof(staged)));
    puts("PASS Driller checkpoint inertia roundtrip, real rigid angular math, staged-only state, atomic rejection");return 0;
}
