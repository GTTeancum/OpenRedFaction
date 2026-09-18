#ifndef SCENE_JEEP_GUN_AIM_INC
#define SCENE_JEEP_GUN_AIM_INC
#include "scene_jeep_gun_resources.inc"
#include <math.h>
/* Explicit first-pass articulation: projected authored muzzle-up determines
 * roll, with authored muzzle-right fallback when looking parallel to up.
 * No recovered yaw/pitch limits or independent gunner input are claimed. */
static int scene_jeep_gun_aim_normalize(float v[3])
{
    float n=0;uint32_t i;for(i=0;i<3;i++){if(!isfinite(v[i]))return RF_FORMAT;n+=v[i]*v[i];}
    if(!isfinite(n) || n<1e-12f)return RF_FORMAT;n=sqrtf(n);for(i=0;i<3;i++)v[i]/=n;return RF_OK;
}
static void scene_jeep_gun_aim_cross(const float a[3],const float b[3],float out[3])
{out[0]=a[1]*b[2]-a[2]*b[1];out[1]=a[2]*b[0]-a[0]*b[2];out[2]=a[0]*b[1]-a[1]*b[0];}
static int scene_jeep_gun_aim_pose(const scene_jeep_gun_resources *owner,const float host_position[3],
    const float host_basis[9],const float desired_forward[3],float gun[12],float muzzle[12])
{
    float rest[12],baseline[12],primary[12],target[9],delta[9],g[12],m[12],dot=0,n=0;
    uint32_t i,j,k;int status;
    if(!owner || !host_position || !host_basis || !desired_forward || !gun || !muzzle)return RF_RANGE;
    status=scene_jeep_gun_attachment_pose(owner,host_position,host_basis,rest,baseline,primary);if(status)return status;
    memcpy(target+6,desired_forward,12);status=scene_jeep_gun_aim_normalize(target+6);if(status)return status;
    for(i=0;i<3;i++)dot+=baseline[3+i]*target[6+i];
    for(i=0;i<3;i++){target[3+i]=baseline[3+i]-dot*target[6+i];n+=target[3+i]*target[3+i];}
    if(n<1e-8f){
        dot=0;for(i=0;i<3;i++)dot+=baseline[i]*target[6+i];
        for(i=0;i<3;i++)target[i]=baseline[i]-dot*target[6+i];
        status=scene_jeep_gun_aim_normalize(target);if(status)return status;
        scene_jeep_gun_aim_cross(target+6,target,target+3);
    }else{
        status=scene_jeep_gun_aim_normalize(target+3);if(status)return status;
        scene_jeep_gun_aim_cross(target+3,target+6,target);
        status=scene_jeep_gun_aim_normalize(target);if(status)return status;
        scene_jeep_gun_aim_cross(target+6,target,target+3);
    }
    /* Row-vector convention: baseline * delta = target. Apply the same
     * world delta to the complete authored gun basis, retaining its mount. */
    for(i=0;i<3;i++)for(j=0;j<3;j++){
        delta[i*3+j]=0;for(k=0;k<3;k++)delta[i*3+j]+=baseline[k*3+i]*target[k*3+j];
    }
    memcpy(g+9,rest+9,12);
    for(i=0;i<3;i++)for(j=0;j<3;j++){
        g[i*3+j]=0;for(k=0;k<3;k++)g[i*3+j]+=rest[i*3+k]*delta[k*3+j];
    }
    status=rf_static_model_tag_place(&owner->model->tags,owner->muzzle,g,g+9,m);if(status)return status;
    memcpy(gun,g,sizeof(g));memcpy(muzzle,m,sizeof(m));return RF_OK;
}
#endif
