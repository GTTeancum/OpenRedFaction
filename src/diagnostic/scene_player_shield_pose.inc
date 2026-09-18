/* Camera-space first-person shield attachment seam, NOT a collision hull.
 * Installed weapons.tbl: fp_riotshield.vcm, FOV65, offset(.066,-.242,.154).
 * Compiled fp_riotshield.v3c has20bones: fk-shield is bone19, parent1;
 * resolve the NAME rather than depending on those observed indices.
 * The FP mesh also contains hand/arm vertices. Do not collide the whole mesh.
 * Renderer uses a separate FOV/depth band: this unprojected model-unit pose is
 * an attachment input, not proof of the static world shield's bind alignment.
 */
#include "rf/player_weapon.h"
#include <math.h>
#include <string.h>
static int scene_player_shield_bone_pose(const rf_player_weapon *weapon,
    const float model_camera[3],const float eye[3],const float eye_basis[9],
    rf_weapon_hand_placement *result)
{
    rf_model_bone_query bone;rf_weapon_hand_placement output={0};
    float local[12],world[12];uint32_t i;int32_t index=-1;int status;
    if(!weapon || !model_camera || !eye || !eye_basis || !result ||
       !weapon->initialized || !weapon->bone_count || weapon->bone_count>50)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(model_camera[i]) || !isfinite(eye[i]))return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(eye_basis[i]))return RF_RANGE;
    for(i=0;i<weapon->bone_count;++i){
        if(!memchr(weapon->bones[i].name,0,sizeof(weapon->bones[i].name)))return RF_FORMAT;
        if(!strcmp(weapon->bones[i].name,"fk-shield")){index=(int32_t)i;break;}
    }
    if(index<0)return RF_NOT_FOUND;
    /* pose is evaluated bone space; prepared is skinning (inverse bind applied)
     * and cannot substitute for an attachment transform. */
    status=rf_model_query_bone(weapon->pose,weapon->bone_count,index,&bone);if(status)return status;
    memcpy(local,bone.basis,36);
    for(i=0;i<3;++i){local[9+i]=bone.position[i]-model_camera[i];if(!isfinite(local[9+i]))return RF_RANGE;}
    status=rf_model_place_tag(local,eye_basis,eye,world);if(status)return status;
    memcpy(output.basis,world,36);memcpy(output.position,world+9,12);memcpy(output.hand,world+9,12);
    *result=output;return RF_OK;
}
