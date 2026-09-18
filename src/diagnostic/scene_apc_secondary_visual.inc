#ifndef SCENE_APC_SECONDARY_VISUAL_INC
#define SCENE_APC_SECONDARY_VISUAL_INC
/* Include after scene_driller_draw.inc. Reuse its static geometry/material
 * ownership and world submission; no vehicle-class fields are populated.
 * Authored APC Rocket model Morter01.V3D resolves to Morter01.v3m. */
typedef scene_driller_resources scene_apc_secondary_visual;
static void scene_apc_secondary_visual_close(scene_apc_secondary_visual **owner)
{scene_driller_resources_close(owner);}
static int scene_apc_secondary_visual_open(rf_vpp *meshes,rf_vpp *maps,uint32_t map_count,
    uint32_t budget,scene_apc_secondary_visual **out)
{
    scene_apc_secondary_visual *owner=NULL;rf_model_file *file=NULL;uint32_t used;int status;
    if(!meshes || !maps || !map_count || !out || *out || budget<sizeof(*owner)+sizeof(*file))return RF_RANGE;
    owner=calloc(1,sizeof(*owner));file=malloc(sizeof(*file));if(!owner || !file){status=RF_IO;goto fail;}
    owner->seat=-1;used=sizeof(*owner)+sizeof(*file);
    status=rf_model_compiled_filename("Morter01.V3D",owner->model,".v3m");if(status)goto fail;
    status=rf_model_file_open(file,meshes,owner->model);if(status)goto fail;
    status=rf_static_render_resource_open(file,budget-used,&owner->render);if(status)goto fail;
    used+=owner->render.allocated_bytes;
    status=rf_model_materials_open(&owner->materials,file,maps,map_count,budget-used);if(status)goto fail;
    owner->peak_bytes=used+owner->materials.peak_bytes;
    owner->resident_bytes=used-sizeof(*file)+owner->materials.resident_bytes;
    free(file);*out=owner;return RF_OK;
fail:free(file);scene_apc_secondary_visual_close(&owner);return status;
}
static int scene_apc_secondary_visual_merge(scene_apc_secondary_visual *owner,rf_materials *scene,uint32_t *base,uint32_t *count)
{return scene_driller_materials_merge(owner,scene,base,count);}
/* Flight +Z follows actual velocity; world-up determines roll. This explicit
 * port presentation policy does not claim recovered projectile spin angles. */
static int scene_apc_secondary_visual_basis(const float velocity[3],float out[9])
{
    float basis[9]={0},up[3]={0,1,0},length=0,r;uint32_t i;
    if(!velocity || !out)return RF_RANGE;
    for(i=0;i<3;i++){if(!isfinite(velocity[i]))return RF_FORMAT;length+=velocity[i]*velocity[i];}
    if(!isfinite(length) || length<1e-12f)return RF_NOT_FOUND;
    length=sqrtf(length);for(i=0;i<3;i++)basis[6+i]=velocity[i]/length;
    if(fabsf(basis[7])>.999f){up[1]=0;up[2]=1;}
    basis[0]=up[1]*basis[8]-up[2]*basis[7];basis[1]=up[2]*basis[6]-up[0]*basis[8];basis[2]=up[0]*basis[7]-up[1]*basis[6];
    r=sqrtf(basis[0]*basis[0]+basis[1]*basis[1]+basis[2]*basis[2]);for(i=0;i<3;i++)basis[i]/=r;
    basis[3]=basis[7]*basis[2]-basis[8]*basis[1];basis[4]=basis[8]*basis[0]-basis[6]*basis[2];basis[5]=basis[6]*basis[1]-basis[7]*basis[0];
    memcpy(out,basis,sizeof(basis));return RF_OK;
}
static int scene_apc_secondary_visual_draw(const scene_apc_secondary_visual *owner,scene_stream *stream,
    const float position[3],const float velocity[3],uint32_t base,uint32_t textures,uint32_t *submissions,uint32_t *vertices)
{
    float basis[9];int status=scene_apc_secondary_visual_basis(velocity,basis);if(status)return status;
    return scene_driller_draw(stream,owner,position,basis,base,textures,submissions,vertices);
}
#endif
