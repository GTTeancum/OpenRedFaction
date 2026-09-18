#ifndef SCENE_DRILLER_BIT_ANIMATION_INC
#define SCENE_DRILLER_BIT_ANIMATION_INC
#include <math.h>
/* Include after scene_driller_resources.inc. Installed Drill explicitly uses
 * Driller01_Drillbit.v3d as its third-person model; the chassis is one immutable
 * SUBM, so rotating a chassis material/vertex subset would be incorrect.
 * Parent feeds scene_driller_weapon.phase (already integrates its spin speed).
 * First-pass visual policy: both independent bits rotate about their authored
 * attachment's local +Z axis. Exact retail handedness is not claimed. */
typedef struct scene_driller_bit_animation {
    scene_driller_resources model;int32_t tags[2];
    uint32_t resident_bytes,peak_bytes;
} scene_driller_bit_animation;
static void scene_driller_bit_animation_close(scene_driller_bit_animation **owner)
{
    scene_driller_bit_animation *o;if(!owner || !(o=*owner))return;
    rf_model_materials_close(&o->model.materials);rf_static_render_resource_close(&o->model.render);
    free(o);*owner=NULL;
}
static int scene_driller_bit_animation_open(const rf_static_model_tags *tags,rf_vpp *meshes,
    rf_vpp *maps,uint32_t map_count,uint32_t budget,scene_driller_bit_animation **out)
{
    scene_driller_bit_animation *o;rf_model_file *file=NULL;uint32_t i,used;int status;
    if(!tags || !tags->items || !meshes || !maps || !map_count || !out || *out || budget<sizeof(*o)+sizeof(*file))return RF_RANGE;
    o=calloc(1,sizeof(*o));if(!o)return RF_IO;o->tags[0]=o->tags[1]=-1;used=sizeof(*o);
    for(i=0;i<tags->count;++i){
        if(!strcmp(tags->items[i].name,"primary_weapon_1"))o->tags[0]=(int32_t)i;
        if(!strcmp(tags->items[i].name,"primary_weapon_2"))o->tags[1]=(int32_t)i;
    }
    if(o->tags[0]<0 || o->tags[1]<0){status=RF_NOT_FOUND;goto fail;}
    strcpy(o->model.model,"Driller01_Drillbit.v3m");
    file=malloc(sizeof(*file));if(!file){status=RF_IO;goto fail;}used+=sizeof(*file);
    status=rf_model_file_open(file,meshes,o->model.model);if(status)goto fail;
    status=rf_static_render_resource_open(file,budget-used,&o->model.render);if(status)goto fail;
    used+=o->model.render.allocated_bytes;
    status=rf_model_materials_open(&o->model.materials,file,maps,map_count,budget-used);if(status)goto fail;
    o->peak_bytes=used+o->model.materials.peak_bytes;
    o->resident_bytes=used-sizeof(*file)+o->model.materials.resident_bytes;
    o->model.resident_bytes=o->resident_bytes;o->model.peak_bytes=o->peak_bytes;
    free(file);*out=o;return RF_OK;
fail:free(file);scene_driller_bit_animation_close(&o);return status;
}
/* Result format matches scene_driller_seat_pose: basis9, then origin3. Tag
 * position is the true rotation pivot; immutable geometry/normals stay local,
 * so both retained and software rendering share the same rigid transform. */
static int scene_driller_bit_animation_pose(const scene_driller_bit_animation *owner,
    const rf_static_model_tags *tags,const float position[3],const float basis[9],
    uint32_t bit,float phase,float out[12])
{
    float attachment[12],rotation[12]={0},value[12],angle;int status;
    if(!owner || !tags || !position || !basis || !out || bit>=2 || !isfinite(phase))return RF_RANGE;
    status=rf_static_model_tag_place(tags,owner->tags[bit],basis,position,attachment);if(status)return status;
    angle=fmodf(phase,6.2831853071795864769f);
    rotation[0]=rotation[4]=cosf(angle);rotation[1]=sinf(angle);rotation[3]=-rotation[1];rotation[8]=1;
    status=rf_model_compose_transform(rotation,attachment,value);if(status)return status;
    memcpy(out,value,sizeof(value));return RF_OK;
}
#endif
