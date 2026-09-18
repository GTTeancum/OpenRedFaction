#ifndef RF_SCENE_FUSION_EFFECTS_RESOURCES_INC
#define RF_SCENE_FUSION_EFFECTS_RESOURCES_INC
/* Include after scene_stream definition, before particle bitmap resolver.
 * Singleton follows current scene ownership. All resources bounded and closed
 * during scene teardown, after particle users cease. */
typedef struct scene_fusion_effects_owner {
    rf_explosion_definition definition;rf_explosion_materials materials;
    struct {rf_explosion_clock clock;uint32_t slots[3];} instances[4];
} scene_fusion_effects_owner;
static scene_fusion_effects_owner *scene_fusion_effects;
static void scene_fusion_effects_close(void)
{
    if(!scene_fusion_effects)return;
    rf_explosion_materials_close(&scene_fusion_effects->materials);
    free(scene_fusion_effects);scene_fusion_effects=NULL;
}
static int scene_fusion_effects_open(const char *tables_path,rf_vpp *maps,uint32_t map_count)
{
    rf_vpp tables={0};rf_vclip_definition clip;scene_fusion_effects_owner *owner;int status;
    if(scene_fusion_effects)return RF_RANGE;
    owner=calloc(1,sizeof(*owner));if(!owner)return RF_IO;
    status=rf_vpp_open(&tables,tables_path);
    if(!status)status=rf_vclip_definition_load(&tables,"shoulder_cannon_explode",65536,&clip);
    if(!status)status=rf_explosion_definition_load(&tables,clip.explosion,256*1024,&owner->definition);
    rf_vpp_close(&tables);
    if(status)printf("FUSION_EFFECT_DEFINITION_ERROR %d\n",status);
    if(!status && (owner->definition.recipe.central_count!=3 || owner->definition.recipe.central_random!=0))status=RF_FORMAT;
    /* Only central emitters are submitted here; do not load unused trail textures. */
    owner->definition.resolved&=(1u<<owner->definition.recipe.central_count)-1;
    /* Shared loader resolves/deduplicates the named emitter bitmap resources.
     * 512KiB material ceiling, in addition to the small owner/metadata. */
    if(!status)status=rf_explosion_materials_open(&owner->materials,&owner->definition,maps,map_count,512*1024);
    if(status)printf("FUSION_EFFECT_MATERIAL_ERROR %d\n",status);
    else printf("FUSION_EFFECT_MATERIALS %u %u\n",owner->materials.count,owner->materials.resident_bytes);
    if(status){rf_explosion_materials_close(&owner->materials);free(owner);return status;}
    scene_fusion_effects=owner;return RF_OK;
}
/* Index relative to first fusion texture, after normal particles, rocket,
 * blood and any loaded flame effect textures. */
static const rf_particle_animation *scene_fusion_effects_image(uint32_t index)
{
    if(!scene_fusion_effects || index>=scene_fusion_effects->materials.count)return NULL;
    return scene_fusion_effects->materials.animations+index;
}
#endif
