#include "../src/diagnostic/scene_player_shield_pose.inc"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
int main(void)
{
    static rf_player_weapon weapon;rf_weapon_hand_placement pose,saved;
    const float camera[3]={-.066f,.242f,-.154f},eye[3]={10,2,3};
    float basis[9]={1,0,0,0,1,0,0,0,1};
    weapon.initialized=1;weapon.bone_count=2;strcpy(weapon.bones[1].name,"fk-shield");
    weapon.pose[1][0]=weapon.pose[1][4]=weapon.pose[1][8]=1;
    weapon.pose[1][9]=.5f;weapon.pose[1][10]=-.5f;weapon.pose[1][11]=1;
    memset(weapon.prepared,0xa5,sizeof(weapon.prepared)); /* Must not use skinning matrices. */
    assert(scene_player_shield_bone_pose(&weapon,camera,eye,basis,&pose)==RF_OK);
    assert(fabsf(pose.position[0]-10.566f)<.00001f && fabsf(pose.position[1]-1.258f)<.00001f && fabsf(pose.position[2]-4.154f)<.00001f);
    memset(basis,0,sizeof(basis));basis[2]=-1;basis[4]=1;basis[6]=1;
    assert(scene_player_shield_bone_pose(&weapon,camera,eye,basis,&pose)==RF_OK);
    assert(fabsf(pose.position[0]-11.154f)<.00001f && fabsf(pose.position[2]-2.434f)<.00001f);
    saved=pose;weapon.bones[1].name[0]='x';
    assert(scene_player_shield_bone_pose(&weapon,camera,eye,basis,&pose)==RF_NOT_FOUND && !memcmp(&saved,&pose,sizeof(pose)));
    return 0;
}
