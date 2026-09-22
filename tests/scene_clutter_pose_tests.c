#include <stdio.h>
#include <string.h>
#include <float.h>
#include "../src/diagnostic/scene_clutter_pose.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"clutter pose line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_clutter_base_owner owner={0},before;float p[3]={10,20,30};
    float basis[9]={0,1,0,-1,0,0,0,0,1},bad[9];unsigned i;
    owner.uid=91;owner.state.handle=123;owner.state.health=70;owner.state.armor=12;
    owner.state.flags=0x204004;owner.state.physics_flags=0x88;owner.body.state.flags=0x40000020;
    owner.body.state.bounds.radius=2;owner.body.state.mass=15;owner.body.state.velocity[0]=4;
    owner.body.state.local_tensor[0]=1;owner.body.state.local_tensor[4]=2;owner.body.state.local_tensor[8]=3;
    CHECK(scene_clutter_pose_publish(&owner,p,basis,8)==RF_OK);
    CHECK(!memcmp(owner.state.position,p,12) && !memcmp(owner.query_position,p,12));
    CHECK(!memcmp(owner.body.state.position,p,12) && !memcmp(owner.body.state.next_position,p,12));
    CHECK(!memcmp(owner.matrix,basis,36) && !memcmp(owner.body.state.orientation,basis,36));
    CHECK(!memcmp(owner.body.state.next_orientation,basis,36));
    for(i=0;i<3;i++)CHECK(owner.body.state.bounds.minimum[i]==p[i]-2 && owner.body.state.bounds.maximum[i]==p[i]+2);
    CHECK(owner.body.state.world_tensor[0]==2 && owner.body.state.world_tensor[4]==1 && owner.body.state.world_tensor[8]==3);
    CHECK(owner.state.first_word==8 && owner.uid==91 && owner.state.handle==123);
    CHECK(owner.state.health==70 && owner.state.armor==12 && owner.state.flags==0x204004);
    CHECK(owner.state.physics_flags==0x88 && owner.body.state.flags==0x40000020 && owner.body.state.mass==15 && owner.body.state.velocity[0]==4);
    CHECK(scene_clutter_pose_publish(&owner,owner.state.position,owner.matrix,0)==RF_OK && !owner.state.first_word);
    before=owner;memcpy(bad,basis,36);bad[0]=.5f;
    CHECK(scene_clutter_pose_publish(&owner,p,bad,3)==RF_FORMAT && !memcmp(&owner,&before,sizeof(owner)));
    memcpy(bad,basis,36);for(i=0;i<3;i++)bad[i]=-bad[i];
    CHECK(scene_clutter_pose_publish(&owner,p,bad,3)==RF_FORMAT && !memcmp(&owner,&before,sizeof(owner)));
    memcpy(bad,basis,36);bad[8]=NAN;
    CHECK(scene_clutter_pose_publish(&owner,p,bad,3)==RF_FORMAT && !memcmp(&owner,&before,sizeof(owner)));
    p[0]=INFINITY;CHECK(scene_clutter_pose_publish(&owner,p,basis,3)==RF_FORMAT && !memcmp(&owner,&before,sizeof(owner)));
    p[0]=FLT_MAX;owner.body.state.bounds.radius=FLT_MAX;before=owner;
    CHECK(scene_clutter_pose_publish(&owner,p,basis,3)==RF_RANGE && !memcmp(&owner,&before,sizeof(owner)));
    puts("Clutter pose: translation, rotation, inertia, bounds, room, identity and atomic rejection passed");return 0;
}
