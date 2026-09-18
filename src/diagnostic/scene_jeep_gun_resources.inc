#ifndef SCENE_JEEP_GUN_RESOURCES_INC
#define SCENE_JEEP_GUN_RESOURCES_INC
#include "scene_driller_resources.inc"
/* Authored Jeep01 jeep_gun tag -> jeep_gun.v3m -> Muzzle_1/primary_1.
 * This is attachment/rest pose, not a recovered gunner yaw/pitch controller.
 * Jeep Gun has from_eye: physical muzzle does not replace its eye-ray policy. */
typedef struct scene_jeep_gun_resources {
    scene_driller_resources *model;rf_model_attachment mount;
    int32_t muzzle,primary;uint32_t resident_bytes,peak_bytes;
} scene_jeep_gun_resources;
static void scene_jeep_gun_resources_close(scene_jeep_gun_resources **out)
{
    scene_jeep_gun_resources *o;if(!out || !(o=*out))return;
    scene_driller_resources_close(&o->model);free(o);*out=NULL;
}
static int scene_jeep_gun_resources_open(rf_vpp *meshes,rf_vpp *maps,uint32_t map_count,
    const rf_static_model_tags *chassis,uint32_t budget,scene_jeep_gun_resources **out)
{
    scene_jeep_gun_resources *o=NULL;rf_model_file *file=NULL;uint32_t i,used=0,stage=0;int status;
    if(!meshes || !maps || !map_count || !chassis || !chassis->items || !out || *out ||
       budget<sizeof(*o)+sizeof(scene_driller_resources)+sizeof(*file))return RF_RANGE;
    for(i=0;i<chassis->count;i++)if(!strcmp(chassis->items[i].name,"jeep_gun"))break;
    if(i==chassis->count)return RF_NOT_FOUND;if(chassis->items[i].parent!=-1)return RF_FORMAT;
    o=calloc(1,sizeof(*o));if(!o)return RF_IO;o->mount=chassis->items[i];o->muzzle=o->primary=-1;
    o->model=calloc(1,sizeof(*o->model));file=malloc(sizeof(*file));if(!o->model || !file){status=RF_IO;goto fail;}
    o->model->seat=-1;strcpy(o->model->model,"jeep_gun.v3m");used=sizeof(*o)+sizeof(*o->model)+sizeof(*file);
    stage=1;status=rf_model_file_open(file,meshes,o->model->model);if(status)goto fail;
    stage=2;status=rf_static_render_resource_open(file,budget-used,&o->model->render);if(status)goto fail;used+=o->model->render.allocated_bytes;
    stage=3;status=rf_static_model_tags_open(file,budget-used,&o->model->tags);if(status)goto fail;used+=o->model->tags.allocated_bytes;
    for(i=0;i<o->model->tags.count;i++){
        const rf_model_attachment *tag=o->model->tags.items+i;
        if(!strcmp(tag->name,"Muzzle_1"))o->muzzle=(int32_t)i;
        if(!strcmp(tag->name,"primary_1"))o->primary=(int32_t)i;
    }
    if(o->muzzle<0 || o->primary<0){status=RF_NOT_FOUND;goto fail;}
    stage=4;status=rf_model_materials_open(&o->model->materials,file,maps,map_count,budget-used);if(status)goto fail;
    o->peak_bytes=used+o->model->materials.peak_bytes;o->resident_bytes=used-sizeof(*file)+o->model->materials.resident_bytes;
    free(file);*out=o;return RF_OK;
fail:fprintf(stderr,"JEEP_GUN_LOAD stage=%u status=%d used=%u budget=%u\n",stage,status,used,budget);free(file);scene_jeep_gun_resources_close(&o);return status;
}
/* Outputs12-float row-basis + position, error atomic. No added offsets or
 * substituted chassis eye/tag. Caller may draw model at returned gun pose. */
static int scene_jeep_gun_attachment_pose(const scene_jeep_gun_resources *o,
    const float host_position[3],const float host_basis[9],float gun[12],float muzzle[12],float primary[12])
{
    rf_static_model_tags mount;float g[12],m[12],p[12];int status;
    if(!o || !o->model || !host_position || !host_basis || !gun || !muzzle || !primary)return RF_RANGE;
    mount.items=(rf_model_attachment *)&o->mount;mount.count=1;mount.allocated_bytes=0;
    status=rf_static_model_tag_place(&mount,0,host_basis,host_position,g);if(status)return status;
    status=rf_static_model_tag_place(&o->model->tags,o->muzzle,g,g+9,m);if(status)return status;
    status=rf_static_model_tag_place(&o->model->tags,o->primary,g,g+9,p);if(status)return status;
    memcpy(gun,g,sizeof(g));memcpy(muzzle,m,sizeof(m));memcpy(primary,p,sizeof(p));return RF_OK;
}
#endif
