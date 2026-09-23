#ifndef RF_SCENE_SCRIPT_EXPLODE_RESOURCES_INC
#define RF_SCENE_SCRIPT_EXPLODE_RESOURCES_INC
/* Scripted vclips keep their own bounded explosion recipe/material owner.
 * Bitmap tag 0x50000000 carries the resolved vclip index and texture slot. */
typedef struct scene_script_explode_effect_owner {
    char foley[64];rf_explosion_definition definition;rf_explosion_materials materials;
    struct {rf_explosion_clock clock;uint32_t slots[6];} instances[8];
} scene_script_explode_effect_owner;
static scene_script_explode_effect_owner *scene_script_explode_effects[64];
static uint32_t scene_script_explode_effect_count,scene_script_explode_effect_bytes;
uint32_t rf_scene_script_explode_visual[8]; /* starts,created,process,released,skipped,active,last clip,status */
static void scene_script_explode_effects_close(void)
{
    uint32_t i;
    for(i=0;i<64;i++)if(scene_script_explode_effects[i]){
        rf_explosion_materials_close(&scene_script_explode_effects[i]->materials);
        free(scene_script_explode_effects[i]);scene_script_explode_effects[i]=NULL;
    }
    scene_script_explode_effect_count=scene_script_explode_effect_bytes=0;
}
static int scene_script_explode_effects_open(rf_vpp *tables,rf_vpp *maps,uint32_t map_count,
    uint32_t index,const char *name)
{
    scene_script_explode_effect_owner *owner;rf_vclip_definition clip;int status;
    if(index>=64 || !name || !name[0])return RF_RANGE;
    if(scene_script_explode_effects[index])return RF_OK;
    if(scene_script_explode_effect_count>=4)return RF_RANGE;
    owner=calloc(1,sizeof(*owner));if(!owner)return RF_IO;
    status=rf_vclip_definition_load(tables,name,65536,&clip);
    if(!status && !clip.explosion[0])status=RF_NOT_FOUND;
    if(!status)status=rf_explosion_definition_load(tables,clip.explosion,256*1024,&owner->definition);
    if(!status && (owner->definition.recipe.central_count>6 ||
       owner->definition.recipe.central_random!=0))status=RF_FORMAT;
    if(!status){owner->definition.resolved&=(1u<<owner->definition.recipe.central_count)-1;
        status=rf_explosion_materials_open(&owner->materials,&owner->definition,maps,map_count,128*1024);}
    if(!status && scene_script_explode_effect_bytes+sizeof(*owner)+owner->materials.resident_bytes>512*1024)status=RF_RANGE;
    if(status){rf_explosion_materials_close(&owner->materials);free(owner);return status;}
    if(clip.has_foley)memcpy(owner->foley,clip.foley,sizeof(owner->foley));
    scene_script_explode_effects[index]=owner;++scene_script_explode_effect_count;
    scene_script_explode_effect_bytes+=(uint32_t)sizeof(*owner)+owner->materials.resident_bytes;
    return RF_OK;
}
static const rf_particle_animation *scene_script_explode_effects_image(uint32_t tag)
{
    uint32_t index=tag>>8,texture=tag&255u;
    if(index>=64 || !scene_script_explode_effects[index] ||
       texture>=scene_script_explode_effects[index]->materials.count)return NULL;
    return scene_script_explode_effects[index]->materials.animations+texture;
}
#endif
